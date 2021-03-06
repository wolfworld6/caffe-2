#ifndef IM_TRANSFORMS_HPP
#define IM_TRANSFORMS_HPP

#ifdef USE_OPENCV
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#endif // USE_OPENCV

#include <vector>

#include "caffe/common.hpp"
#include "caffe/proto/caffe.pb.h"

namespace caffe {

namespace internal {
template <typename T>
bool is_border(const cv::Mat &edge, T color);

// Auto cropping image.
template <typename T>
cv::Rect CropMask(const cv::Mat &src, T point, int padding = 2);

cv::Mat colorReduce(const cv::Mat &image, int div = 64);

void fillEdgeImage(const cv::Mat &edgesIn, cv::Mat *filledEdgesOut);

void CenterObjectAndFillBg(const cv::Mat &in_img, bool fill_bg,
                           cv::Mat *out_img);

cv::Mat AspectKeepingResizeAndPad(const cv::Mat &in_img, int new_width,
                                  int new_height,
                                  int pad_type = cv::BORDER_CONSTANT,
                                  const cv::Scalar &pad = cv::Scalar(0, 0, 0),
                                  int interp_mode = cv::INTER_LINEAR);

cv::Mat AspectKeepingResizeBySmall(const cv::Mat &in_img, int new_width,
                                   int new_height,
                                   int interp_mode = cv::INTER_LINEAR);

void constantNoise(int n, const vector<uchar> &val, cv::Mat *image);
void RandomBrightness(const cv::Mat &in_img, cv::Mat *out_img,
                      float brightness_prob, float brightness_delta);

void AdjustBrightness(const cv::Mat &in_img, float delta, cv::Mat *out_img);

void RandomContrast(const cv::Mat &in_img, cv::Mat *out_img,
                    float contrast_prob, float lower, float upper);

void AdjustContrast(const cv::Mat &in_img, float delta, cv::Mat *out_img);

void RandomSaturation(const cv::Mat &in_img, cv::Mat *out_img,
                      float saturation_prob, float lower, float upper);

void AdjustSaturation(const cv::Mat &in_img, float delta, cv::Mat *out_img);

void RandomHue(const cv::Mat &in_img, cv::Mat *out_img, float hue_prob,
               float hue_delta);

void AdjustHue(const cv::Mat &in_img, float delta, cv::Mat *out_img);

void RandomOrderChannels(const cv::Mat &in_img, cv::Mat *out_img,
                         float random_order_prob);

void getEnlargedImage(const cv::Mat &in_img, const GeometryParameter &param,
                      cv::Mat &in_img_enlarged);

void getQuads(int rows, int cols, const GeometryParameter &param,
              cv::Point2f (&inputQuad)[4], cv::Point2f (&outQuad)[4]);
} // namespace internal

// Generate random number given the probablities for each number.
int roll_weighted_die(const std::vector<float> &probabilities);

void UpdateBBoxByResizePolicy(const ResizeParameter &param, int old_width,
                              int old_height, NormalizedBBox *bbox);

void InferNewSize(const ResizeParameter &resize_param, int old_width,
                  int old_height, int *new_width, int *new_height);

#ifdef USE_OPENCV

cv::Mat ApplyResize(const cv::Mat &in_img, const ResizeParameter &param);

cv::Mat ApplyNoise(const cv::Mat &in_img, const NoiseParameter &param);

cv::Mat ApplyDistort(const cv::Mat &in_img, const DistortionParameter &param);

void ApplyZoom(const cv::Mat &in_img, cv::Mat &out_img, const cv::Mat &in_lbl,
               cv::Mat &out_lbl, const ExpansionParameter &param);

cv::Mat ApplyGeometry(const cv::Mat &in_img, const GeometryParameter &param);
void ApplyGeometry(const cv::Mat &in_img, cv::Mat &out_img,
                   const cv::Mat &in_lbl, cv::Mat &out_lbl,
                   const GeometryParameter &param);
void ApplyGeometry(const cv::Mat &in_img, cv::Mat &out_img,
                   const AnnotatedDatum &anno_datum,
                   AnnotatedDatum &geom_anno_datum,
                   const GeometryParameter &param);
#endif // USE_OPENCV

} // namespace caffe

#endif // IM_TRANSFORMS_HPP
