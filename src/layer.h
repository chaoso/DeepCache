// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2017 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef NCNN_LAYER_H
#define NCNN_LAYER_H

#if NCNN_CNNCACHE
#include <android/log.h>
#endif

#include <stdio.h>
#include <string>
#include <vector>
#include "mat.h"
#include "platform.h"
#include "mrect.h"

namespace ncnn {

class Layer
{
public:
    // empty
    Layer();
    // virtual destructor
    virtual ~Layer();

#if NCNN_STDIO
#if NCNN_STRING
    // load layer specific parameter from plain param file
    // return 0 if success
    virtual int load_param(FILE* paramfp);
#endif // NCNN_STRING
    // load layer specific parameter from binary param file
    // return 0 if success
    virtual int load_param_bin(FILE* paramfp);

    // load layer specific weight data from model file
    // return 0 if success
    virtual int load_model(FILE* binfp);
#endif // NCNN_STDIO

    // load layer specific parameter from memory
    // memory pointer is 32-bit aligned
    // return 0 if success
    virtual int load_param(const unsigned char*& mem);

    // load layer specific weight data from memory
    // memory pointer is 32-bit aligned
    // return 0 if success
    virtual int load_model(const unsigned char*& mem);

public:
    // one input and one output blob
    bool one_blob_only;

    // support inplace inference
    bool support_inplace;

public:
    // implement inference
    // return 0 if success
    virtual int forward(const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs) const;
    virtual int forward(const Mat& bottom_blob, Mat& top_blob) const;

    // implement inplace inference
    // return 0 if success
    virtual int forward_inplace(std::vector<Mat>& bottom_top_blobs) const;
    virtual int forward_inplace(Mat& bottom_top_blob) const;

#if NCNN_CNNCACHE
    virtual int forward_mrect(std::vector<MRect>& bottom_mrects, std::vector<MRect>& top_mrects) const;
    virtual int forward_mrect(MRect& bottom_mrect, MRect& top_mrect) const;
    virtual int forward_cached(const Mat& bottom_blob, Mat& top_blob, MRect& mrect, Mat& cached_blob) const;
    virtual bool needs_cache() const {return false;}
#endif

public:
#if NCNN_STRING
    // layer type name
    std::string type;
    // layer name
    std::string name;
#endif // NCNN_STRING
    // blob index which this layer needs as input
    std::vector<int> bottoms;
    // blob index which this layer produces as output
    std::vector<int> tops;
};

namespace LayerType {
enum
{
    AbsVal      = 0,
    ArgMax      = 1,
    BatchNorm   = 2,
    Bias        = 3,
    BNLL        = 4,
    Concat      = 5,
    Convolution = 6,
    Crop        = 7,
    Deconvolution = 8,
    Dropout     = 9,
    ELU         = 10,
    Eltwise     = 11,
    Embed       = 12,
    Exp         = 13,
    Flatten     = 14,
    InnerProduct = 15,
    Input       = 16,
    Log         = 17,
    LRN         = 18,
    MemoryData  = 19,
    MVN         = 20,
    Pooling     = 21,
    Power       = 22,
    PReLU       = 23,
    Proposal    = 24,
    Reduction   = 25,
    ReLU        = 26,
    Reshape     = 27,
    ROIPooling  = 28,
    Scale       = 29,
    Sigmoid     = 30,
    Slice       = 31,
    Softmax     = 32,
    Split       = 33,
    SPP         = 34,
    TanH        = 35,
    Threshold   = 36,
    Tile        = 37,
    RNN         = 38,
    LSTM        = 39,
    BinaryOp    = 40,
    UnaryOp     = 41,
    ConvolutionDepthWise = 42,

    CustomBit   = (1<<8),
};
} // namespace LayerType

// layer factory function
typedef Layer* (*layer_creator_func)();

struct layer_registry_entry
{
#if NCNN_STRING
    // layer type name
    const char* name;
#endif // NCNN_STRING
    // layer factory entry
    layer_creator_func creator;
};

#if NCNN_STRING
// get layer type from type name
int layer_to_index(const char* type);
#endif // NCNN_STRING
// create layer from layer type
Layer* create_layer(int index);

#define DEFINE_LAYER_CREATOR(name) \
    Layer* name##_layer_creator() { return new name; }

} // namespace ncnn

#endif // NCNN_LAYER_H
