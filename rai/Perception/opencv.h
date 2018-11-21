/*  ------------------------------------------------------------------
    Copyright (c) 2017 Marc Toussaint
    email: marc.toussaint@informatik.uni-stuttgart.de

    This code is distributed under the MIT License.
    Please see <root-path>/LICENSE for details.
    --------------------------------------------------------------  */

#ifndef RAI_opencv_h
#define RAI_opencv_h

//OpenCV (C++) wrappers

#include <Core/array.h>
#include <Core/util.h>

#ifdef RAI_OPENCV

#undef COUNT
//#include <opencv/highgui.h>
#include <opencv2/opencv.hpp>
#undef MIN
#undef MAX

extern ::Mutex cvMutex;

inline cv::Mat conv_Arr2CvRef(const byteA& img) {
  if(img.nd==2) return cv::Mat(img.d0, img.d1, CV_8UC1, img.p);
  if(img.nd==3) return cv::Mat(img.d0, img.d1, CV_8UC3, img.p);
  return cv::Mat();
}

inline cv::Mat conv_Arr2CvRef(const floatA& img) {
  if(img.nd==1) return cv::Mat(img.d0, 1, CV_32FC3, img.p);
  if(img.nd==2) return cv::Mat(img.d0, img.d1, CV_32FC1, img.p);
  if(img.nd==3) return cv::Mat(img.d0, img.d1, CV_32FC3, img.p);
  return cv::Mat();
}

inline cv::Mat conv_Arr2CvRef(const doubleA& img) {
  if(img.nd==2) return cv::Mat(img.d0, img.d1, CV_64FC1, img.p);
  if(img.nd==3) return cv::Mat(img.d0, img.d1, CV_64FC3, img.p);
  return cv::Mat();
}

inline byteA cvtMAT(const cv::Mat& mat) {
  CHECK_EQ(mat.dims,2,"");
  if(mat.elemSize()==1) return byteA(mat.data, mat.total());
  if(mat.elemSize()==3) return byteA(mat.data, 3*mat.total()).reshape(mat.rows, mat.cols, 3);
  NIY;
  return byteA();
}

char cvShow(const byteA& img, const char *window="opencv", bool wait=false);
char cvShow(const floatA& img, const char *window="opencv", bool wait=false);
void getDiffProb(floatA& diff, const byteA& img0, const byteA& img1, float pixSdv, uint range);

#else

inline char cvShow(const byteA& img, const char *window="opencv", bool wait=false) { NICO }
inline char cvShow(const floatA& img, const char *window="opencv", bool wait=false) { NICO };
void getDiffProb(floatA& diff, const byteA& img0, const byteA& img1, float pixSdv, uint range) { NICO };

#endif //RAI_OPENCV

#endif
