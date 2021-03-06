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

#include "net.h"
// #include <android/log.h>

#include <stdio.h>
#include <string.h>

#ifdef _OPENMP
#include <omp.h>
#endif // _OPENMP

namespace ncnn {

Net::Net()
{
}

Net::~Net()
{
    clear();
}

#if NCNN_STRING
int Net::register_custom_layer(const char* type, layer_creator_func creator)
{
    int typeindex = layer_to_index(type);
    if (typeindex != 0)
    {
        fprintf(stderr, "can not register build-in layer type %s\n", type);
        return -1;
    }

    int custom_index = custom_layer_to_index(type);
    if (custom_index == -1)
    {
        struct layer_registry_entry entry = { type, creator };
        custom_layer_registry.push_back(entry);
    }
    else
    {
        fprintf(stderr, "overwrite existing custom layer type %s\n", type);
        custom_layer_registry[custom_index].name = type;
        custom_layer_registry[custom_index].creator = creator;
    }

    return 0;
}
#endif // NCNN_STRING

int Net::register_custom_layer(int index, layer_creator_func creator)
{
    int custom_index = index & ~LayerType::CustomBit;
    if (index == custom_index)
    {
        fprintf(stderr, "can not register build-in layer index %d\n", custom_index);
        return -1;
    }

    if ((int)custom_layer_registry.size() <= custom_index)
    {
#if NCNN_STRING
        struct layer_registry_entry dummy = { "", 0 };
#else
        struct layer_registry_entry dummy = { 0 };
#endif // NCNN_STRING
        custom_layer_registry.resize(custom_index + 1, dummy);
    }

    if (custom_layer_registry[custom_index].creator)
    {
        fprintf(stderr, "overwrite existing custom layer index %d\n", custom_index);
    }

    custom_layer_registry[custom_index].creator = creator;
    return 0;
}

#if NCNN_STDIO
#if NCNN_STRING
int Net::load_param(FILE* fp)
{
    // parse
    int layer_count = 0;
    int blob_count = 0;
    fscanf(fp, "%d %d", &layer_count, &blob_count);

    layers.resize(layer_count);
    blobs.resize(blob_count);

    int layer_index = 0;
    int blob_index = 0;
    while (!feof(fp))
    {
        int nscan = 0;

        char layer_type[256];
        char layer_name[256];
        int bottom_count = 0;
        int top_count = 0;
        nscan = fscanf(fp, "%256s %256s %d %d", layer_type, layer_name, &bottom_count, &top_count);
        if (nscan != 4)
        {
            continue;
        }

        int typeindex = layer_to_index(layer_type);
        Layer* layer = create_layer(typeindex);
        if (!layer)
        {
            typeindex = custom_layer_to_index(layer_type);
            layer = create_custom_layer(typeindex);
        }

        layer->type = std::string(layer_type);
        layer->name = std::string(layer_name);
        // fprintf(stderr, "new layer %d %s\n", layer_index, layer_name);
        // LOGI("new layer %d %s %d\n", layer_index, layer_name, typeindex);

        layer->bottoms.resize(bottom_count);
        for (int i=0; i<bottom_count; i++)
        {
            char bottom_name[256];
            nscan = fscanf(fp, "%256s", bottom_name);
            if (nscan != 1)
            {
                continue;
            }

            int bottom_blob_index = find_blob_index_by_name(bottom_name);
            if (bottom_blob_index == -1)
            {
                Blob& blob = blobs[blob_index];

                bottom_blob_index = blob_index;

                blob.name = std::string(bottom_name);
//                 fprintf(stderr, "new blob %s\n", bottom_name);

                blob_index++;
            }

            Blob& blob = blobs[bottom_blob_index];

            blob.consumers.push_back(layer_index);

            layer->bottoms[i] = bottom_blob_index;
        }

        layer->tops.resize(top_count);
        for (int i=0; i<top_count; i++)
        {
            Blob& blob = blobs[blob_index];

            char blob_name[256];
            nscan = fscanf(fp, "%256s", blob_name);
            if (nscan != 1)
            {
                continue;
            }

            blob.name = std::string(blob_name);
//             fprintf(stderr, "new blob %s\n", blob_name);

            blob.producer = layer_index;

            layer->tops[i] = blob_index;

            blob_index++;
        }

        // layer specific params
        int lr = layer->load_param(fp);
        if (lr != 0)
        {
            fprintf(stderr, "layer load_param failed\n");
            continue;
        }

        layers[layer_index] = layer;

        layer_index++;
    }

    return 0;
}

int Net::load_param(const char* protopath)
{
    FILE* fp = fopen(protopath, "rb");
    if (!fp)
    {
#if NCNN_CNNCACHE
        LOGE("Fail to load fopen param file: %s\n", protopath);
#endif
        fprintf(stderr, "fopen %s failed\n", protopath);
        return -1;
    }

    int ret = load_param(fp);

    fclose(fp);

    return ret;
}
#endif // NCNN_STRING

int Net::load_param_bin(FILE* fp)
{
    int layer_count = 0;
    fread(&layer_count, sizeof(int), 1, fp);

    int blob_count = 0;
    fread(&blob_count, sizeof(int), 1, fp);

    layers.resize(layer_count);
    blobs.resize(blob_count);

    for (int i=0; i<layer_count; i++)
    {
        int typeindex;
        fread(&typeindex, sizeof(int), 1, fp);

        int bottom_count;
        fread(&bottom_count, sizeof(int), 1, fp);

        int top_count;
        fread(&top_count, sizeof(int), 1, fp);

        Layer* layer = create_layer(typeindex);
        if (!layer)
        {
            int custom_index = typeindex & ~LayerType::CustomBit;
            layer = create_custom_layer(custom_index);
        }

//         layer->type = std::string(layer_type);
//         layer->name = std::string(layer_name);
//         fprintf(stderr, "new layer %d\n", typeindex);

        layer->bottoms.resize(bottom_count);
        for (int j=0; j<bottom_count; j++)
        {
            int bottom_blob_index;
            fread(&bottom_blob_index, sizeof(int), 1, fp);

            Blob& blob = blobs[bottom_blob_index];

            blob.consumers.push_back(i);

            layer->bottoms[j] = bottom_blob_index;
        }

        layer->tops.resize(top_count);
        for (int j=0; j<top_count; j++)
        {
            int top_blob_index;
            fread(&top_blob_index, sizeof(int), 1, fp);

            Blob& blob = blobs[top_blob_index];

//             blob.name = std::string(blob_name);
//             fprintf(stderr, "new blob %s\n", blob_name);

            blob.producer = i;

            layer->tops[j] = top_blob_index;
        }

        // layer specific params
        int lr = layer->load_param_bin(fp);
        if (lr != 0)
        {
            fprintf(stderr, "layer load_param failed\n");
            continue;
        }

        layers[i] = layer;
    }

    return 0;
}

int Net::load_param_bin(const char* protopath)
{
    FILE* fp = fopen(protopath, "rb");
    if (!fp)
    {
#if NCNN_CNNCACHE
        LOGE("Fail to load fopen param_bin file: %s\n", protopath);
#endif
        fprintf(stderr, "fopen %s failed\n", protopath);
        return -1;
    }

    int ret = load_param_bin(fp);

    fclose(fp);

    return ret;
}

int Net::load_model(FILE* fp)
{
    // load file
    int ret = 0;

    for (size_t i=0; i<layers.size(); i++)
    {
        Layer* layer = layers[i];

        // LOGI("Load bin layer: %d", i);

        int lret = layer->load_model(fp);
        if (lret != 0)
        {
            fprintf(stderr, "layer load_model %d failed\n", (int)i);
            ret = -1;
            break;
        }
    }

    return ret;
}

int Net::load_model(const char* modelpath)
{
    FILE* fp = fopen(modelpath, "rb");
    if (!fp)
    {
        fprintf(stderr, "fopen %s failed\n", modelpath);
        return -1;
    }

    int ret = load_model(fp);

    fclose(fp);

    return ret;
}
#endif // NCNN_STDIO

int Net::load_param(const unsigned char* _mem)
{
    if ((unsigned long)_mem & 0x3)
    {
        // reject unaligned memory
        fprintf(stderr, "memory not 32-bit aligned at %p\n", _mem);
        return 0;
    }

    const unsigned char* mem = _mem;
    int layer_count = *(int*)(mem);
    mem += 4;

    int blob_count = *(int*)(mem);
    mem += 4;

    layers.resize(layer_count);
    blobs.resize(blob_count);

    for (int i=0; i<layer_count; i++)
    {
        int typeindex = *(int*)mem;
        mem += 4;

        int bottom_count = *(int*)mem;
        mem += 4;

        int top_count = *(int*)mem;
        mem += 4;

        Layer* layer = create_layer(typeindex);
        if (!layer)
        {
            int custom_index = typeindex & ~LayerType::CustomBit;
            layer = create_custom_layer(custom_index);
        }

//         layer->type = std::string(layer_type);
//         layer->name = std::string(layer_name);
#if NCNN_CNNCACHE
        // LOGI("new layer %d\n", typeindex);
#endif

        layer->bottoms.resize(bottom_count);
        for (int j=0; j<bottom_count; j++)
        {
            int bottom_blob_index = *(int*)mem;
            mem += 4;

            Blob& blob = blobs[bottom_blob_index];

            blob.consumers.push_back(i);

            layer->bottoms[j] = bottom_blob_index;
        }

        layer->tops.resize(top_count);
        for (int j=0; j<top_count; j++)
        {
            int top_blob_index = *(int*)mem;
            mem += 4;

            Blob& blob = blobs[top_blob_index];

//             blob.name = std::string(blob_name);
//             fprintf(stderr, "new blob %s\n", blob_name);

            blob.producer = i;

            layer->tops[j] = top_blob_index;
        }

        // layer specific params
        int lr = layer->load_param(mem);
        if (lr != 0)
        {
            fprintf(stderr, "layer load_param failed\n");
            continue;
        }

        layers[i] = layer;
    }

    return mem - _mem;
}

int Net::load_model(const unsigned char* _mem)
{
    if ((unsigned long)_mem & 0x3)
    {
        // reject unaligned memory
        fprintf(stderr, "memory not 32-bit aligned at %p\n", _mem);
        return 0;
    }

    const unsigned char* mem = _mem;
    for (size_t i=0; i<layers.size(); i++)
    {
        Layer* layer = layers[i];

        int lret = layer->load_model(mem);
        if (lret != 0)
        {
            fprintf(stderr, "layer load_model failed\n");
            return -1;
        }
    }

    return mem - _mem;
}

void Net::clear()
{
    blobs.clear();
    for (size_t i=0; i<layers.size(); i++)
    {
        delete layers[i];
    }
    layers.clear();
}

Extractor Net::create_extractor() const
{
    return Extractor(this, blobs.size());
}

#if NCNN_STRING
int Net::find_blob_index_by_name(const char* name) const
{
    for (size_t i=0; i<blobs.size(); i++)
    {
        const Blob& blob = blobs[i];
        // LOGI("name %s %s\n", blob.name.c_str(), name);
        if (blob.name == name)
        {
            return i;
        }
    }

    fprintf(stderr, "find_blob_index_by_name %s failed\n", name);
    return -1;
}

int Net::find_layer_index_by_name(const char* name) const
{
    for (size_t i=0; i<layers.size(); i++)
    {
        const Layer* layer = layers[i];
        if (layer->name == name)
        {
            return i;
        }
    }

    fprintf(stderr, "find_layer_index_by_name %s failed\n", name);
    return -1;
}

int Net::custom_layer_to_index(const char* type)
{
    const int custom_layer_registry_entry_count = custom_layer_registry.size();
    for (int i=0; i<custom_layer_registry_entry_count; i++)
    {
        if (strcmp(type, custom_layer_registry[i].name) == 0)
        {
            return i;
        }
    }

    fprintf(stderr, "custom layer %s not exists\n", type);
    return -1;
}
#endif // NCNN_STRING

Layer* Net::create_custom_layer(int index)
{
    const int custom_layer_registry_entry_count = custom_layer_registry.size();
    if (index < 0 || index >= custom_layer_registry_entry_count)
    {
        fprintf(stderr, "custom layer index %d not exists\n", index);
        return 0;
    }

    layer_creator_func layer_creator = custom_layer_registry[index].creator;
    return layer_creator();
}

int Net::forward_layer(int layer_index, Extractor* extractor) const
{
    bool lightmode = extractor->lightmode;
    std::vector<Mat>& blob_mats = extractor->blob_mats;
    const Layer* layer = layers[layer_index];
    int ret;

// #if NCNN_CNNCACHE
//     LOGI("Net::forward_layer index: %d name: %s type: %s\n",
//         layer_index, layer->name.c_str(), layer->type.c_str());
// #endif
    // fprintf(stderr, "forward_layer %d %s\n", layer_index, layer->name.c_str());

    if (layer->one_blob_only)
    {
        // load bottom blob
        int bottom_blob_index = layer->bottoms[0];
        int top_blob_index = layer->tops[0];

        if (blob_mats[bottom_blob_index].dims == 0)
        {
            ret = forward_layer(blobs[bottom_blob_index].producer, extractor);
            if (ret != 0)
                return ret;
        }

        // LOGI("Net::forward_layer index: %d name: %s type: %s\n",
        //     layer_index, layer->name.c_str(), layer->type.c_str());

        Mat bottom_blob = blob_mats[bottom_blob_index];

        if (lightmode)
        {
            // delete after taken in light mode
            blob_mats[bottom_blob_index].release();
            // deep copy for inplace forward if data is shared
            if (layer->support_inplace && *bottom_blob.refcount != 1)
            {
                bottom_blob = bottom_blob.clone();
            }
        }

#if NCNN_CNNCACHE
        ret = layer->forward_mrect(
            extractor->matched_rects[bottom_blob_index],
            extractor->matched_rects[top_blob_index]);
        if (ret != 0)
            LOGE("Failing forward_mrect: %d", ret);
#endif

        // forward
        if (lightmode && layer->support_inplace)
        {
            Mat& bottom_top_blob = bottom_blob;
            ret = layer->forward_inplace(bottom_top_blob);
            if (ret != 0)
                return ret;

            // store top blob
            blob_mats[top_blob_index] = bottom_top_blob;
        }
        else
        {
            Mat top_blob;
#if NCNN_CNNCACHE
            // TODO: we should add this every place forward func is called but
            // conv is one_blob_only and has no light impl it's enough we impl here
            if (extractor->cache_mode) {
                ret = layer->forward_cached(bottom_blob, top_blob,
                    extractor->matched_rects[top_blob_index],
                    extractor->blob_mats_cached[layer_index]);
            }
            else {
                ret = layer->forward(bottom_blob, top_blob);
            }
#else
            ret = layer->forward(bottom_blob, top_blob);
#endif
            if (ret != 0)
                return ret;

            // store top blob
            blob_mats[top_blob_index] = top_blob;
        }

    }
    else
    {
        // load bottom blobs
        std::vector<Mat> bottom_blobs;
        bottom_blobs.resize(layer->bottoms.size());
#if NCNN_CNNCACHE
        std::vector<MRect> bottom_mrects;
        bottom_mrects.resize(layer->bottoms.size());
#endif
        for (size_t i=0; i<layer->bottoms.size(); i++)
        {
            int bottom_blob_index = layer->bottoms[i];

            if (blob_mats[bottom_blob_index].dims == 0)
            {
                ret = forward_layer(blobs[bottom_blob_index].producer, extractor);
                if (ret != 0)
                    return ret;
            }

            bottom_blobs[i] = blob_mats[bottom_blob_index];

#if NCNN_CNNCACHE
            bottom_mrects[i].copyFrom(extractor->matched_rects[bottom_blob_index]);
#endif
            if (lightmode)
            {
                // delete after taken in light mode
                blob_mats[bottom_blob_index].release();
                // deep copy for inplace forward if data is shared
                if (layer->support_inplace && *bottom_blobs[i].refcount != 1)
                {
                    bottom_blobs[i] = bottom_blobs[i].clone();
                }
            }
        }

#if NCNN_CNNCACHE
        std::vector<MRect> top_mrects;
        top_mrects.resize(layer->tops.size());
        ret = layer->forward_mrect(bottom_mrects, top_mrects);
        if (ret != 0)
            LOGE("Failing forward_mrect: %d", ret);
        for (size_t i=0; i<layer->tops.size(); i++)
        {
            int top_blob_index = layer->tops[i];
            extractor->matched_rects[top_blob_index].copyFrom(top_mrects[i]);
        }
#endif

        // forward
        if (lightmode && layer->support_inplace)
        {
            std::vector<Mat>& bottom_top_blobs = bottom_blobs;
            ret = layer->forward_inplace(bottom_top_blobs);
            if (ret != 0)
                return ret;

            // store top blobs
            for (size_t i=0; i<layer->tops.size(); i++)
            {
                int top_blob_index = layer->tops[i];

                blob_mats[top_blob_index] = bottom_top_blobs[i];
            }
        }
        else
        {
            std::vector<Mat> top_blobs;
            top_blobs.resize(layer->tops.size());
            ret = layer->forward(bottom_blobs, top_blobs);
            if (ret != 0)
                return ret;

            // store top blobs
            for (size_t i=0; i<layer->tops.size(); i++)
            {
                int top_blob_index = layer->tops[i];

                blob_mats[top_blob_index] = top_blobs[i];
            }
        }
    }

//     fprintf(stderr, "forward_layer %d %s done\n", layer_index, layer->name.c_str());
//     const Mat& blob = blob_mats[layer->tops[0]];
//     fprintf(stderr, "[%-2d %-16s %-16s]  %d    blobs count = %-3d   size = %-3d x %-3d\n", layer_index, layer->type.c_str(), layer->name.c_str(), layer->tops[0], blob.c, blob.h, blob.w);

    return 0;
}

Extractor::Extractor(const Net* _net, int blob_count) : net(_net)
{
    blob_mats.resize(blob_count);
    lightmode = false;
    num_threads = 0;
#if NCNN_CNNCACHE
    blob_mats_cached.resize(net->layers.size());
    matched_rects.resize(blob_count);
    cache_mode = true;
#endif
}

void Extractor::set_light_mode(bool enable)
{
    lightmode = enable;
}

void Extractor::set_num_threads(int _num_threads)
{
    num_threads = _num_threads;
}

int Extractor::input(int blob_index, const Mat& in)
{
    if (blob_index < 0 || blob_index >= (int)blob_mats.size())
        return -1;

    blob_mats[blob_index] = in;

    return 0;
}

#if NCNN_CNNCACHE
int Extractor::input_mrect(int blob_index, MRect& mrect)
{
    if (blob_index < 0 || blob_index >= (int)blob_mats.size())
        return -1;

    matched_rects[blob_index] = mrect;

    return 0;
}
int Extractor::input_mrect(const char* blob_name, MRect& mrect)
{
    int blob_index = net->find_blob_index_by_name(blob_name);
    if (blob_index < 0 || blob_index >= (int)blob_mats.size())
        return -1;

    matched_rects[blob_index] = mrect;

    return 0;
}
int Extractor::clear_blob_data()
{
    // int total_size = 0;
    for (Mat& mat : blob_mats) {
        // total_size += mat.total();
        mat.release();
    }
    // LOGI("TOTAL_SIZE: %d", total_size);
    return 0;
}
int Extractor::update_cnncache()
{
    // int cached_size = 0;
    // struct timeval tv_begin, tv_end;
    // gettimeofday(&tv_begin, NULL);
    for (size_t i = 0, max = net->layers.size(); i < max; i ++) {
        Layer* layer = net->layers[i];
        if (layer->needs_cache()) {
            Mat& cache_blob = blob_mats_cached[i];
            int top_blob_index = layer->tops[0];
            Mat& top_blob = blob_mats[top_blob_index];
            // LOGI("PPP %p %p", top_blob.data, cache_blob.data);
            cache_blob.cloneFrom(top_blob);
            // cached_size += top_blob.total();
        }
    }
    // LOGI("CACHE_SIZE: %d", cached_size);
    // gettimeofday(&tv_end, NULL);
    // int elapsed = ((tv_end.tv_sec - tv_begin.tv_sec) * 1000000.0f + tv_end.tv_usec - tv_begin.tv_usec) / 1000.0f;
    // LOGI("update_cnncache elapsed: %d", elapsed);
    return 0;
}
int Extractor::clear_cnncache()
{
    for (Mat& mat : blob_mats_cached)
        mat.release();
    return 0;
}
#endif

int Extractor::extract(int blob_index, Mat& feat)
{
    log_time_end("__reset");
    if (blob_index < 0 || blob_index >= (int)blob_mats.size())
        return -1;

    int ret = 0;

    if (blob_mats[blob_index].dims == 0)
    {
        int layer_index = net->blobs[blob_index].producer;

#ifdef _OPENMP
        int dynamic_current = 0;
        int num_threads_current = 1;
        if (num_threads)
        {
            dynamic_current = omp_get_dynamic();
            num_threads_current = omp_get_num_threads();
            omp_set_dynamic(0);
            omp_set_num_threads(num_threads);
        }
#endif

        ret = net->forward_layer(layer_index, this);

#ifdef _OPENMP
        if (num_threads)
        {
            omp_set_dynamic(dynamic_current);
            omp_set_num_threads(num_threads_current);
        }
#endif
    }

    feat = blob_mats[blob_index];

    return ret;
}

#if NCNN_STRING
int Extractor::input(const char* blob_name, const Mat& in)
{
    int blob_index = net->find_blob_index_by_name(blob_name);
    if (blob_index == -1)
        return -1;

    blob_mats[blob_index] = in;

    return 0;
}

int Extractor::extract(const char* blob_name, Mat& feat)
{
    int blob_index = net->find_blob_index_by_name(blob_name);
    return extract(blob_index, feat);
}
#endif // NCNN_STRING

} // namespace ncnn
