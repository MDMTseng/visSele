#include <opencv2/opencv.hpp>
#include <mutex>
#include <vector>
#include <cstdio>
#include <cstdint>
class ImageStackAddUp
{
  // NOT recursive, and it must not need to be: every public method takes this
  // ONCE and then works through the _-prefixed helpers, which assume it is
  // already held. The helpers used to take it too and the public methods
  // called them (ReSize -> Reset, Add -> set_1CH/addUp_1CH, Export() ->
  // Export(out)), which is a self-deadlock on a plain std::mutex: the thread
  // stops forever holding a lock nothing can release.
  std::mutex lock;

  // The accumulator is a plain 32-bit integer sum, one channel.
  //
  // It used to be a CV_8UC3 buffer read through _24BitUnion -- three bytes per
  // pixel reinterpreted as a 24-bit counter -- and BOTH ends of that were
  // wrong. The source was indexed `sRow[j * 3]`, so a single-channel frame was
  // read three times past the end of every row; and `_3BYTE { unsigned Num :
  // 24; }` has sizeof 4, so writing the last pixel of the last row wrote a
  // byte past the buffer. It never showed because the deadlock above meant the
  // loops had literally never run; the first frame that reached them was an
  // access violation (SIGSEGV in _set_1CH, 2026-09-22).
  //
  // A CV_32SC1 sum cannot overflow at these counts (100 frames x 255) and lets
  // OpenCV do the adding.
  cv::Mat accum;        // CV_32SC1, the running sum
  cv::Mat scratch;      // CV_8UC1 view of the last frame handed in
  int     srcChannels = 1;  // what Add was given, so Export gives it back

  // One channel of 8-bit gray out of whatever the pipeline handed us. The
  // frames here are mono carried in however many channels the camera layer
  // happens to use, so channel 0 IS the picture.
  const cv::Mat &_gray(const cv::Mat &in)
  {
    if (in.channels() == 1) { return in; }
    cv::extractChannel(in, scratch, 0);
    return scratch;
  }

public:
  int stackingC = 0;
  // Kept for the callers that ask "is there anything in here, and what shape".
  // It is the ACCUMULATOR, not a picture: read it through Export().
  cv::Mat imgStacked;
  cv::Mat imgExtract;

  void clear()
  {
    std::lock_guard<std::mutex> guard(lock);
    if (!accum.empty()) accum.setTo(cv::Scalar(0));
    stackingC = 0;
  }

  void ReSize(const cv::Mat &ref)
  {
    std::lock_guard<std::mutex> guard(lock);
    accum.create(ref.rows, ref.cols, CV_32SC1);
    accum.setTo(cv::Scalar(0));
    imgStacked = accum;                 // same geometry, for the size checks
    srcChannels = ref.channels() < 1 ? 1 : ref.channels();
    stackingC = 0;
  }

  void Reset()
  {
    std::lock_guard<std::mutex> guard(lock);
    stackingC = 0;
  }

  void Add(const cv::Mat &in)
  {
    std::lock_guard<std::mutex> guard(lock);
    if (in.empty()) return;
    // Sized here rather than trusting every caller: an Add against an
    // accumulator of another shape is what walks off the end of a buffer.
    if (accum.rows != in.rows || accum.cols != in.cols || accum.type() != CV_32SC1)
    {
      accum.create(in.rows, in.cols, CV_32SC1);
      accum.setTo(cv::Scalar(0));
      imgStacked = accum;
      stackingC = 0;
    }
    srcChannels = in.channels() < 1 ? 1 : in.channels();
    if (stackingC >= 100) return;       // the counter's ceiling, as before

    const cv::Mat &g = _gray(in);
    if (stackingC == 0) { g.convertTo(accum, CV_32SC1); }
    else                { cv::add(accum, g, accum, cv::noArray(), CV_32SC1); }
    stackingC++;
    imgStacked = accum;
  }

  // The mean, back in the shape it was given. Empty if nothing was added --
  // the caller must check, and the SI path does.
  void Export(cv::Mat &out)
  {
    std::lock_guard<std::mutex> guard(lock);
    _export(out);
  }

  void Export()
  {
    std::lock_guard<std::mutex> guard(lock);
    _export(imgExtract);
    // Kept because callers read imgExtract directly.
  }

