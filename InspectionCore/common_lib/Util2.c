#include "common_lib.h"
#include "logctrl.h"
#include <stdexcept>
#include <stdlib.h>
#include <compat_dirent.h>

#include <lodepng.h>

#include <limits.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>



bool isDirExist(const char* dir_path)
{
  DIR* dir = opendir(dir_path);
  if (dir) {
      /* Directory exists. */
      closedir(dir);
      return true;
  } else if (ENOENT == errno) {
      /* Directory does not exist. */
  } else {
      /* opendir() failed for some other reason. */
  }
  return false;
}

int Save2PNG(uint8_t *data, int width, int height, int channelCount, const char *filePath)
{
  // we're going to encode with a state rather than a convenient function, because enforcing a color type requires setting options
  lodepng::State state;
  // input color type
  state.info_raw.colortype = LCT_GREY;
  switch (channelCount)
  {
  case 1:
    state.info_raw.colortype = LCT_GREY;
    break;
  case 2:
    state.info_raw.colortype = LCT_GREY_ALPHA;
    break; //Weird but what ever
  case 3:
    state.info_raw.colortype = LCT_RGB;
    break;
  case 4:
    state.info_raw.colortype = LCT_RGBA;
    break;
  default:
    return -1;
  }
  state.info_raw.bitdepth = 8;
  // output color type
  state.info_png.color.colortype = LCT_RGB;
  state.info_png.color.bitdepth = 8;
  state.encoder.auto_convert = 1; // without this, it would ignore the output color type specified above and choose an optimal one instead

  std::vector<unsigned char> buffer;
  unsigned error = lodepng::encode(buffer, data, width, height, state);
  if (error)
  {
    LOGI("encoder error %d : %s", error, lodepng_error_text(error));
    return error;
  }

  return lodepng::save_file(buffer, filePath);
}

int LoadPNGFile(acvImage *img, const char *filename)
{

  std::vector<unsigned char> image;
  unsigned ret_width, ret_height;
  image.resize(0);

  unsigned error = lodepng::decode(image, ret_width, ret_height, filename, LCT_RGB, 8);
  if (error != 0)
    return error;
  img->ReSize(ret_width, ret_height);

  for (int i = 0; i < img->GetHeight(); i++)
  {

    for (int j = 0; j < img->GetWidth(); j++)
    {
      img->CVector[i][j * 3 + 0] = image[i * ret_width * 3 + j * 3 + 2];
      img->CVector[i][j * 3 + 1] = image[i * ret_width * 3 + j * 3 + 1];
      img->CVector[i][j * 3 + 2] = image[i * ret_width * 3 + j * 3 + 0]; //The order is reversed
    }
    //memcpy(img->CVector[i],&(image[i*ret_width*3]),ret_width*3);
  }

  return 0;
}

int LoadIMGFile(acvImage *ret_img, const char *filename)
{
  const char *dot = strrchr(filename, '.');
  std::string fname_str(filename);

  int retVal;
  if (dot)
  {
    if (strcmp(dot, ".bmp") == 0 || strcmp(dot, ".BMP") == 0)
    {
      return acvLoadBitmapFile(ret_img, filename);
    }
    if (strcmp(dot, ".png") == 0 || strcmp(dot, ".PNG") == 0)
    {
      return LoadPNGFile(ret_img, filename);
    }
  }

  retVal = LoadPNGFile(ret_img, (fname_str + ".png").c_str());
  if (retVal == 0)
    return 0;
  retVal = acvLoadBitmapFile(ret_img, (fname_str + ".bmp").c_str());
  if (retVal == 0)
    return 0;

  return -1;
}

int SavePNGFile_(const char *filename, acvImage *img)
{
  LOGE("SavePNGFile:%s", filename);
  int w = img->GetWidth();
  std::vector<uint8_t> pix_arr(img->GetHeight() * img->GetWidth() * 3);
  for (int i = 0; i < img->GetHeight(); i++)
  {

    for (int j = 0; j < w; j++)
    {
      pix_arr[i * w * 3 + j * 3 + 2] = img->CVector[i][j * 3 + 0];
      pix_arr[i * w * 3 + j * 3 + 1] = img->CVector[i][j * 3 + 1];
      pix_arr[i * w * 3 + j * 3 + 0] = img->CVector[i][j * 3 + 2];
    }
    //memcpy(img->CVector[i],&(image[i*ret_width*3]),ret_width*3);
  }

  return Save2PNG(&(pix_arr[0]), img->GetWidth(), img->GetHeight(), 3, filename);
}
int SavePNGFile(const char *filename, acvImage *img)
{
  LOGE("SavePNGFile:%s", filename);
  int w = img->GetWidth();

  return Save2PNG(img->CVector[0], img->GetWidth(), img->GetHeight(), 3, filename);
}

int SaveIMGFile(const char *filename, acvImage *img)
{
  const char *dot = strrchr(filename, '.');

  LOGE("SaveIMGFile:%s", filename);
  int retVal;
  if (!dot)
  {
    std::string fname_str(filename);
    return SavePNGFile((fname_str + ".png").c_str(), img);
  }

  if (strcmp(dot, ".bmp") == 0)
  {
    return acvSaveBitmapFile(filename, img);
  }
  if (strcmp(dot, ".png") == 0)
  {
    return SavePNGFile(filename, img);
  }

  {

    std::string fname_str(filename);
    return SavePNGFile((fname_str + ".png").c_str(), img);
  }

  return -1;
}

void realfullPath(const char *curPath, char *ret_fullPath)
{
#ifdef _WIN32
  _fullpath(ret_fullPath, curPath, PATH_MAX);
#else
  realpath(curPath, ret_fullPath);
#endif
}

int cross_mkdir(const char *path)
{
#ifdef _WIN32
  return mkdir(path);
#else
  return mkdir(path, 0777);
#endif
}
