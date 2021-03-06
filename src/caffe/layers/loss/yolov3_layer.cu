#include <cmath>
#include <vector>

#include "caffe/layers/loss/yolov3_layer.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/util/yolo_utils.hpp"

namespace caffe {
template <typename Dtype>
void Yolov3Layer<Dtype>::Forward_gpu(const vector<Blob<Dtype> *> &bottom,
                                     const vector<Blob<Dtype> *> &top) {
  const int count = bottom[0]->count();
  const Dtype *input_data = bottom[0]->gpu_data();
  if (swap_.width() != bottom[0]->width()) {
    swap_.ReshapeLike(*bottom[0]);
  }
  side_w_ = bottom[0]->width();
  side_h_ = bottom[0]->height();
  int len = 4 + num_class_ + 1;
  int stride = side_w_ * side_h_;
  Dtype *swap_data = swap_.mutable_gpu_data();
  // caffe_copy(count, input_data, swap_data);
  for (int b = 0; b < bottom[0]->num(); ++b) {
    for (int n = 0; n < num_; ++n) {
      int index = b * bottom[0]->count(1) + n * len * stride;
      activate_yolo_gpu(stride, index, num_class_, input_data, swap_data,
                        DEFAULT, false, true, 1.0);
    }
  }

  Forward_cpu(bottom, top);
}

template <typename Dtype>
void Yolov3Layer<Dtype>::Backward_gpu(const vector<Blob<Dtype> *> &top,
                                      const vector<bool> &propagate_down,
                                      const vector<Blob<Dtype> *> &bottom) {
  if (propagate_down[0]) {
    if (use_logic_gradient_) {
      Backward_cpu(top, propagate_down, bottom);
    } else {
      const Dtype sign(1.);
      const Dtype alpha = sign * top[0]->cpu_diff()[0] / bottom[0]->num();
      caffe_gpu_axpby(bottom[0]->count(), alpha, diff_.gpu_data(), Dtype(0),
                      bottom[0]->mutable_gpu_diff());
    }
  }
}

INSTANTIATE_LAYER_GPU_FUNCS(Yolov3Layer);

} // namespace caffe
