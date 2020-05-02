#include <string>
#include <vector>

#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"

#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/filler.hpp"
#include "caffe/layers/split_layer.hpp"
#include "caffe/proto/caffe.pb.h"
#include "caffe/util/insert_splits.hpp"

#include "caffe/test/test_caffe_main.hpp"
#include "caffe/test/test_gradient_check_util.hpp"

namespace caffe {

template<typename TypeParam>
class SplitLayerTest : public MultiDeviceTest<TypeParam> {
  typedef typename TypeParam::Dtype Dtype;

protected:
  SplitLayerTest()
      : blob_bottom_(new Blob<Dtype>(2, 3, 6, 5)),
        blob_top_a_(new Blob<Dtype>()),
        blob_top_b_(new Blob<Dtype>()) {
    // fill the values
    FillerParameter filler_param;
    GaussianFiller<Dtype> filler(filler_param);
    filler.Fill(this->blob_bottom_);
    blob_bottom_vec_.push_back(blob_bottom_);
    blob_top_vec_.push_back(blob_top_a_);
    blob_top_vec_.push_back(blob_top_b_);
  }

  virtual ~SplitLayerTest() {
    delete blob_bottom_;
    delete blob_top_a_;
    delete blob_top_b_;
  }

  Blob<Dtype> *const blob_bottom_;
  Blob<Dtype> *const blob_top_a_;
  Blob<Dtype> *const blob_top_b_;
  vector<Blob<Dtype> *> blob_bottom_vec_;
  vector<Blob<Dtype> *> blob_top_vec_;
};

TYPED_TEST_CASE(SplitLayerTest, TestDtypesAndDevices
);

TYPED_TEST(SplitLayerTest, TestSetup
) {
typedef typename TypeParam::Dtype Dtype;
LayerParameter layer_param;
SplitLayer<Dtype> layer(layer_param);
layer.SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
EXPECT_EQ(this->blob_top_a_->

num(),

2);
EXPECT_EQ(this->blob_top_a_->

channels(),

3);
EXPECT_EQ(this->blob_top_a_->

height(),

6);
EXPECT_EQ(this->blob_top_a_->

width(),

5);
EXPECT_EQ(this->blob_top_b_->

num(),

2);
EXPECT_EQ(this->blob_top_b_->

channels(),

3);
EXPECT_EQ(this->blob_top_b_->

height(),

6);
EXPECT_EQ(this->blob_top_b_->

width(),

5);
}

TYPED_TEST(SplitLayerTest, Test
) {
typedef typename TypeParam::Dtype Dtype;
LayerParameter layer_param;
SplitLayer <Dtype> layer(layer_param);
layer.SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
layer.Forward(this->blob_bottom_vec_, this->blob_top_vec_);
for (
int i = 0;
i < this->blob_bottom_->

count();

++i) {
Dtype bottom_value = this->blob_bottom_->cpu_data()[i];
EXPECT_EQ(bottom_value,
this->blob_top_a_->

cpu_data()[i]

);
EXPECT_EQ(bottom_value,
this->blob_top_b_->

cpu_data()[i]

);
}
}

TYPED_TEST(SplitLayerTest, TestGradient
) {
typedef typename TypeParam::Dtype Dtype;
LayerParameter layer_param;
SplitLayer <Dtype> layer(layer_param);
GradientChecker <Dtype> checker(1e-2, 1e-2);
checker.
CheckGradientEltwise(&layer,
this->blob_bottom_vec_,
this->blob_top_vec_);
}

class SplitLayerInsertionTest : public ::testing::Test {
protected:
  void RunInsertionTest(
      const string &input_param_string, const string &output_param_string) {
    // Test that InsertSplits called on the proto specified by
    // input_param_string results in the proto specified by
    // output_param_string.
    NetParameter input_param;
    CHECK(google::protobuf::TextFormat::ParseFromString(
        input_param_string, &input_param));
    NetParameter expected_output_param;
    CHECK(google::protobuf::TextFormat::ParseFromString(
        output_param_string, &expected_output_param));
    NetParameter actual_output_param;
    InsertSplits(input_param, &actual_output_param);
    EXPECT_EQ(expected_output_param.DebugString(),
              actual_output_param.DebugString());
    // Also test idempotence.
    NetParameter double_split_insert_param;
    InsertSplits(actual_output_param, &double_split_insert_param);
    EXPECT_EQ(actual_output_param.DebugString(),
              double_split_insert_param.DebugString());
  }
};

TEST_F(SplitLayerInsertionTest, TestNoInsertion1
) {
const string &input_proto =
    "name: 'TestNetwork' "
    "layer { "
    "  name: 'data' "
    "  type: 'Data' "
    "  top: 'data' "
    "  top: 'label' "
    "} "
    "layer { "
    "  name: 'innerprod' "
    "  type: 'InnerProduct' "
    "  bottom: 'data' "
    "  top: 'innerprod' "
    "} "
    "layer { "
    "  name: 'loss' "
    "  type: 'SoftmaxWithLoss' "
    "  bottom: 'innerprod' "
    "  bottom: 'label' "
    "} ";
this->
RunInsertionTest(input_proto, input_proto
);
}

TEST_F(SplitLayerInsertionTest, TestNoInsertion2
) {
const string &input_proto =
    "name: 'TestNetwork' "
    "layer { "
    "  name: 'data' "
    "  type: 'Data' "
    "  top: 'data' "
    "  top: 'label' "
    "} "
    "layer { "
    "  name: 'data_split' "
    "  type: 'Split' "
    "  bottom: 'data' "
    "  top: 'data_split_0' "
    "  top: 'data_split_1' "
    "} "
    "layer { "
    "  name: 'innerprod1' "
    "  type: 'InnerProduct' "
    "  bottom: 'data_split_0' "
    "  top: 'innerprod1' "
    "} "
    "layer { "
    "  name: 'innerprod2' "
    "  type: 'InnerProduct' "
    "  bottom: 'data_split_1' "
    "  top: 'innerprod2' "
    "} "
    "layer { "
    "  name: 'loss' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerprod1' "
    "  bottom: 'innerprod2' "
    "} ";
this->
RunInsertionTest(input_proto, input_proto
);
}

TEST_F(SplitLayerInsertionTest, TestNoInsertionImageNet
) {
const string &input_proto =
    "name: 'CaffeNet' "
    "layer { "
    "  name: 'data' "
    "  type: 'Data' "
    "  data_param { "
    "    source: '/home/jiayq/Data/ILSVRC12/train-leveldb' "
    "    batch_size: 256 "
    "  } "
    "  transform_param { "
    "    crop_size: 227 "
    "    mirror: true "
    "    mean_file: '/home/jiayq/Data/ILSVRC12/image_mean.binaryproto' "
    "  } "
    "  top: 'data' "
    "  top: 'label' "
    "} "
    "layer { "
    "  name: 'conv1' "
    "  type: 'Convolution' "
    "  convolution_param { "
    "    num_output: 96 "
    "    kernel_size: 11 "
    "    stride: 4 "
    "    weight_filler { "
    "      type: 'gaussian' "
    "      std: 0.01 "
    "    } "
    "    bias_filler { "
    "      type: 'constant' "
    "      value: 0. "
    "    } "
    "  } "
    "  param { "
    "    lr_mult: 1 "
    "    decay_mult: 1 "
    "  } "
    "  param { "
    "    lr_mult: 2 "
    "    decay_mult: 0 "
    "  } "
    "  bottom: 'data' "
    "  top: 'conv1' "
    "} "
    "layer { "
    "  name: 'relu1' "
    "  type: 'ReLU' "
    "  bottom: 'conv1' "
    "  top: 'conv1' "
    "} "
    "layer { "
    "  name: 'pool1' "
    "  type: 'Pooling' "
    "  pooling_param { "
    "    pool: MAX "
    "    kernel_size: 3 "
    "    stride: 2 "
    "  } "
    "  bottom: 'conv1' "
    "  top: 'pool1' "
    "} "
    "layer { "
    "  name: 'norm1' "
    "  type: 'LRN' "
    "  lrn_param { "
    "    local_size: 5 "
    "    alpha: 0.0001 "
    "    beta: 0.75 "
    "  } "
    "  bottom: 'pool1' "
    "  top: 'norm1' "
    "} "
    "layer { "
    "  name: 'conv2' "
    "  type: 'Convolution' "
    "  convolution_param { "
    "    num_output: 256 "
    "    group: 2 "
    "    kernel_size: 5 "
    "    pad: 2 "
    "    weight_filler { "
    "      type: 'gaussian' "
    "      std: 0.01 "
    "    } "
    "    bias_filler { "
    "      type: 'constant' "
    "      value: 1. "
    "    } "
    "  } "
    "  param { "
    "    lr_mult: 1 "
    "    decay_mult: 1 "
    "  } "
    "  param { "
    "    lr_mult: 2 "
    "    decay_mult: 0 "
    "  } "
    "  bottom: 'norm1' "
    "  top: 'conv2' "
    "} "
    "layer { "
    "  name: 'relu2' "
    "  type: 'ReLU' "
    "  bottom: 'conv2' "
    "  top: 'conv2' "
    "} "
    "layer { "
    "  name: 'pool2' "
    "  type: 'Pooling' "
    "  pooling_param { "
    "    pool: MAX "
    "    kernel_size: 3 "
    "    stride: 2 "
    "  } "
    "  bottom: 'conv2' "
    "  top: 'pool2' "
    "} "
    "layer { "
    "  name: 'norm2' "
    "  type: 'LRN' "
    "  lrn_param { "
    "    local_size: 5 "
    "    alpha: 0.0001 "
    "    beta: 0.75 "
    "  } "
    "  bottom: 'pool2' "
    "  top: 'norm2' "
    "} "
    "layer { "
    "  name: 'conv3' "
    "  type: 'Convolution' "
    "  convolution_param { "
    "    num_output: 384 "
    "    kernel_size: 3 "
    "    pad: 1 "
    "    weight_filler { "
    "      type: 'gaussian' "
    "      std: 0.01 "
    "    } "
    "    bias_filler { "
    "      type: 'constant' "
    "      value: 0. "
    "    } "
    "  } "
    "  param { "
    "    lr_mult: 1 "
    "    decay_mult: 1 "
    "  } "
    "  param { "
    "    lr_mult: 2 "
    "    decay_mult: 0 "
    "  } "
    "  bottom: 'norm2' "
    "  top: 'conv3' "
    "} "
    "layer { "
    "  name: 'relu3' "
    "  type: 'ReLU' "
    "  bottom: 'conv3' "
    "  top: 'conv3' "
    "} "
    "layer { "
    "  name: 'conv4' "
    "  type: 'Convolution' "
    "  convolution_param { "
    "    num_output: 384 "
    "    group: 2 "
    "    kernel_size: 3 "
    "    pad: 1 "
    "    weight_filler { "
    "      type: 'gaussian' "
    "      std: 0.01 "
    "    } "
    "    bias_filler { "
    "      type: 'constant' "
    "      value: 1. "
    "    } "
    "  } "
    "  param { "
    "    lr_mult: 1 "
    "    decay_mult: 1 "
    "  } "
    "  param { "
    "    lr_mult: 2 "
    "    decay_mult: 0 "
    "  } "
    "  bottom: 'conv3' "
    "  top: 'conv4' "
    "} "
    "layer { "
    "  name: 'relu4' "
    "  type: 'ReLU' "
    "  bottom: 'conv4' "
    "  top: 'conv4' "
    "} "
    "layer { "
    "  name: 'conv5' "
    "  type: 'Convolution' "
    "  convolution_param { "
    "    num_output: 256 "
    "    group: 2 "
    "    kernel_size: 3 "
    "    pad: 1 "
    "    weight_filler { "
    "      type: 'gaussian' "
    "      std: 0.01 "
    "    } "
    "    bias_filler { "
    "      type: 'constant' "
    "      value: 1. "
    "    } "
    "  } "
    "  param { "
    "    lr_mult: 1 "
    "    decay_mult: 1 "
    "  } "
    "  param { "
    "    lr_mult: 2 "
    "    decay_mult: 0 "
    "  } "
    "  bottom: 'conv4' "
    "  top: 'conv5' "
    "} "
    "layer { "
    "  name: 'relu5' "
    "  type: 'ReLU' "
    "  bottom: 'conv5' "
    "  top: 'conv5' "
    "} "
    "layer { "
    "  name: 'pool5' "
    "  type: 'Pooling' "
    "  pooling_param { "
    "    kernel_size: 3 "
    "    pool: MAX "
    "    stride: 2 "
    "  } "
    "  bottom: 'conv5' "
    "  top: 'pool5' "
    "} "
    "layer { "
    "  name: 'fc6' "
    "  type: 'InnerProduct' "
    "  inner_product_param { "
    "    num_output: 4096 "
    "    weight_filler { "
    "      type: 'gaussian' "
    "      std: 0.005 "
    "    } "
    "    bias_filler { "
    "      type: 'constant' "
    "      value: 1. "
    "    } "
    "  } "
    "  param { "
    "    lr_mult: 1 "
    "    decay_mult: 1 "
    "  } "
    "  param { "
    "    lr_mult: 2 "
    "    decay_mult: 0 "
    "  } "
    "  bottom: 'pool5' "
    "  top: 'fc6' "
    "} "
    "layer { "
    "  name: 'relu6' "
    "  type: 'ReLU' "
    "  bottom: 'fc6' "
    "  top: 'fc6' "
    "} "
    "layer { "
    "  name: 'drop6' "
    "  type: 'Dropout' "
    "  dropout_param { "
    "    dropout_ratio: 0.5 "
    "  } "
    "  bottom: 'fc6' "
    "  top: 'fc6' "
    "} "
    "layer { "
    "  name: 'fc7' "
    "  type: 'InnerProduct' "
    "  inner_product_param { "
    "    num_output: 4096 "
    "    weight_filler { "
    "      type: 'gaussian' "
    "      std: 0.005 "
    "    } "
    "    bias_filler { "
    "      type: 'constant' "
    "      value: 1. "
    "    } "
    "  } "
    "  param { "
    "    lr_mult: 1 "
    "    decay_mult: 1 "
    "  } "
    "  param { "
    "    lr_mult: 2 "
    "    decay_mult: 0 "
    "  } "
    "  bottom: 'fc6' "
    "  top: 'fc7' "
    "} "
    "layer { "
    "  name: 'relu7' "
    "  type: 'ReLU' "
    "  bottom: 'fc7' "
    "  top: 'fc7' "
    "} "
    "layer { "
    "  name: 'drop7' "
    "  type: 'Dropout' "
    "  dropout_param { "
    "    dropout_ratio: 0.5 "
    "  } "
    "  bottom: 'fc7' "
    "  top: 'fc7' "
    "} "
    "layer { "
    "  name: 'fc8' "
    "  type: 'InnerProduct' "
    "  inner_product_param { "
    "    num_output: 1000 "
    "    weight_filler { "
    "      type: 'gaussian' "
    "      std: 0.01 "
    "    } "
    "    bias_filler { "
    "      type: 'constant' "
    "      value: 0 "
    "    } "
    "  } "
    "  param { "
    "    lr_mult: 1 "
    "    decay_mult: 1 "
    "  } "
    "  param { "
    "    lr_mult: 2 "
    "    decay_mult: 0 "
    "  } "
    "  bottom: 'fc7' "
    "  top: 'fc8' "
    "} "
    "layer { "
    "  name: 'loss' "
    "  type: 'SoftmaxWithLoss' "
    "  bottom: 'fc8' "
    "  bottom: 'label' "
    "} ";
this->
RunInsertionTest(input_proto, input_proto
);
}

TEST_F(SplitLayerInsertionTest, TestNoInsertionWithInPlace
) {
const string &input_proto =
    "name: 'TestNetwork' "
    "layer { "
    "  name: 'data' "
    "  type: 'Data' "
    "  top: 'data' "
    "  top: 'label' "
    "} "
    "layer { "
    "  name: 'innerprod' "
    "  type: 'InnerProduct' "
    "  bottom: 'data' "
    "  top: 'innerprod' "
    "} "
    "layer { "
    "  name: 'relu' "
    "  type: 'ReLU' "
    "  bottom: 'innerprod' "
    "  top: 'innerprod' "
    "} "
    "layer { "
    "  name: 'loss' "
    "  type: 'SoftmaxWithLoss' "
    "  bottom: 'innerprod' "
    "  bottom: 'label' "
    "} ";
this->
RunInsertionTest(input_proto, input_proto
);
}

TEST_F(SplitLayerInsertionTest, TestLossInsertion
) {
const string &input_proto =
    "name: 'UnsharedWeightsNetwork' "
    "force_backward: true "
    "layer { "
    "  name: 'data' "
    "  type: 'DummyData' "
    "  dummy_data_param { "
    "    num: 5 "
    "    channels: 2 "
    "    height: 3 "
    "    width: 4 "
    "    data_filler { "
    "      type: 'gaussian' "
    "      std: 0.01 "
    "    } "
    "  } "
    "  top: 'data' "
    "} "
    "layer { "
    "  name: 'innerproduct1' "
    "  type: 'InnerProduct' "
    "  inner_product_param { "
    "    num_output: 10 "
    "    bias_term: false "
    "    weight_filler { "
    "      type: 'gaussian' "
    "      std: 10 "
    "    } "
    "  } "
    "  param { name: 'unsharedweights1' } "
    "  bottom: 'data' "
    "  top: 'innerproduct1' "
    "  loss_weight: 2.5 "
    "} "
    "layer { "
    "  name: 'innerproduct2' "
    "  type: 'InnerProduct' "
    "  inner_product_param { "
    "    num_output: 10 "
    "    bias_term: false "
    "    weight_filler { "
    "      type: 'gaussian' "
    "      std: 10 "
    "    } "
    "  } "
    "  param { name: 'unsharedweights2' } "
    "  bottom: 'data' "
    "  top: 'innerproduct2' "
    "} "
    "layer { "
    "  name: 'loss' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerproduct1' "
    "  bottom: 'innerproduct2' "
    "} ";
const string &expected_output_proto =
    "name: 'UnsharedWeightsNetwork' "
    "force_backward: true "
    "layer { "
    "  name: 'data' "
    "  type: 'DummyData' "
    "  dummy_data_param { "
    "    num: 5 "
    "    channels: 2 "
    "    height: 3 "
    "    width: 4 "
    "    data_filler { "
    "      type: 'gaussian' "
    "      std: 0.01 "
    "    } "
    "  } "
    "  top: 'data' "
    "} "
    "layer { "
    "  name: 'data_data_0_split' "
    "  type: 'Split' "
    "  bottom: 'data' "
    "  top: 'data_data_0_split_0' "
    "  top: 'data_data_0_split_1' "
    "} "
    "layer { "
    "  name: 'innerproduct1' "
    "  type: 'InnerProduct' "
    "  inner_product_param { "
    "    num_output: 10 "
    "    bias_term: false "
    "    weight_filler { "
    "      type: 'gaussian' "
    "      std: 10 "
    "    } "
    "  } "
    "  param { name: 'unsharedweights1' } "
    "  bottom: 'data_data_0_split_0' "
    "  top: 'innerproduct1' "
    "} "
    "layer { "
    "  name: 'innerproduct1_innerproduct1_0_split' "
    "  type: 'Split' "
    "  bottom: 'innerproduct1' "
    "  top: 'innerproduct1_innerproduct1_0_split_0' "
    "  top: 'innerproduct1_innerproduct1_0_split_1' "
    "  loss_weight: 2.5 "
    "  loss_weight: 0 "
    "} "
    "layer { "
    "  name: 'innerproduct2' "
    "  type: 'InnerProduct' "
    "  inner_product_param { "
    "    num_output: 10 "
    "    bias_term: false "
    "    weight_filler { "
    "      type: 'gaussian' "
    "      std: 10 "
    "    } "
    "  } "
    "  param { name: 'unsharedweights2' } "
    "  bottom: 'data_data_0_split_1' "
    "  top: 'innerproduct2' "
    "} "
    "layer { "
    "  name: 'loss' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerproduct1_innerproduct1_0_split_1' "
    "  bottom: 'innerproduct2' "
    "} ";
this->
RunInsertionTest(input_proto, expected_output_proto
);
}

TEST_F(SplitLayerInsertionTest, TestInsertion
) {
const string &input_proto =
    "name: 'TestNetwork' "
    "layer { "
    "  name: 'data' "
    "  type: 'Data' "
    "  top: 'data' "
    "  top: 'label' "
    "} "
    "layer { "
    "  name: 'innerprod1' "
    "  type: 'InnerProduct' "
    "  bottom: 'data' "
    "  top: 'innerprod1' "
    "} "
    "layer { "
    "  name: 'innerprod2' "
    "  type: 'InnerProduct' "
    "  bottom: 'data' "
    "  top: 'innerprod2' "
    "} "
    "layer { "
    "  name: 'innerprod3' "
    "  type: 'InnerProduct' "
    "  bottom: 'data' "
    "  top: 'innerprod3' "
    "} "
    "layer { "
    "  name: 'loss1' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerprod1' "
    "  bottom: 'innerprod2' "
    "} "
    "layer { "
    "  name: 'loss2' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerprod2' "
    "  bottom: 'innerprod3' "
    "} ";
const string &expected_output_proto =
    "name: 'TestNetwork' "
    "layer { "
    "  name: 'data' "
    "  type: 'Data' "
    "  top: 'data' "
    "  top: 'label' "
    "} "
    "layer { "
    "  name: 'data_data_0_split' "
    "  type: 'Split' "
    "  bottom: 'data' "
    "  top: 'data_data_0_split_0' "
    "  top: 'data_data_0_split_1' "
    "  top: 'data_data_0_split_2' "
    "} "
    "layer { "
    "  name: 'innerprod1' "
    "  type: 'InnerProduct' "
    "  bottom: 'data_data_0_split_0' "
    "  top: 'innerprod1' "
    "} "
    "layer { "
    "  name: 'innerprod2' "
    "  type: 'InnerProduct' "
    "  bottom: 'data_data_0_split_1' "
    "  top: 'innerprod2' "
    "} "
    "layer { "
    "  name: 'innerprod2_innerprod2_0_split' "
    "  type: 'Split' "
    "  bottom: 'innerprod2' "
    "  top: 'innerprod2_innerprod2_0_split_0' "
    "  top: 'innerprod2_innerprod2_0_split_1' "
    "} "
    "layer { "
    "  name: 'innerprod3' "
    "  type: 'InnerProduct' "
    "  bottom: 'data_data_0_split_2' "
    "  top: 'innerprod3' "
    "} "
    "layer { "
    "  name: 'loss1' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerprod1' "
    "  bottom: 'innerprod2_innerprod2_0_split_0' "
    "} "
    "layer { "
    "  name: 'loss2' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerprod2_innerprod2_0_split_1' "
    "  bottom: 'innerprod3' "
    "} ";
this->
RunInsertionTest(input_proto, expected_output_proto
);
}

TEST_F(SplitLayerInsertionTest, TestInsertionTwoTop
) {
const string &input_proto =
    "name: 'TestNetwork' "
    "layer { "
    "  name: 'data' "
    "  type: 'Data' "
    "  top: 'data' "
    "  top: 'label' "
    "} "
    "layer { "
    "  name: 'innerprod1' "
    "  type: 'InnerProduct' "
    "  bottom: 'data' "
    "  top: 'innerprod1' "
    "} "
    "layer { "
    "  name: 'innerprod2' "
    "  type: 'InnerProduct' "
    "  bottom: 'label' "
    "  top: 'innerprod2' "
    "} "
    "layer { "
    "  name: 'innerprod3' "
    "  type: 'InnerProduct' "
    "  bottom: 'data' "
    "  top: 'innerprod3' "
    "} "
    "layer { "
    "  name: 'innerprod4' "
    "  type: 'InnerProduct' "
    "  bottom: 'label' "
    "  top: 'innerprod4' "
    "} "
    "layer { "
    "  name: 'loss1' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerprod1' "
    "  bottom: 'innerprod3' "
    "} "
    "layer { "
    "  name: 'loss2' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerprod2' "
    "  bottom: 'innerprod4' "
    "} ";
const string &expected_output_proto =
    "name: 'TestNetwork' "
    "layer { "
    "  name: 'data' "
    "  type: 'Data' "
    "  top: 'data' "
    "  top: 'label' "
    "} "
    "layer { "
    "  name: 'data_data_0_split' "
    "  type: 'Split' "
    "  bottom: 'data' "
    "  top: 'data_data_0_split_0' "
    "  top: 'data_data_0_split_1' "
    "} "
    "layer { "
    "  name: 'label_data_1_split' "
    "  type: 'Split' "
    "  bottom: 'label' "
    "  top: 'label_data_1_split_0' "
    "  top: 'label_data_1_split_1' "
    "} "
    "layer { "
    "  name: 'innerprod1' "
    "  type: 'InnerProduct' "
    "  bottom: 'data_data_0_split_0' "
    "  top: 'innerprod1' "
    "} "
    "layer { "
    "  name: 'innerprod2' "
    "  type: 'InnerProduct' "
    "  bottom: 'label_data_1_split_0' "
    "  top: 'innerprod2' "
    "} "
    "layer { "
    "  name: 'innerprod3' "
    "  type: 'InnerProduct' "
    "  bottom: 'data_data_0_split_1' "
    "  top: 'innerprod3' "
    "} "
    "layer { "
    "  name: 'innerprod4' "
    "  type: 'InnerProduct' "
    "  bottom: 'label_data_1_split_1' "
    "  top: 'innerprod4' "
    "} "
    "layer { "
    "  name: 'loss1' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerprod1' "
    "  bottom: 'innerprod3' "
    "} "
    "layer { "
    "  name: 'loss2' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerprod2' "
    "  bottom: 'innerprod4' "
    "} ";
this->
RunInsertionTest(input_proto, expected_output_proto
);
}

TEST_F(SplitLayerInsertionTest, TestWithInPlace
) {
const string &input_proto =
    "name: 'TestNetwork' "
    "layer { "
    "  name: 'data' "
    "  type: 'Data' "
    "  top: 'data' "
    "  top: 'label' "
    "} "
    "layer { "
    "  name: 'innerprod1' "
    "  type: 'InnerProduct' "
    "  bottom: 'data' "
    "  top: 'innerprod1' "
    "} "
    "layer { "
    "  name: 'relu1' "
    "  type: 'ReLU' "
    "  bottom: 'innerprod1' "
    "  top: 'innerprod1' "
    "} "
    "layer { "
    "  name: 'innerprod2' "
    "  type: 'InnerProduct' "
    "  bottom: 'innerprod1' "
    "  top: 'innerprod2' "
    "} "
    "layer { "
    "  name: 'loss1' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerprod1' "
    "  bottom: 'label' "
    "} "
    "layer { "
    "  name: 'loss2' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerprod2' "
    "  bottom: 'data' "
    "} ";
const string &expected_output_proto =
    "name: 'TestNetwork' "
    "layer { "
    "  name: 'data' "
    "  type: 'Data' "
    "  top: 'data' "
    "  top: 'label' "
    "} "
    "layer { "
    "  name: 'data_data_0_split' "
    "  type: 'Split' "
    "  bottom: 'data' "
    "  top: 'data_data_0_split_0' "
    "  top: 'data_data_0_split_1' "
    "} "
    "layer { "
    "  name: 'innerprod1' "
    "  type: 'InnerProduct' "
    "  bottom: 'data_data_0_split_0' "
    "  top: 'innerprod1' "
    "} "
    "layer { "
    "  name: 'relu1' "
    "  type: 'ReLU' "
    "  bottom: 'innerprod1' "
    "  top: 'innerprod1' "
    "} "
    "layer { "
    "  name: 'innerprod1_relu1_0_split' "
    "  type: 'Split' "
    "  bottom: 'innerprod1' "
    "  top: 'innerprod1_relu1_0_split_0' "
    "  top: 'innerprod1_relu1_0_split_1' "
    "} "
    "layer { "
    "  name: 'innerprod2' "
    "  type: 'InnerProduct' "
    "  bottom: 'innerprod1_relu1_0_split_0' "
    "  top: 'innerprod2' "
    "} "
    "layer { "
    "  name: 'loss1' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerprod1_relu1_0_split_1' "
    "  bottom: 'label' "
    "} "
    "layer { "
    "  name: 'loss2' "
    "  type: 'EuclideanLoss' "
    "  bottom: 'innerprod2' "
    "  bottom: 'data_data_0_split_1' "
    "} ";
this->
RunInsertionTest(input_proto, expected_output_proto
);
}

}  // namespace caffe
