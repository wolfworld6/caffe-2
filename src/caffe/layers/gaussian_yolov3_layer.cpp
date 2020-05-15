/*
 * @Author: Eric612
 * @Date:   2018-08-20
 * @https://github.com/eric612/Caffe-YOLOv2-Windows
 * @https://github.com/eric612/MobileNet-YOLO
 * Avisonic
 */
#include "caffe/layers/gaussian_yolov3_layer.hpp"
#include "caffe/layers/sigmoid_layer.hpp"
#include "caffe/util/math_functions.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <algorithm>
#include <cfloat>
#include <vector>

#ifdef USE_OPENCV
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#endif // USE_OPENCV

namespace caffe {

template <typename Dtype>
void delta_region_gaussian_class_v3(Dtype *input_data, Dtype *&diff, int index,
                                    int class_label, int classes, float scale,
                                    Dtype *avg_cat, int stride,
                                    bool use_focal_loss) {
  if (diff[index]) {
    diff[index + stride * class_label] =
        (-1.0) * (1 - input_data[index + stride * class_label]);
    *avg_cat += input_data[index + stride * class_label];
    // LOG(INFO)<<"test";
    return;
  }
  if (use_focal_loss) {
    // Reference :
    // https://github.com/AlexeyAB/darknet/blob/master/src/yolo_layer.c
    float alpha = 0.5; // 0.25 or 0.5
    // float gamma = 2;    // hardcoded in many places of the grad-formula

    int ti = index + stride * class_label;
    float pt = input_data[ti] + 0.000000000000001F;
    // http://fooplot.com/#W3sidHlwZSI6MCwiZXEiOiItKDEteCkqKDIqeCpsb2coeCkreC0xKSIsImNvbG9yIjoiIzAwMDAwMCJ9LHsidHlwZSI6MTAwMH1d
    float grad = -(1 - pt) *
                 (2 * pt * logf(pt) + pt -
                  1); // http://blog.csdn.net/linmingan/article/details/77885832
    // float grad = (1 - pt) * (2 * pt*logf(pt) + pt - 1);    //
    // https://github.com/unsky/focal-loss

    for (int n = 0; n < classes; ++n) {
      diff[index + stride * n] =
          (-1.0) * scale *
          (((n == class_label) ? 1 : 0) - input_data[index + n * stride]);

      diff[index + stride * n] *= alpha * grad;

      if (n == class_label) {
        *avg_cat += input_data[index + stride * n];
      }
    }

  } else {
    for (int n = 0; n < classes; ++n) {
      diff[index + n * stride] =
          (-1.0) * scale *
          (((n == class_label) ? 1 : 0) - input_data[index + n * stride]);
      // std::cout<<diff[index+n]<<",";
      if (n == class_label) {
        *avg_cat += input_data[index + n * stride];
        // std::cout<<"avg_cat:"<<input_data[index+n]<<std::endl;
      }
    }
  }
}
static inline float fix_nan_inf(float val) {
  if (isnan(val) || isinf(val))
    val = 0;
  return val;
}
static inline float clip_value(float val, const float max_val) {
  if (val > max_val) {
    // printf("\n val = %f > max_val = %f \n", val, max_val);
    val = max_val;
  } else if (val < -max_val) {
    // printf("\n val = %f < -max_val = %f \n", val, -max_val);
    val = -max_val;
  }
  return val;
}

// Reference :
// https://github.com/jwchoi384/Gaussian_YOLOv3/blob/master/src/gaussian_yolo_layer.c
template <typename Dtype>
Dtype delta_region_gaussian_box(vector<Dtype> truth, Dtype *x,
                                vector<Dtype> biases, int n, int index, int i,
                                int j, int lw, int lh, int w, int h,
                                Dtype *delta, float scale, int stride,
                                IOU_LOSS iou_loss, float iou_normalizer,
                                float max_delta, bool accumulate) {
  vector<Dtype> pred;
  pred.clear();

  get_gaussian_yolo_box(pred, x, biases, n, index, i, j, lw, lh, w, h, stride);
  Dtype iou = box_iou(pred, truth);

  Dtype tx = (truth[0] * lw - i);
  Dtype ty = (truth[1] * lh - j);
  Dtype tw = log(truth[2] * w / biases[2 * n]);
  Dtype th = log(truth[3] * h / biases[2 * n + 1]);

  Dtype sigma_const = 0.3;
  Dtype epsi = pow(10, -9);

  Dtype in_exp_x = (tx - x[index + 0 * stride]) / x[index + 1 * stride];
  Dtype in_exp_x_2 = pow(in_exp_x, 2);
  Dtype normal_dist_x =
      exp(in_exp_x_2 * (-1. / 2.)) /
      (sqrt(M_PI * 2.0) * (x[index + 1 * stride] + sigma_const));

  Dtype in_exp_y = (ty - x[index + 2 * stride]) / x[index + 3 * stride];
  Dtype in_exp_y_2 = pow(in_exp_y, 2);
  Dtype normal_dist_y =
      exp(in_exp_y_2 * (-1. / 2.)) /
      (sqrt(M_PI * 2.0) * (x[index + 3 * stride] + sigma_const));

  Dtype in_exp_w = (tw - x[index + 4 * stride]) / x[index + 5 * stride];
  Dtype in_exp_w_2 = pow(in_exp_w, 2);
  Dtype normal_dist_w =
      exp(in_exp_w_2 * (-1. / 2.)) /
      (sqrt(M_PI * 2.0) * (x[index + 5 * stride] + sigma_const));

  Dtype in_exp_h = (th - x[index + 6 * stride]) / x[index + 7 * stride];
  Dtype in_exp_h_2 = pow(in_exp_h, 2);
  Dtype normal_dist_h =
      exp(in_exp_h_2 * (-1. / 2.)) /
      (sqrt(M_PI * 2.0) * (x[index + 7 * stride] + sigma_const));

  Dtype temp_x =
      (1. / 2.) * 1. / (normal_dist_x + epsi) * normal_dist_x * scale;
  Dtype temp_y =
      (1. / 2.) * 1. / (normal_dist_y + epsi) * normal_dist_y * scale;
  Dtype temp_w =
      (1. / 2.) * 1. / (normal_dist_w + epsi) * normal_dist_w * scale;
  Dtype temp_h =
      (1. / 2.) * 1. / (normal_dist_h + epsi) * normal_dist_h * scale;

  Dtype dx = temp_x * in_exp_x * (1. / x[index + 1 * stride]);
  Dtype dy = temp_y * in_exp_y * (1. / x[index + 3 * stride]);
  Dtype dw = temp_w * in_exp_w * (1. / x[index + 5 * stride]);
  Dtype dh = temp_h * in_exp_h * (1. / x[index + 7 * stride]);

  dx *= iou_normalizer;
  dy *= iou_normalizer;
  dw *= iou_normalizer;
  dh *= iou_normalizer;

  dx = fix_nan_inf(dx);
  dy = fix_nan_inf(dy);
  dw = fix_nan_inf(dw);
  dh = fix_nan_inf(dh);

  if (max_delta != FLT_MAX) {
    dx = clip_value(dx, max_delta);
    dy = clip_value(dy, max_delta);
    dw = clip_value(dw, max_delta);
    dh = clip_value(dh, max_delta);
  }

  if (!accumulate) {
    delta[index + 0 * stride] = 0;
    delta[index + 1 * stride] = 0;
    delta[index + 2 * stride] = 0;
    delta[index + 3 * stride] = 0;
    delta[index + 4 * stride] = 0;
    delta[index + 5 * stride] = 0;
    delta[index + 6 * stride] = 0;
    delta[index + 7 * stride] = 0;
  }

  delta[index + 0 * stride] += (-1) * dx;
  delta[index + 2 * stride] += (-1) * dy;
  delta[index + 4 * stride] += (-1) * dw;
  delta[index + 6 * stride] += (-1) * dh;

  dx = temp_x * (in_exp_x_2 / x[index + 1 * stride] -
                 1. / (x[index + 1 * stride] + sigma_const));
  dy = temp_y * (in_exp_y_2 / x[index + 3 * stride] -
                 1. / (x[index + 3 * stride] + sigma_const));
  dw = temp_w * (in_exp_w_2 / x[index + 5 * stride] -
                 1. / (x[index + 5 * stride] + sigma_const));
  dh = temp_h * (in_exp_h_2 / x[index + 7 * stride] -
                 1. / (x[index + 7 * stride] + sigma_const));

  dx *= iou_normalizer;
  dy *= iou_normalizer;
  dw *= iou_normalizer;
  dh *= iou_normalizer;

  dx = fix_nan_inf(dx);
  dy = fix_nan_inf(dy);
  dw = fix_nan_inf(dw);
  dh = fix_nan_inf(dh);

  if (max_delta != FLT_MAX) {
    dx = clip_value(dx, max_delta);
    dy = clip_value(dy, max_delta);
    dw = clip_value(dw, max_delta);
    dh = clip_value(dh, max_delta);
  }
  delta[index + 1 * stride] += (-1) * dx;
  delta[index + 3 * stride] += (-1) * dy;
  delta[index + 5 * stride] += (-1) * dw;
  delta[index + 7 * stride] += (-1) * dh;
  return iou;
}

template <typename Dtype>
void GaussianYolov3Layer<Dtype>::LayerSetUp(const vector<Blob<Dtype> *> &bottom,
                                            const vector<Blob<Dtype> *> &top) {
  LossLayer<Dtype>::LayerSetUp(bottom, top);
  Yolov3Parameter param = this->layer_param_.yolov3_param();
  iter_ = 0;
  num_class_ = param.num_class(); // 20
  num_ = param.num();             // 5
  side_w_ = bottom[0]->width();
  side_h_ = bottom[0]->height();
  anchors_scale_ = param.anchors_scale();
  object_scale_ = param.object_scale();     // 5.0
  noobject_scale_ = param.noobject_scale(); // 1.0
  class_scale_ = param.class_scale();       // 1.0
  coord_scale_ = param.coord_scale();       // 1.0
  thresh_ = param.thresh();                 // 0.6
  use_logic_gradient_ = param.use_logic_gradient();
  use_focal_loss_ = param.use_focal_loss();
  iou_loss_ = (IOU_LOSS)param.iou_loss();

  iou_normalizer_ = param.iou_normalizer();
  iou_thresh_ = param.iou_thresh();
  max_delta_ = param.max_delta();
  accumulate_ = param.accumulate();
  for (int c = 0; c < param.biases_size(); ++c) {
    biases_.push_back(param.biases(c));
  }
  for (int c = 0; c < param.mask_size(); ++c) {
    mask_.push_back(param.mask(c));
  }
  biases_size_ = param.biases_size() / 2;
  int input_count =
      bottom[0]->count(1); // h*w*n*(classes+coords+1) = 13*13*5*(20+4+1)
  int label_count = bottom[1]->count(1); // 30*5-
  // outputs: classes, iou, coordinates
  int tmp_input_count =
      side_w_ * side_h_ * num_ *
      (8 + num_class_ +
       1); // 13*13*5*(20+4+1) label: isobj, class_label, coordinates
  int tmp_label_count = 300 * num_;
  CHECK_EQ(input_count, tmp_input_count);
  // CHECK_EQ(label_count, tmp_label_count);
}

template <typename Dtype>
void GaussianYolov3Layer<Dtype>::Reshape(const vector<Blob<Dtype> *> &bottom,
                                         const vector<Blob<Dtype> *> &top) {
  LossLayer<Dtype>::Reshape(bottom, top);
  diff_.ReshapeLike(*bottom[0]);
  real_diff_.ReshapeLike(*bottom[0]);
}
template <typename Dtype>
int int_index(vector<Dtype> a, int val, int n) {
  int i;
  for (i = 0; i < n; ++i) {
    if (a[i] == val)
      return i;
  }
  return -1;
}
template <typename Dtype>
void GaussianYolov3Layer<Dtype>::Forward_cpu(
    const vector<Blob<Dtype> *> &bottom, const vector<Blob<Dtype> *> &top) {
  side_w_ = bottom[0]->width();
  side_h_ = bottom[0]->height();
  // LOG(INFO)<<"iou loss" << iou_loss_<<","<<GIOU;
  // LOG(INFO) << side_*anchors_scale_;
  const Dtype *label_data = bottom[1]->cpu_data(); //[label,x,y,w,h]
  if (diff_.width() != bottom[0]->width()) {
    diff_.ReshapeLike(*bottom[0]);
  }
  Dtype *diff = diff_.mutable_cpu_data();
  caffe_set(diff_.count(), Dtype(0.0), diff);

  Dtype avg_anyobj(0.0), avg_obj(0.0), avg_iou(0.0), avg_cat(0.0), recall(0.0),
      recall75(0.0), loss(0.0), avg_iou_loss(0.0);
  int count = 0;

  const Dtype *input_data = bottom[0]->cpu_data();
  // const Dtype* label_data = bottom[1]->cpu_data();
  if (swap_.width() != bottom[0]->width()) {
    swap_.ReshapeLike(*bottom[0]);
  }
  Dtype *swap_data = swap_.mutable_cpu_data();
  int len = 8 + num_class_ + 1;
  int stride = side_w_ * side_h_;
  /*for (int i = 0; i < 81; i++) {
    char label[100];
    sprintf(label, "%d,%s\n",i, CLASSES[static_cast<int>(i )]);
    printf(label);
  }*/

  for (int b = 0; b < bottom[0]->num(); b++) {
    /*//if (b == 0) {
      char buf[100];
      int idx = iter_*bottom[0]->num() + b;
      sprintf(buf, "input/input_%05d.jpg", idx+1 );
      //int idx = (iter*swap.num() % 200) + b;
      cv::Mat cv_img = cv::imread(buf);
      for (int t = 0; t < 300; ++t) {
        vector<Dtype> truth;
        Dtype c = label_data[b * 300 * 5 + t * 5 + 0];
        Dtype x = label_data[b * 300 * 5 + t * 5 + 1];

        Dtype y = label_data[b * 300 * 5 + t * 5 + 2];
        Dtype w = label_data[b * 300 * 5 + t * 5 + 3];
        Dtype h = label_data[b * 300 * 5 + t * 5 + 4];
        if (!x) break;
        float left = (x - w / 2.);
        float right = (x + w / 2.);
        float top = (y - h / 2.);
        float bot = (y + h / 2.);

        cv::Point pt1, pt2;
        pt1.x = left*cv_img.cols;
        pt1.y = top*cv_img.rows;
        pt2.x = right*cv_img.cols;
        pt2.y = bot*cv_img.rows;

        cv::rectangle(cv_img, pt1, pt2, cvScalar(0, 255, 0), 1, 8, 0);
        char label[100];
        sprintf(label, "%s", CLASSES[static_cast<int>(c + 1)]);
        int baseline;
        cv::Size size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 0,
    &baseline); cv::Point pt3; pt3.x = pt1.x + size.width; pt3.y = pt1.y -
    size.height; cv::rectangle(cv_img, pt1, pt3, cvScalar(0, 255, 0), -1);


        cv::putText(cv_img, label, pt1, cv::FONT_HERSHEY_SIMPLEX, 0.5,
    cv::Scalar(0, 0, 0));
        //LOG(INFO) << "Truth box" << "," << c << "," << x << "," << y << "," <<
    w << "," << h;
      }
      sprintf(buf, "out/out_%05d.jpg", idx);
      cv::imwrite(buf, cv_img);
    //}*/
    for (int s = 0; s < stride; s++) {
      for (int n = 0; n < num_; n++) {
        int index = n * len * stride + s + b * bottom[0]->count(1);
        // LOG(INFO)<<index;
        vector<Dtype> pred;
        float best_iou = 0;
        int best_class = -1;
        vector<Dtype> best_truth;
#ifdef CPU_ONLY
        for (int c = 0; c < len; ++c) {
          int index2 = c * stride + index;
          // LOG(INFO)<<index2;
          if (c == 4 || c == 6) {
            swap_data[index2] = (input_data[index2 + 0]);
          } else {
            swap_data[index2] = caffe_fn_sigmoid(input_data[index2 + 0]);
          }
        }
#endif
        int y2 = s / side_w_;
        int x2 = s % side_w_;
        get_gaussian_yolo_box(pred, swap_data, biases_, mask_[n], index, x2, y2,
                              side_w_, side_h_, side_w_ * anchors_scale_,
                              side_h_ * anchors_scale_, stride);
        for (int t = 0; t < 300; ++t) {
          vector<Dtype> truth;
          Dtype x = label_data[b * 300 * 5 + t * 5 + 1];
          Dtype y = label_data[b * 300 * 5 + t * 5 + 2];
          Dtype w = label_data[b * 300 * 5 + t * 5 + 3];
          Dtype h = label_data[b * 300 * 5 + t * 5 + 4];

          if (!x)
            break;

          truth.push_back(x);
          truth.push_back(y);
          truth.push_back(w);
          truth.push_back(h);
          Dtype iou = box_iou(pred, truth);
          if (iou > best_iou) {
            best_class = label_data[b * 300 * 5 + t * 5];
            best_iou = iou;
            best_truth = truth;
          }
        }
        avg_anyobj += swap_data[index + 8 * stride];
        diff[index + 8 * stride] = (-1) * (0 - swap_data[index + 8 * stride]);
        // diff[index + 4 * stride] = (-1) * (0 - exp(input_data[index + 4 *
        // stride]-exp(input_data[index + 4 * stride]))); diff[index + 4 *
        // stride] = (-1) * noobject_scale_ * (0 - swap_data[index + 4 *
        // stride]) *caffe_fn_sigmoid_gradient(swap_data[index + 4 * stride]);
        if (best_iou > thresh_) {
          diff[index + 8 * stride] = 0;
        }
        if (best_iou > 1) {
          LOG(INFO) << "best_iou > 1"; // plz tell me ..
          diff[index + 8 * stride] = (-1) * (1 - swap_data[index + 8 * stride]);

          delta_region_gaussian_class_v3(swap_data, diff, index + 9 * stride,
                                         best_class, num_class_, class_scale_,
                                         &avg_cat, stride, use_focal_loss_);
          delta_region_gaussian_box(
              best_truth, swap_data, biases_, mask_[n], index, x2, y2, side_w_,
              side_h_, side_w_ * anchors_scale_, side_h_ * anchors_scale_, diff,
              coord_scale_ * (2 - best_truth[2] * best_truth[3]), stride,
              iou_loss_, iou_normalizer_, max_delta_, accumulate_);
        }
      }
    }
    // vector<Dtype> used;
    // used.clear();
    for (int t = 0; t < 300; ++t) {
      vector<Dtype> truth;
      truth.clear();
      int class_label = label_data[t * 5 + b * 300 * 5 + 0];
      float x = label_data[t * 5 + b * 300 * 5 + 1];
      float y = label_data[t * 5 + b * 300 * 5 + 2];
      float w = label_data[t * 5 + b * 300 * 5 + 3];
      float h = label_data[t * 5 + b * 300 * 5 + 4];

      if (!w)
        break;
      truth.push_back(x);
      truth.push_back(y);
      truth.push_back(w);
      truth.push_back(h);
      float best_iou = 0;
      int best_index = 0;
      int best_n = -1;
      int i = truth[0] * side_w_;
      int j = truth[1] * side_h_;
      int pos = j * side_w_ + i;
      vector<Dtype> truth_shift;
      truth_shift.clear();
      truth_shift.push_back(0);
      truth_shift.push_back(0);
      truth_shift.push_back(w);
      truth_shift.push_back(h);

      // LOG(INFO) << j << "," << i << "," << anchors_scale_;

      for (int n = 0; n < biases_size_; ++n) {
        vector<Dtype> pred(4);
        pred[2] = biases_[2 * n] / (float)(side_w_ * anchors_scale_);
        pred[3] = biases_[2 * n + 1] / (float)(side_h_ * anchors_scale_);

        pred[0] = 0;
        pred[1] = 0;
        float iou = box_iou(pred, truth_shift);
        if (iou > best_iou) {
          best_n = n;
          best_iou = iou;
        }
      }
      int mask_n = int_index(mask_, best_n, num_);
      if (mask_n >= 0) {
        bool overlap = false;
        float iou;
        best_n = mask_n;
        // LOG(INFO) << best_n;
        best_index = best_n * len * stride + pos + b * bottom[0]->count(1);

        iou = delta_region_gaussian_box(
            truth, swap_data, biases_, mask_[best_n], best_index, i, j, side_w_,
            side_h_, side_w_ * anchors_scale_, side_h_ * anchors_scale_, diff,
            coord_scale_ * (2 - truth[2] * truth[3]), stride, iou_loss_,
            iou_normalizer_, max_delta_, accumulate_);

        if (iou > 0.5)
          recall += 1;
        if (iou > 0.75)
          recall75 += 1;
        avg_iou += iou;
        avg_iou_loss += (1 - iou);
        avg_obj += swap_data[best_index + 8 * stride];
        if (use_logic_gradient_) {
          diff[best_index + 8 * stride] =
              (-1.0) * (1 - swap_data[best_index + 8 * stride]) * object_scale_;
        } else {
          diff[best_index + 8 * stride] =
              (-1.0) * (1 - swap_data[best_index + 8 * stride]);
          // diff[best_index + 4 * stride] = (-1) * (1 -
          // exp(input_data[best_index + 4 * stride] - exp(input_data[best_index
          // + 4 * stride])));
        }

        // diff[best_index + 4 * stride] = (-1.0) * (1 - swap_data[best_index +
        // 4 * stride]) ;

        delta_region_gaussian_class_v3(swap_data, diff, best_index + 9 * stride,
                                       class_label, num_class_, class_scale_,
                                       &avg_cat, stride,
                                       use_focal_loss_); // softmax_tree_

        ++count;
        ++class_count_;
      }

      for (int n = 0; n < biases_size_; ++n) {
        int mask_n = int_index(mask_, n, num_);
        if (mask_n >= 0 && n != best_n && iou_thresh_ < 1.0f) {
          vector<Dtype> pred(4);
          pred[2] = biases_[2 * n] / (float)(side_w_ * anchors_scale_);
          pred[3] = biases_[2 * n + 1] / (float)(side_h_ * anchors_scale_);

          pred[0] = 0;
          pred[1] = 0;
          float iou;

          iou = box_iou(pred, truth_shift);
          if (iou > iou_thresh_) {
            bool overlap = false;
            float iou;
            // LOG(INFO) << best_n;
            best_index = mask_n * len * stride + pos + b * bottom[0]->count(1);

            iou = delta_region_gaussian_box(
                truth, swap_data, biases_, mask_[mask_n], best_index, i, j,
                side_w_, side_h_, side_w_ * anchors_scale_,
                side_h_ * anchors_scale_, diff,
                coord_scale_ * (2 - truth[2] * truth[3]), stride, iou_loss_,
                iou_normalizer_, max_delta_, accumulate_);

            if (iou > 0.5)
              recall += 1;
            if (iou > 0.75)
              recall75 += 1;
            avg_iou += iou;
            avg_iou_loss += (1 - iou);
            avg_obj += swap_data[best_index + 8 * stride];
            if (use_logic_gradient_) {
              diff[best_index + 8 * stride] =
                  (-1.0) * (1 - swap_data[best_index + 8 * stride]) *
                  object_scale_;
            } else {
              diff[best_index + 8 * stride] =
                  (-1.0) * (1 - swap_data[best_index + 8 * stride]);
              // diff[best_index + 4 * stride] = (-1) * (1 -
              // exp(input_data[best_index + 4 * stride] -
              // exp(input_data[best_index + 4 * stride])));
            }

            // diff[best_index + 4 * stride] = (-1.0) * (1 -
            // swap_data[best_index + 4 * stride]) ;

            delta_region_gaussian_class_v3(
                swap_data, diff, best_index + 9 * stride, class_label,
                num_class_, class_scale_, &avg_cat, stride,
                use_focal_loss_); // softmax_tree_

            ++count;
            ++class_count_;
          }
        }
      }
    }
  }

  for (int b = 0; b < bottom[0]->num(); b++) {
    for (int s = 0; s < stride; s++) {
      for (int n = 0; n < num_; n++) {
        int index = n * len * stride + s + b * bottom[0]->count(1);
        for (int c = 0; c < len; ++c) {
          int index2 = c * stride + index;
          // LOG(INFO)<<index2;
          if (c < 8) {
            swap_data[index2] = (input_data[index2 + 0]);
          } else {
            loss += diff[index2] * diff[index2];
          }
        }
      }
    }
  }
  // LOG(INFO) << avg_iou_loss;
  if (count > 0) {
    loss += iou_normalizer_ * avg_iou_loss / count;
  }

  top[0]->mutable_cpu_data()[0] = loss / bottom[0]->num();

  // LOG(INFO) << "avg_noobj: " << avg_anyobj / (side_ * side_ * num_ *
  // bottom[0]->num());
  iter_++;
  // LOG(INFO) << "iter: " << iter <<" loss: " << loss;
  if (!(iter_ % 16)) {
    if (time_count_ > 0) {
      LOG(INFO) << "noobj: " << score_.avg_anyobj / time_count_
                << " obj: " << score_.avg_obj / time_count_
                << " iou: " << score_.avg_iou / time_count_
                << " cat: " << score_.avg_cat / time_count_
                << " recall: " << score_.recall / time_count_
                << " recall75: " << score_.recall75 / time_count_
                << " count: " << class_count_ / time_count_;
      // LOG(INFO) << "avg_noobj: "<<
      // avg_anyobj/(side_*side_*num_*bottom[0]->num()) << " avg_obj: " <<
      // avg_obj/count <<" avg_iou: " << avg_iou/count << " avg_cat: " <<
      // avg_cat/class_count << " recall: " << recall/count << " recall75: " <<
      // recall75 / count;
      score_.avg_anyobj = score_.avg_obj = score_.avg_iou = score_.avg_cat =
          score_.recall = score_.recall75 = 0;
      class_count_ = 0;
      time_count_ = 0;
    }
  } else {
    score_.avg_anyobj +=
        avg_anyobj / (side_w_ * side_h_ * num_ * bottom[0]->num());
    if (count > 0) {
      score_.avg_obj += avg_obj / count;
      score_.avg_iou += avg_iou / count;
      score_.avg_cat += avg_cat / count;
      score_.recall += recall / count;
      score_.recall75 += recall75 / count;
      time_count_++;
    }
  }
}

template <typename Dtype>
void GaussianYolov3Layer<Dtype>::Backward_cpu(
    const vector<Blob<Dtype> *> &top, const vector<bool> &propagate_down,
    const vector<Blob<Dtype> *> &bottom) {
  // LOG(INFO) <<" propagate_down: "<< propagate_down[1] << " " <<
  // propagate_down[0];
  if (propagate_down[1]) {
    LOG(FATAL) << this->type()
               << " Layer cannot backpropagate to label inputs.";
  }
  if (propagate_down[0]) {
    if (use_logic_gradient_) {
      const Dtype *top_data = swap_.cpu_data();
      Dtype *diff = diff_.mutable_cpu_data();
      side_w_ = bottom[0]->width();
      side_h_ = bottom[0]->height();
      int len = 8 + num_class_ + 1;
      int stride = side_w_ * side_h_;
      // LOG(INFO)<<swap.count(1);
      for (int b = 0; b < bottom[0]->num(); b++) {
        for (int s = 0; s < stride; s++) {
          for (int n = 0; n < num_; n++) {
            int index = n * len * stride + s + b * bottom[0]->count(1);
            // LOG(INFO)<<index;
            vector<Dtype> pred;
            float best_iou = 0;
            int best_class = -1;
            vector<Dtype> best_truth;
            for (int c = 0; c < len; ++c) {
              int index2 = c * stride + index;
              // LOG(INFO)<<index2;
              if (c == 4 || c == 6) {
                diff[index2] = diff[index2 + 0];
              } else {
                diff[index2] = diff[index2 + 0] *
                               caffe_fn_sigmoid_grad_fast(top_data[index2 + 0]);
              }
            }
          }
        }
      }
    } else {
      // non-logic_gradient formula
      // https://blog.csdn.net/yanzi6969/article/details/80505421
      // https://xmfbit.github.io/2018/03/21/cs229-supervised-learning/
      // https://zlatankr.github.io/posts/2017/03/06/mle-gradient-descent
    }
    const Dtype sign(1.);
    const Dtype alpha = sign * top[0]->cpu_diff()[0] / bottom[0]->num();
    // const Dtype alpha(1.0);
    // LOG(INFO) << "alpha:" << alpha;

    caffe_blas_axpby(bottom[0]->count(), alpha, diff_.cpu_data(), Dtype(0),
                     bottom[0]->mutable_cpu_diff());
  }
}

#ifdef CPU_ONLY
STUB_GPU(GaussianYolov3Layer);
#endif

INSTANTIATE_CLASS(GaussianYolov3Layer);
REGISTER_LAYER_CLASS(GaussianYolov3);

} // namespace caffe
