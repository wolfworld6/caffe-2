//
// Created by troyl on 5/11/2020.
//

#include "caffe/layers/mish_layer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {

template <typename Dtype>
void MishLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype> *> &bottom,
                                   const vector<Blob<Dtype> *> &top) {
  Blob<Dtype> *b = bottom[0];
  Blob<Dtype> *t = top[0];
  const Dtype *input_data = b->cpu_data();
  Dtype *output_data = t->mutable_cpu_data();
  const int count = b->count();

  Dtype x;
#pragma omp parallel for private(x)
  for (int i = 0; i < count; ++i) {
    x = input_data[i];
    output_data[i] = x * tanh(log(1 + exp(x)));
  }
}
template <typename Dtype>
void MishLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype> *> &top,
                                    const vector<bool> &propagate_down,
                                    const vector<Blob<Dtype> *> &bottom) {
  if (propagate_down[0]) {
    Blob<Dtype> *b = bottom[0];
    Blob<Dtype> *t = top[0];
    const int count = b->count();
    const Dtype *input_data = b->cpu_data();
    const Dtype *in_diff = t->cpu_diff();
    Dtype *out_diff = b->mutable_cpu_diff();

    Dtype x;
#pragma omp parallel for private(x)
    for (int i = 0; i < count; ++i) {
      x = input_data[i];
      Dtype w =
          4 * (x + 1) + (4 * exp(2 * x)) + exp(3 * x) + exp(x) * (4 * x + 6);
      Dtype sigma = 2 * exp(x) + exp(2 * x) + 2;
      out_diff[i] = (exp(x) * w / pow(sigma, 2)) * in_diff[i];
    }
  }
}

#ifdef CPU_ONLY
STUB_GPU(MishLayer);
#endif

INSTANTIATE_CLASS(MishLayer);
REGISTER_LAYER_CLASS(Mish);

} // namespace caffe