  bool DiffBigger(const cv::Mat &img2, float globalDiffThres, int localDiffThres, int skipSampling = 10)
  {
    std::lock_guard<std::mutex> guard(lock);
    if (skipSampling < 1) skipSampling = 1;
    // Nothing to compare against, or two pictures of different shapes: there
    // is no answer, and "changed" would fail every attempt.
    if (accum.empty() || stackingC <= 0 || img2.empty() ||
        accum.rows != img2.rows || accum.cols != img2.cols)
      return false;

    const cv::Mat &g = _gray(img2);

    const double globalLimit =
      (double)globalDiffThres * globalDiffThres *
      ((double)accum.rows * accum.cols / skipSampling / skipSampling);
    const int localLimit = localDiffThres * localDiffThres;

    double diffSum = 0; int diffMax = 0;
    for (int i = 0; i < accum.rows; i += skipSampling)
    {
      const int32_t *aRow = accum.ptr<int32_t>(i);
      const uchar   *sRow = g.ptr<uchar>(i);
      for (int j = 0; j < accum.cols; j += skipSampling)
      {
        const int mean = aRow[j] / stackingC;
        int diff = mean - (int)sRow[j];
        diff *= diff;
        diffSum += diff;
        if (diffSum > globalLimit) return true;
        if (diffMax < diff)
        {
          diffMax = diff;
          if (diffMax > localLimit) return true;
        }
      }
    }
    return false;
  }

private:
  void _export(cv::Mat &out)
  {
    if (accum.empty() || stackingC <= 0) { out.release(); return; }
    cv::Mat mean8;
    accum.convertTo(mean8, CV_8UC1, 1.0 / stackingC);
    if (srcChannels <= 1) { out = mean8; return; }
    std::vector<cv::Mat> ch((size_t)srcChannels, mean8);
    cv::merge(ch, out);
  }
};


static int fails = 0;
#define CHECK(c, msg) do { if(!(c)){ printf("FAIL: %s\n", msg); fails++; } } while(0)

static void roundTrip(int type, const char *name)
{
  ImageStackAddUp st;
  cv::Mat a(37, 53, type, cv::Scalar::all(10));
  cv::Mat b(37, 53, type, cv::Scalar::all(20));
  st.ReSize(a);
  st.Add(a); st.Add(b); st.Add(a); st.Add(b);   // mean 15
  cv::Mat out; st.Export(out);
  CHECK(!out.empty(), name);
  CHECK(out.channels() == CV_MAT_CN(type), name);
  CHECK(out.rows == 37 && out.cols == 53, name);
  CHECK(out.depth() == CV_8U, name);
  cv::Mat ch0; if(out.channels()==1) ch0=out; else cv::extractChannel(out, ch0, 0);
  double mn, mx; cv::minMaxLoc(ch0, &mn, &mx);
  CHECK(mn == 15 && mx == 15, name);
  printf("  %-10s mean=%g..%g ch=%d stackingC=%d\n", name, mn, mx, out.channels(), st.stackingC);
}

int main()
{
  printf("round trip\n");
  roundTrip(CV_8UC1, "mono");
  roundTrip(CV_8UC3, "bgr");

  printf("empty export\n");
  { ImageStackAddUp st; cv::Mat o; st.Export(o); CHECK(o.empty(), "empty export"); }

  printf("diff\n");
  {
    ImageStackAddUp st;
    cv::Mat a(200, 300, CV_8UC1, cv::Scalar::all(100));
    CHECK(st.DiffBigger(a, 6.0f, 40, 10) == false, "diff on empty");
    st.Add(a);
    CHECK(st.DiffBigger(a, 6.0f, 40, 10) == false, "identical frames differ");
    cv::Mat b = a.clone(); b.setTo(cv::Scalar::all(200));
    CHECK(st.DiffBigger(b, 6.0f, 40, 10) == true, "big change not seen");
    cv::Mat wrong(100, 100, CV_8UC1, cv::Scalar::all(100));
    CHECK(st.DiffBigger(wrong, 6.0f, 40, 10) == false, "mismatched size");
    cv::Mat c3(200, 300, CV_8UC3, cv::Scalar::all(100));
    CHECK(st.DiffBigger(c3, 6.0f, 40, 10) == false, "3ch against 1ch accum");
  }

  printf("size change mid-stack\n");
  {
    ImageStackAddUp st;
    st.Add(cv::Mat(50, 50, CV_8UC1, cv::Scalar::all(30)));
    st.Add(cv::Mat(80, 90, CV_8UC3, cv::Scalar::all(60)));   // must resize, not crash
    cv::Mat o; st.Export(o);
    CHECK(o.rows == 80 && o.cols == 90 && o.channels() == 3, "resize mid-stack");
    CHECK(st.stackingC == 1, "counter reset on resize");
  }

  printf("ceiling\n");
  {
    ImageStackAddUp st;
    cv::Mat a(20, 20, CV_8UC1, cv::Scalar::all(8));
    for (int i = 0; i < 250; i++) st.Add(a);
    CHECK(st.stackingC == 100, "ceiling at 100");
    cv::Mat o; st.Export(o); double mn,mx; cv::minMaxLoc(o,&mn,&mx);
    CHECK(mn == 8 && mx == 8, "mean after ceiling");
  }

  printf("odd width, last pixel\n");
  {
    ImageStackAddUp st;
    cv::Mat a(1, 1, CV_8UC1, cv::Scalar::all(255));
    st.Add(a); st.Add(a);
    cv::Mat o; st.Export(o);
    CHECK(!o.empty() && o.at<uchar>(0,0) == 255, "1x1");
    cv::Mat big(1943, 2591, CV_8UC3, cv::Scalar::all(255));   // odd dims, last row/col
    ImageStackAddUp st2; st2.Add(big); st2.Add(big);
    cv::Mat o2; st2.Export(o2);
    CHECK(!o2.empty(), "odd big");
  }

  printf("%s (%d failure(s))\n", fails ? "FAILED" : "all ok", fails);
  return fails ? 1 : 0;
}
