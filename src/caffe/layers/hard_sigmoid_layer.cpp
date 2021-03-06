#include <cmath>
#include <vector>

#include "caffe/layers/hard_sigmoid_layer.hpp"
#include "caffe/util/math_functions.hpp"
#include <algorithm>
#include <iostream>
namespace caffe {

template <typename Dtype>
void HardSigmoidLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype> *> &bottom,
                                          const vector<Blob<Dtype> *> &top) {
  const Dtype *bottom_data = bottom[0]->cpu_data();
  Dtype *top_data = top[0]->mutable_cpu_data();
  const int count = bottom[0]->count();
  caffe_hard_sigmoid(count, bottom_data, top_data);
}

template <typename Dtype>
void HardSigmoidLayer<Dtype>::Backward_cpu(
    const vector<Blob<Dtype> *> &top, const vector<bool> &propagate_down,
    const vector<Blob<Dtype> *> &bottom) {
  if (propagate_down[0]) {
    const Dtype *top_data = top[0]->cpu_data();
    const Dtype *top_diff = top[0]->cpu_diff();
    Dtype *bottom_diff = bottom[0]->mutable_cpu_diff();
    const int count = bottom[0]->count();
    parallel_for(count, [&](int i) {
      Dtype sigmoid_x = top_data[i];
      // bottom_diff[i] = top_diff[i] * sigmoid_x * (1. - sigmoid_x);
      bottom_diff[i] =
          top_diff[i] * ((-2.5 < sigmoid_x) & (sigmoid_x < 2.5)) * 0.2;
    });
  }
}

#ifdef CPU_ONLY
// STUB_GPU(HardSigmoidLayer);
#endif

INSTANTIATE_CLASS(HardSigmoidLayer);

} // namespace caffe
