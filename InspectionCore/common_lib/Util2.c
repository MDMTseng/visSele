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
#include <string>
#include <memory>
#include "mjpegLib.h"

#ifdef _WIN32
#include<windows.h>
#endif
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


uint8_t* _buffer_request_callback(int W,int H,int channel,void* cb_param)
{
  acvImage *img=(acvImage *)cb_param;
  img->ReSize(W, H);
  return img->CVector[0];
}



int LoadJPEGFile(acvImage *img, const char *filename)
{
  return jpecLib_dec(filename ,_buffer_request_callback,(void*)img);
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
    if (strcmp(dot, ".jpeg") == 0 || strcmp(dot, ".jpg") == 0)
    {
      return LoadPNGFile(ret_img, filename);
    }
  }

  retVal = LoadPNGFile(ret_img, (fname_str + ".png").c_str());
  if (retVal == 0)
    return 0;
  retVal = LoadJPEGFile(ret_img, (fname_str + ".jpg").c_str());
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



struct JPEGSaveConfig
{
  const char* fileName;
  int errorCode;
};

void jpeg_encoder_buffer_callback(const uint8_t *rawbuffer,int size, void*cb_param)
{

  struct JPEGSaveConfig *conf=(struct JPEGSaveConfig *)cb_param;
  FILE *hs = fopen(conf->fileName,"w");
  if( hs == NULL)
  {
      // fprintf(stderr,"Error writing to %s\n",filename);
      conf->errorCode=-1;
      return;
  }
  fwrite(rawbuffer, size, 1, hs);
  fclose(hs);

}




int SaveJPEGFile(const char *filename, acvImage *img, int quality)
{
  // LOGE("SaveJPEGFile:%s", filename);
  // struct JPEGSaveConfig conf={fileName:filename,errorCode:0};
  // int ret = mjpecLib_enc(img->CVector[0],img->GetWidth(), img->GetHeight(), quality,jpeg_encoder_buffer_callback, &conf);
  // if(ret==0)
  // {
  //   ret=conf.errorCode;
  // }


  return mjpecLib_enc(filename, img->CVector[0],img->GetWidth(), img->GetHeight(),quality);
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

  if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".JPG") == 0|| strcmp(dot, ".jpeg") == 0)
  {
    return SaveJPEGFile(filename, img, 90);
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



char **str_split(const char *in, size_t in_len, char delm, size_t *num_elm, size_t max)
{
    char   *parsestr;
    char   **out;
    size_t  cnt = 1;
    size_t  i;

    if (in == NULL || in_len == 0 || num_elm == NULL)
        return NULL;

    parsestr = (char   *)malloc(in_len+1);
    memcpy(parsestr, in, in_len+1);
    parsestr[in_len] = '\0';

    *num_elm = 1;
    for (i=0; i<in_len; i++) {
        if (parsestr[i] == delm)
            (*num_elm)++;
        if (max > 0 && *num_elm == max)
            break;
    }

    out    = (char   **)malloc(*num_elm * sizeof(*out));
    out[0] = parsestr;
    for (i=0; i<in_len && cnt<*num_elm; i++) {
        if (parsestr[i] != delm)
            continue;

        /* Add the pointer to the array of elements */
        parsestr[i] = '\0';
        out[cnt] = parsestr+i+1;
        cnt++;
    }

    return out;
}
void str_split_free(char **in, size_t num_elm)
{
    if (in == NULL)
        return;
    if (num_elm != 0)
        free(in[0]);
    free(in);
}

int cross_mkdir(const char *path)
{
#ifdef _WIN32
  return mkdir(path);
#else
  return mkdir(path, 0777);
#endif
}


char systemPathSEP()
{
#ifdef _WIN32
  return '\\';
#else
  return '/';
#endif
}

bool rw_create_dir(const char *name)
{
  const char SEP =systemPathSEP();
    char strBuffer[300]={0};
    char *strPtr=strBuffer;
    char          **parts;
    size_t          num_parts;
    size_t          i;
    bool            ret = true;

    if (name == NULL || *name == '\0')
        return false;

    parts = str_split(name, strlen(name), SEP, &num_parts, 0);
    if (parts == NULL || num_parts == 0) {
        str_split_free(parts, num_parts);
        return false;
    }
    i  = 0;
#ifdef _WIN32
    /* If the first part has a ':' it's a drive. E.g 'C:'. We don't
     * want to try creating it because we can't. We'll add it to base
     * and move forward. The next part will be a directory we need
     * to try creating. */
    if (strchr(parts[0], ':')) {
        i++;
        strPtr+=sprintf(strPtr,"%s%c",parts[0],SEP);
    }
#else
    if (*name == '/') {
        strPtr+=sprintf(strPtr,"%c",SEP);
    }
#endif

    for ( ; i<num_parts; i++) {
        if (parts[i] == NULL || *(parts[i]) == '\0') {
            continue;
        }

        strPtr+=sprintf(strPtr,"%s%c",parts[i],SEP);

#ifdef _WIN32

        if (CreateDirectory(strBuffer, NULL) == FALSE) {
            if (GetLastError() != ERROR_ALREADY_EXISTS) {
                ret = false;
                goto done;
            }
        }
#else
        if (mkdir(strBuffer, 0774) != 0)
            if (errno != EEXIST) {
                ret = false;
                goto done;
            }
#endif
    }

done:
    str_split_free(parts, num_parts);
    return ret;
}








std::string run_exe(const char* cmd) {
    char buffer[128];
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer, sizeof(buffer)-1, pipe.get()) != nullptr) {
        result += buffer;
    }
    return result;
}