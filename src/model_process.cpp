/**
* @file model_process.cpp
*
* Copyright (C) 2020. Huawei Technologies Co., Ltd. All rights reserved.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/
#include "model_process.h"
#include <map>                   // 标准库路径下来寻找，查看改头文件  以及指定的第三方库的头文件
#include <sstream>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <functional>
#include "acl/acl_mdl.h"                        //是在包含用户自定义的文件，但是也包含非标准路径下的标准库和第三方文件
#include <opencv2/opencv.hpp>
 
using namespace std                 //不需要直接使用这个std指定，
extern bool g_isDevice;      //这个就是一个其他程序中用过的变量，来进行先声明，再定义
extern uint32_t g_modelWidth;
extern uint32_t g_modelHeight;

typedef struct DataInfo {
    void *data;
    size_t size;
} DataInfo;                         //结构体的定义  顺便创建一个结构体变量

ModelProcess::ModelProcess()                //创建实例
    : modelId_(0), modelWorkPtr_(nullptr), modelWorkSize_(0), modelWeightPtr_(nullptr), modelWeightSize_(0),
      loadFlag_(false), modelDesc_(nullptr), input_(nullptr), output_(nullptr)
{
    kGridSizeX_.push_back(g_modelWidth / 8);
    kGridSizeX_.push_back(g_modelWidth / 16);
    kGridSizeX_.push_back(g_modelWidth / 32);
    kGridSizeY_.push_back(g_modelHeight / 8);
    kGridSizeY_.push_back(g_modelHeight / 16);
    kGridSizeY_.push_back(g_modelHeight / 32);
    //std::vector<uint32_t> kGridSizeX_{g_modelWidth / 8, g_modelWidth / 16, g_modelWidth / 32};
    //std::vector<uint32_t> kGridSizeY_{g_modelHeight/ 8, g_modelHeight / 16, g_modelHeight / 32};

    //int m_anchor[3][3][2];// = {{{10,13},{21,16},{14,31}}, {{37,26},{25,59},{61,43}}, {{52,124},{100,68},{168,145}}};
    //float anchors[32] = {10, 13, 16, 30, 33, 23, 30, 61, 62, 45, 59, 119, 116, 90, 156, 198, 373, 326};
    float anchors[32] = {5, 3, 7, 5, 10, 7, 15, 9, 18, 12, 25, 17, 47, 31, 92, 55, 155, 134};  // 可以使用32个浮点数，但是只有17
    if (anchors != NULL)
    {
        for (size_t i = 0; i < sizeof(m_anchor) / sizeof(int); i++)
        {
            m_anchor[i / 6][(i / 2) % 3][i % 2] = (int)anchors[i];
        }
    }
}

ModelProcess::~ModelProcess()     //析构函数，来释放内存
{
    UnloadModel();
    DestroyDesc();
    DestroyInput();
    DestroyOutput();
}

Result ModelProcess::LoadModel(const char *modelPath)     //查找类中的函数，或者是指定这个函数为类下的函数。类中 成员函数指定路径下的模型是否被正确加载
{
    if (modelPath == nullptr) {
        ERROR_LOG("modelPath is empty.");                //查询模型路径是否为空， 反复用一个内存空间来返回这些查询的值
        return FAILED;
    }

    if (loadFlag_) {                                         //查询是否已经被加载，如果加载的话，就不再进行加载，标志位
        ERROR_LOG("model has already been loaded.");
        return FAILED;
    }

    aclError ret = aclmdlQuerySize(modelPath, &modelWorkSize_, &modelWeightSize_);    //查询模型需要分配的内存空间
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("query model failed, model file is %s, errorCode = %d.",
            modelPath, static_cast<int32_t>(ret));
        return FAILED;
    }

    // using ACL_MEM_MALLOC_HUGE_FIRST to malloc memory, huge memory is preferred to use
    // and huge memory can improve performance.
    ret = aclrtMalloc(&modelWorkPtr_, modelWorkSize_, ACL_MEM_MALLOC_HUGE_FIRST);        //查询分配的运行空间是否成功
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("malloc buffer for work failed, require size is %zu, errorCode = %d.",
            modelWorkSize_, static_cast<int32_t>(ret));
        return FAILED;
    }

    // using ACL_MEM_MALLOC_HUGE_FIRST to malloc memory, huge memory is preferred to use
    // and huge memory can improve performance.
    ret = aclrtMalloc(&modelWeightPtr_, modelWeightSize_, ACL_MEM_MALLOC_HUGE_FIRST);    //查询分配的权重内存是否成功
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("malloc buffer for weight failed, require size is %zu, errorCode = %d.",
            modelWeightSize_, static_cast<int32_t>(ret));
        (void)aclrtFree(modelWorkPtr_);
        modelWorkPtr_ = nullptr;
        modelWorkSize_ = 0;
        return FAILED;
    }

    ret = aclmdlLoadFromFileWithMem(modelPath, &modelId_, modelWorkPtr_,                   //查询模型是否写入
        modelWorkSize_, modelWeightPtr_, modelWeightSize_);
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("load model from file failed, model file is %s, errorCode = %d.",
            modelPath,  static_cast<int32_t>(ret));

        (void)aclrtFree(modelWorkPtr_);     //将数据类型转成void ，忽略这个返回值
        modelWorkPtr_ = nullptr;
        modelWorkSize_ = 0;

        (void)aclrtFree(modelWeightPtr_);
        modelWeightPtr_ = nullptr;
        modelWeightSize_ = 0;

        return FAILED;
    }

    loadFlag_ = true;                                                                //模型写入
    INFO_LOG("load model %s success.", modelPath);                                    //加载成功

    return SUCCESS;
}

void ModelProcess::UnloadModel()
{
    if (!loadFlag_) {
        WARN_LOG("no model had been loaded.");
        return;
    }

    aclError ret = aclmdlUnload(modelId_);
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("unload model failed, modelId is %u, errorCode = %d.",
            modelId_, static_cast<int32_t>(ret));
    }

    if (modelWorkPtr_ != nullptr) {
        (void)aclrtFree(modelWorkPtr_);       //释放之前分配的内存和缓存
        modelWorkPtr_ = nullptr;
        modelWorkSize_ = 0;
    }

    if (modelWeightPtr_ != nullptr) {
        (void)aclrtFree(modelWeightPtr_);
        modelWeightPtr_ = nullptr;
        modelWeightSize_ = 0;
    }

    loadFlag_ = false;                  //模型加载变为一个没有加载的
    INFO_LOG("unload model success, modelId is %u.", modelId_);
}

Result ModelProcess::CreateDesc()
{
    modelDesc_ = aclmdlCreateDesc();     //描述符是一个用于存储模型信息的结构，如模型的输入和输出信息、模型的配置参数等。如果创建描述符失败
    if (modelDesc_ == nullptr) {
        ERROR_LOG("create model description failed.");
        return FAILED;
    }

    aclError ret = aclmdlGetDesc(modelDesc_, modelId_); //使用modelId_来填充modelDesc_中存储的模型描述信息
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("get model description failed, modelId_ = %u, errorCode = %d.",
            modelId_, static_cast<int32_t>(ret));
        return FAILED;
    }

    INFO_LOG("create model description success.");         //创建模型描述成功

    return SUCCESS;
} //确保模型的描述信息被正确获取，为后续的模型输入输出配置
  //和模型执行做好准备。描述符的创建和填充是模型推理过程中的重要环节，它确保了模型可以被正确地执行并处理输入数据。

void ModelProcess::DestroyDesc()         // 销毁释放描述子
{
    if (modelDesc_ != nullptr) {
        (void)aclmdlDestroyDesc(modelDesc_);
        modelDesc_ = nullptr;
    }
    INFO_LOG("destroy model description success.");
}

Result ModelProcess::CreateInput(const ImageMemoryInfo &imageMemInfo)
{
    if ((imageMemInfo.imageDataBuf == nullptr) || (imageMemInfo.imageInfoBuf == nullptr)) {
        ERROR_LOG("input image memory is nullptr.");
        return FAILED;
    }

    const size_t mdlInputNum = aclmdlGetNumInputs(modelDesc_);
    // dynamic batch yolov3 has three inputs, two are data input, one is dynamic input
    if (mdlInputNum != 3) {
        ERROR_LOG("the input number of dynamic batch yolov3 must be 3.");
        return FAILED;
    }
    size_t dynamicIdx = 0;
    auto ret = aclmdlGetInputIndexByName(modelDesc_, ACL_DYNAMIC_TENSOR_NAME, &dynamicIdx);
    if ((ret != ACL_SUCCESS) || (dynamicIdx != (mdlInputNum - 1))) {
        ERROR_LOG("get input index by name failed, dynamicIdx = %zu, errorCode = %d.",
            dynamicIdx, static_cast<int32_t>(ret));
        return FAILED;
    }
    size_t dataLen = aclmdlGetInputSizeByIndex(modelDesc_, dynamicIdx);  // 获取动态输入的大小，并为动态数据分配GPU
    void *data = nullptr;  // 空指针的时候
    ret = aclrtMalloc(&data, dataLen, ACL_MEM_MALLOC_HUGE_FIRST);
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("malloc device memory failed, errorCode = %d.", static_cast<int32_t>(ret));
        return FAILED;
    }

    // the first two inputs of yolov3 model are data input, the third input is dynamic input
    DataInfo inputDataInfo[mdlInputNum] = {{imageMemInfo.imageDataBuf, imageMemInfo.imageDataSize},
        {imageMemInfo.imageInfoBuf, imageMemInfo.imageInfoSize},
        {data, dataLen}};                     // 创建一个数组来存储模型的数据输入信息，包括图像数据，图像信息和动态输入的指针和大小

    input_ = aclmdlCreateDataset()              //创建一个数据集来容纳所有数据
    if (input_ == nullptr) {
        ERROR_LOG("can't create dataset, create input failed.");
        aclrtFree(data);
        data = nullptr;
        return FAILED;
    }

    for (size_t index = 0; index < mdlInputNum; ++index) {
        aclDataBuffer *inputData = aclCreateDataBuffer(inputDataInfo[index].data, inputDataInfo[index].size);
        if (inputData == nullptr) {
            ERROR_LOG("can't create data buffer, create input failed.");
            (void)aclrtFree(inputDataInfo[index].data);
            inputDataInfo[index].data = nullptr;
            return FAILED;
        }

        aclError ret = aclmdlAddDatasetBuffer(input_, inputData);
        if (ret != ACL_SUCCESS) {
            ERROR_LOG("add input dataset buffer failed, errorCode = %d.", static_cast<int32_t>(ret));
            aclDestroyDataBuffer(inputData);
            inputData = nullptr;
            (void)aclrtFree(inputDataInfo[index].data);
            inputDataInfo[index].data = nullptr;
            return FAILED;
        }   // 每个模型的输入数据，都被正确的封装并添加在数据集里，以便模型能够正确的读取和处理这些输入数据
    }
    INFO_LOG("create model input success.");

    return SUCCESS;
}

void ModelProcess::DestroyInput()
{
    if (input_ == nullptr) {
        return;
    }    // 如果已经是空的指针的话 ，那么就返回，执行成功

    size_t bufNum = aclmdlGetDatasetNumBuffers(input_);   //获取数据集input_缓冲区数量
    for (size_t i = 0; i < bufNum; ++i) {
        aclDataBuffer *dataBuffer = aclmdlGetDatasetBuffer(input_, i);
        if (dataBuffer == nullptr){
            continue;
        }
        void *data = aclGetDataBufferAddr(dataBuffer);
        if (data == nullptr){
            (void)aclDestroyDataBuffer(dataBuffer);
            continue;
        }
        (void)aclrtFree(data);
        data = nullptr;
        (void)aclDestroyDataBuffer(dataBuffer);
        dataBuffer = nullptr;
    }
    (void)aclmdlDestroyDataset(input_);             //销毁数据集
    input_ = nullptr;
    INFO_LOG("destroy model input success.");
}

Result ModelProcess::CreateOutput()
{
    if (modelDesc_ == nullptr) {
        ERROR_LOG("no model description, create ouput failed.");
        return FAILED;
    }

    output_ = aclmdlCreateDataset();          //创建一个输出数据集
    if (output_ == nullptr) {
        ERROR_LOG("can't create dataset, create output failed.");
        return FAILED;
    }

    size_t outputSize = aclmdlGetNumOutputs(modelDesc_);     //模型的数量和获取输出数据的大小
    for (size_t i = 0; i < outputSize; ++i) {
        size_t buffer_size = aclmdlGetOutputSizeByIndex(modelDesc_, i);

        void *outputBuffer = nullptr;
        aclError ret = aclrtMalloc(&outputBuffer, buffer_size, ACL_MEM_MALLOC_NORMAL_ONLY);
        if (ret != ACL_SUCCESS) {
            ERROR_LOG("can't malloc buffer, size is %zu, create output failed, errorCode = %d.",
                buffer_size, static_cast<int32_t>(ret));
            return FAILED;
        }

        aclDataBuffer* outputData = aclCreateDataBuffer(outputBuffer, buffer_size);     //创建一个输出缓存区
        if (outputData == nullptr) {
            ERROR_LOG("can't create data buffer, create output failed.");
            (void)aclrtFree(outputBuffer);
            outputBuffer = nullptr;
            return FAILED;
        }

        ret = aclmdlAddDatasetBuffer(output_, outputData);              //将数据添加到内存中
        if (ret != ACL_SUCCESS) {
            ERROR_LOG("can't add data buffer, create output failed, errorCode = %d.", static_cast<int32_t>(ret));
            (void)aclrtFree(outputBuffer);
            outputBuffer = nullptr;
            (void)aclDestroyDataBuffer(outputData);
            outputData = nullptr;
            return FAILED;
        }
    }

    INFO_LOG("create model output success.");
    return SUCCESS;
}  // 创建输出数据集的内存

void ModelProcess::DestroyOutput()
{
    if (output_ == nullptr) {
        return;
    }

    size_t outputNum = aclmdlGetDatasetNumBuffers(output_);
    for (size_t i = 0; i < outputNum; ++i) {
        aclDataBuffer* dataBuffer = aclmdlGetDatasetBuffer(output_, i);
        if (dataBuffer == nullptr){
            continue;
        }

        void* data = aclGetDataBufferAddr(dataBuffer);
        if (data == nullptr){
            (void)aclDestroyDataBuffer(dataBuffer);
            continue;
        }

        (void)aclrtFree(data);
        data = nullptr;
        (void)aclDestroyDataBuffer(dataBuffer);
        dataBuffer = nullptr;
    }

    (void)aclmdlDestroyDataset(output_);
    output_ = nullptr;
    INFO_LOG("destroy model output success.");
}

Result ModelProcess::SetDynamicBatchSize(uint64_t batchSize)    //为动态批处理模型设置批大小
{
    size_t index;    //获取动态输入索引
    aclError ret = aclmdlGetInputIndexByName(modelDesc_, ACL_DYNAMIC_TENSOR_NAME, &index);   //输入三个指针  接口同步
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("get input index by name[%s] failed, errorCode = %d.",
            ACL_DYNAMIC_TENSOR_NAME, static_cast<int32_t>(ret));
        return FAILED;
    }

    ret = aclmdlSetDynamicBatchSize(modelId_, input_, index, batchSize);    //设置动态批大小  ，这个是初始话
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("set dynamic batch size[%lu] failed, errorCode = %d.",
            batchSize, static_cast<int32_t>(ret));
        return FAILED;
    }

    return SUCCESS;
}

Result ModelProcess::SetDynamicHWSize(uint64_t height, uint64_t width)  //设置动态分辨率的情况下宽和高的设置
{
    size_t index;
    aclError ret = aclmdlGetInputIndexByName(modelDesc_, ACL_DYNAMIC_TENSOR_NAME, &index);
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("get input index by name[%s] failed, errorCode = %d.",
            ACL_DYNAMIC_TENSOR_NAME, static_cast<int32_t>(ret));
        return FAILED;
    }

    ret = aclmdlSetDynamicHWSize(modelId_, input_, index, height, width);
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("set dynamic hw[%lu, %lu] failed, errorCode = %d.",
            height, width, static_cast<int32_t>(ret));
        return FAILED;
    }

    return SUCCESS;
}

Result ModelProcess::SetDynamicSize(const DynamicInfo &dynamicInfo)
{
    Result ret = SUCCESS;
    DynamicType dynamicType = dynamicInfo.dynamicType;
    if (dynamicType == DYNAMIC_BATCH) {
        ret = SetDynamicBatchSize(dynamicInfo.dynamicArr[0]);
        if (ret != SUCCESS) {
            return FAILED;
        }
        INFO_LOG("set dynamic batch size[%lu] success.", dynamicInfo.dynamicArr[0]);
    } else if (dynamicType == DYNAMIC_HW) {
        ret = SetDynamicHWSize(dynamicInfo.dynamicArr[0], dynamicInfo.dynamicArr[1]);
        if (ret != SUCCESS) {
            return FAILED;
        }
        INFO_LOG("set dynamic hw[%lu, %lu] success.", dynamicInfo.dynamicArr[0], dynamicInfo.dynamicArr[1]);
    } else {
        ERROR_LOG("invalid dynamic type %d.", static_cast<int32_t>(dynamicType));
        return FAILED;
    }

    return SUCCESS;   //设置一个动态的尺寸
}

Result ModelProcess::Execute()            //执行推理
{
    aclError ret = aclmdlExecute(modelId_, input_, output_);  //输入，输出，模型   调用模型推理接口，将输入数据送入模型进行计算，保存在输出数中
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("execute model failed, modelId is %u, errorCode = %d.",
            modelId_, static_cast<int32_t>(ret));
        return FAILED;
    }

    INFO_LOG("model execute success.");
    return SUCCESS;
}

void ModelProcess::DumpModelOutputResult()   //将模型推理的输出结果保存在二进制文件中，用于调试，验证以及后续处理，
{
    if (output_ == nullptr) {
        ERROR_LOG("output is empty.");
        return;
    }

    size_t outputNum = aclmdlGetDatasetNumBuffers(output_);      //获取数据数量缓存，在这个输出内存
    static int executeNum = 0;                         //定义一个静态变量，用于记录模型的执行次数，每执行一次推理，就会递增

    for (size_t i = 0; i < outputNum; ++i) {          //遍历输出数据，并保存到文件
        stringstream ss;
        ss << "output" << ++executeNum << "_" << i << ".bin";
        string outputFileName = ss.str();
        FILE *outputFile = fopen(outputFileName.c_str(), "wb");
        if (outputFile != nullptr) {
            aclDataBuffer* dataBuffer = aclmdlGetDatasetBuffer(output_, i);
            if (dataBuffer == nullptr){
                ERROR_LOG("output data buffer is empty! The seq is %zu.", i);
                fclose(outputFile);
                continue;
            }

            void* data = aclGetDataBufferAddr(dataBuffer);
            if (data == nullptr){
                ERROR_LOG("output data is empty! The seq is %zu.", i);
                fclose(outputFile);
                continue;
            }

            size_t len = aclGetDataBufferSizeV2(dataBuffer);

            void* outHostData = nullptr;
            aclError ret = ACL_SUCCESS;
            if (!g_isDevice) {                       //这个当时一个false,就是执行一个主机模型的操作
                ret = aclrtMallocHost(&outHostData, len);
                if (ret != ACL_SUCCESS) {
                    ERROR_LOG("aclrtMallocHost failed, errorCode = %d.", static_cast<int32_t>(ret));
                    fclose(outputFile);
                    outputFile = nullptr;
                    return;
                }

                ret = aclrtMemcpy(outHostData, len, data, len, ACL_MEMCPY_DEVICE_TO_HOST);
                if (ret != ACL_SUCCESS) {
                    ERROR_LOG("aclrtMemcpy failed, errorCode = %d.", static_cast<int32_t>(ret));
                    (void)aclrtFreeHost(outHostData);
                    outHostData = nullptr;
                    fclose(outputFile);
                    outputFile = nullptr;
                    return;
                }
                fwrite(outHostData, len, sizeof(char), outputFile);
                ret = aclrtFreeHost(outHostData);
                outHostData = nullptr;
                if (ret != ACL_SUCCESS) {
                    ERROR_LOG("aclrtFreeHost failed, errorCode = %d.", static_cast<int32_t>(ret));
                    fclose(outputFile);
                    outputFile = nullptr;
                    return;
                }
            } else {
                fwrite(data, len, sizeof(char), outputFile);
            }
            fclose(outputFile);
            outputFile = nullptr;
        } else {
            ERROR_LOG("create output file [%s] failed.", outputFileName.c_str());
            return;
        }
    }
    INFO_LOG("dump data success.");
    return;
}

void ModelProcess::PrintModelDescInfo(DynamicType dynamicType)  //打印模型描述信息
{ //打印模型的输入和输出张量的描述信息，包括它们的名称、大小、格式、数据类型以及维度信息。
    if (modelDesc_ == nullptr) {
        ERROR_LOG("modelDesc_ is empty.");
        return;
    }

    size_t inputNum = aclmdlGetNumInputs(modelDesc_);   //模型的输入个数
    size_t outputNum = aclmdlGetNumOutputs(modelDesc_);  //模型的输出个数
    INFO_LOG("model input num[%zu], output num[%zu].", inputNum, outputNum);
    INFO_LOG("start to print input tensor desc:");   //描述信息

    for (size_t i = 0; i < inputNum; ++i) {
        size_t inputSize = aclmdlGetInputSizeByIndex(modelDesc_, i);   //获取输入的大小  ，参数是模型描述， i是获取第几个输入的大小
        const char* name = aclmdlGetInputNameByIndex(modelDesc_, i);   //获取指定输入的名称
        if (name == nullptr){
            ERROR_LOG("fail to get input name, index[%zu].", i);
            continue;
        }
        aclFormat format = aclmdlGetInputFormat(modelDesc_, i);   //获取输入的格式
        aclDataType dataType = aclmdlGetInputDataType(modelDesc_, i);            //获取输入的数据类型
        INFO_LOG("index[%zu]: name[%s], inputSize[%zu], fotmat[%d], dataType[%d]",
            i, name, inputSize,static_cast<int>(format), static_cast<int>(dataType));

        aclmdlIODims ioDims;     //维度
        auto ret = aclmdlGetInputDims(modelDesc_, i, &ioDims);   //获取模型输入的tensor维度
        if (ret != ACL_SUCCESS) {     //这个就是一个成功与否的值，返回一个不布尔值，要不0或者1
            ERROR_LOG("fail to get input tendor dims, index[%zu], errorCode = %d.",
                i, static_cast<int32_t>(ret));
            return;
        }

        stringstream ss;
        ss << "dimcount:[" << ioDims.dimCount << "],dims:";
        for(size_t j = 0; j < ioDims.dimCount; ++j) {
            ss << "[" << ioDims.dims[j] << "]";
        }

        INFO_LOG("%s", ss.str().c_str());
    }

    INFO_LOG("start to print output tensor desc:");
    for (size_t i = 0; i < outputNum; ++i) {
        size_t outputSize = aclmdlGetOutputSizeByIndex(modelDesc_, i);
        const char* name = aclmdlGetOutputNameByIndex(modelDesc_, i);
        if (name == nullptr){
            ERROR_LOG("fail to get output name, index[%zu].", i);
            continue;
        }

        aclFormat format = aclmdlGetOutputFormat(modelDesc_, i);
        aclDataType dataType = aclmdlGetOutputDataType(modelDesc_, i);
        INFO_LOG("index[%zu]: name[%s], outputSize[%zu], fotmat[%d], dataType[%d]",
            i, name, outputSize, static_cast<int>(format), static_cast<int>(dataType));

        aclmdlIODims ioDims;
        auto ret = aclmdlGetOutputDims(modelDesc_, i, &ioDims);
        if (ret != ACL_SUCCESS) {
            ERROR_LOG("fail to get output tendor dims, index[%zu], errorCode = %d.",
                i, static_cast<int32_t>(ret));
            return;
        }

        stringstream ss;
        ss << "dimcount:[" << ioDims.dimCount << "],dims:";
        for(size_t j = 0; j < ioDims.dimCount; ++j) {
            ss << "[" << ioDims.dims[j] << "]";
        }

        INFO_LOG("%s", ss.str().c_str());
    }

    if (m_vecDynamicHW.empty())
    {
        if (dynamicType == DYNAMIC_BATCH)
        {
            PrintDynamicBatchInfo();
        }
        else
        {
            PrintDynamicHWInfo();
        }
    }
}

void ModelProcess::PrintDynamicBatchInfo()     //打印动态batch信息
{
    if (modelDesc_ == nullptr) {
        ERROR_LOG("modelDesc_ is empty.");
        return;
    }

    INFO_LOG("start to print model dynamic batch info:");
    aclmdlBatch batchInfo;
    auto ret = aclmdlGetDynamicBatch(modelDesc_, &batchInfo);
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("fail to get dynamic batch info, errorCode = %d", static_cast<int32_t>(ret));
        return;
    }

    stringstream ss;
    ss << "dynamic batch count:[" << batchInfo.batchCount << "],dims:{";
    for(size_t i = 0; i < batchInfo.batchCount; ++i) {
        ss << "[" << batchInfo.batch[i] << "]";
    }
    ss << "}";
    INFO_LOG("%s", ss.str().c_str());
}

void ModelProcess::PrintDynamicHWInfo()    //打印动态长宽信息
{
    if (modelDesc_ == nullptr) {
        ERROR_LOG("modelDesc_ is empty.");
        return;
    }

    INFO_LOG("start to print model dynamic hw info:");
    aclmdlHW hwInfo;
    auto ret = aclmdlGetDynamicHW(modelDesc_, -1, &hwInfo);
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("fail to get dynamic hw info, errorCode = %d", static_cast<int32_t>(ret));
        return;
    }

    stringstream ss;
    ss << "dynamic hw count:[" << hwInfo.hwCount << "],dims:{";
    for(size_t i = 0; i < hwInfo.hwCount; ++i) {
        ss << "[" << hwInfo.hw[i][0];
        ss << ", " << hwInfo.hw[i][1] << "]";
        m_vecDynamicHW.push_back(std::make_pair(hwInfo.hw[i][0], hwInfo.hw[i][1]));
    }
    ss << "}";
    INFO_LOG("%s", ss.str().c_str());
}

void ModelProcess::PrintModelCurOutputDims()   //打印模型当前的输出张量的维度信息
{
    if (modelDesc_ == nullptr) {                        //检查模型描述符
        ERROR_LOG("modelDesc_ is empty.");
        return;
    }

    INFO_LOG("start to print model current output shape info:");
    aclmdlIODims ioDims;
    for (size_t i = 0; i < aclmdlGetNumOutputs(modelDesc_); ++i) {      //获取模型的输出数量，并输出以一个标题
        auto ret = aclmdlGetCurOutputDims(modelDesc_, i, &ioDims);           //遍历输出张量并打印维度信息
        if (ret != ACL_SUCCESS) {
            ERROR_LOG("fail to get cur output dims, index[%zu], errorCode = %d.",
                i, static_cast<int32_t>(ret));
            return;
        }

        stringstream ss;
        ss << "index:" << i << ",dims:";
        for(size_t j = 0; j < ioDims.dimCount; ++j) {
            ss << "[" << ioDims.dims[j] << "]";
        }
        INFO_LOG("%s", ss.str().c_str());
    }
}

std::vector<std::pair<int, int>> ModelProcess::GetVecDynamicHW()    //返回一个动态宽高信息向量，来告诉系统哪些尺寸是有效的
{
    return m_vecDynamicHW;
}

Result ModelProcess::CreateInput2(const std::string strFile, int dynamicW, int dynamicH, Rect &rect) //数据集图像的预处理
{
    input_ = aclmdlCreateDataset();     // 创建数据集
    if (input_ == nullptr)
    {
        ERROR_LOG("can't create dataset, CreateInput2 failed.");
        return FAILED;
    }

    if (FAILED == Utils::CheckPathIsFile(strFile))
    {
        return FAILED;   //路径文件
    }
    cv::Mat image = cv::imread(strFile)     //输入一个图片
    if (image.empty())
    {
        ERROR_LOG("%s imread ret empty", strFile.c_str());
        return FAILED;
    }

    // 缩放图像大小并保持纵横比        进行图像进行缩放
    cv::Size targetSize(g_modelWidth, g_modelHeight);
    cv::Mat resizedImage;      //声明了一个图像类
    cv::resize(image, resizedImage, targetSize, 0, 0, cv::INTER_LINEAR);
    cv::Mat yuvImg;
    cv::cvtColor(resizedImage, yuvImg, cv::COLOR_BGR2YUV_I420);
    int yuvSize = g_modelWidth * g_modelHeight * 3 / 2;


    // int iCenterX = 960;
    // int iCenterY = 544;
    // rect.ltX = std::max((int)(iCenterX - dynamicW / 2), 0);
    // rect.ltY = std::max((int)(iCenterY - dynamicH / 2), 0);
    // rect.rbX = std::min(iCenterX + dynamicW / 2, image.cols);
    // rect.rbY = std::min(iCenterY + dynamicH / 2, image.rows);
    // Utils::ExtendBox(rect, image.cols, image.rows, dynamicW, dynamicH);

    // cv::Rect cvRect(rect.ltX, rect.ltY, rect.rbX - rect.ltX, rect.rbY - rect.ltY);
    // cv::Mat rectImage = image(cvRect);
    // std::string strSavePath = "./" + std::to_string(dynamicW) + "_" + std::to_string(dynamicH) + ".jpg";
    // cv::imwrite(strSavePath, rectImage);
    // cv::Mat yuvImg;
    // cv::cvtColor(rectImage, yuvImg, cv::COLOR_BGR2YUV_I420);
    // int yuvSize = dynamicW * dynamicH * 3 / 2;

    for (size_t index = 0; index < aclmdlGetNumInputs(modelDesc_); ++index)   //构建模型输入数据，遍历模型的所有输入
    {
        const char *name = aclmdlGetInputNameByIndex(modelDesc_, index);
        if (name == nullptr)
        {
            ERROR_LOG("get input name failed, index = %zu.", index);
            return FAILED;
        }
        size_t inputLen = aclmdlGetInputSizeByIndex(modelDesc_, index);
        if (strcmp(name, ACL_DYNAMIC_TENSOR_NAME) == 0)
        {
            void *data = nullptr;
            auto ret = aclrtMalloc(&data, inputLen, ACL_MEM_MALLOC_HUGE_FIRST);
            if (ret != ACL_SUCCESS)
            {
                ERROR_LOG("malloc device memory failed, errorCode = %d.", static_cast<int32_t>(ret));
                return FAILED;
            }

            aclDataBuffer *dataBuffer = aclCreateDataBuffer(data, inputLen);
            if (dataBuffer == nullptr)
            {
                ERROR_LOG("create data buffer failed.");
                (void)aclrtFree(data);
                data = nullptr;
                return FAILED;
            }

            ret = aclmdlAddDatasetBuffer(input_, dataBuffer);
            if (ret != ACL_SUCCESS)
            {
                ERROR_LOG("add user input dataset buffer failed, errorCode = %d.", static_cast<int32_t>(ret));
                (void)aclrtFree(data);
                data = nullptr;
                (void)aclDestroyDataBuffer(dataBuffer);
                dataBuffer = nullptr;
                return FAILED;
            }
        }
        else
        {
            void *data = nullptr;
            auto ret = aclrtMalloc(&data, inputLen, ACL_MEM_MALLOC_HUGE_FIRST);
            if (ret != ACL_SUCCESS)
            {
                ERROR_LOG("malloc device memory failed, errorCode = %d.", static_cast<int32_t>(ret));
                return FAILED;
            }

            INFO_LOG("inputLen:%zu---------yuvSize:%d.", inputLen, yuvSize);
            ret = aclrtMemcpy(data, inputLen, yuvImg.data, yuvSize, ACL_MEMCPY_DEVICE_TO_DEVICE);
            if (ret != ACL_SUCCESS)
            {
                ERROR_LOG("Copy data to device failed, ret is %d", ret);
                (void)aclrtFree(data);
                return FAILED;
            }

            aclDataBuffer *inputData = aclCreateDataBuffer(data, inputLen);
            if (inputData == nullptr)
            {
                ERROR_LOG("can't create data buffer, create input failed.");
                (void)aclrtFree(data);
                data = nullptr;
                return FAILED;
            }

            ret = aclmdlAddDatasetBuffer(input_, inputData);
            if (ret != ACL_SUCCESS)
            {
                ERROR_LOG("add input dataset buffer failed, errorCode = %d.", static_cast<int32_t>(ret));
                aclDestroyDataBuffer(inputData);
                inputData = nullptr;
                (void)aclrtFree(data);
                data = nullptr;
                return FAILED;
            }
        }
    }
    INFO_LOG("create model input success.");   //这个代码的意思是
    return SUCCESS;
}



/**
 *@brief 获取推理输出数据
 *@param in itemDataSize 数据大小
 *@param in inferenceOutput 推理输出数据
 *@param in idx 索引
 *@return 输出数据指针
 */ //从深度模型推理数据集中，获取指定的索引数据和大小，以便后续的处理，解析输出，后处理等。
void* ModelProcess::GetInferenceOutputItem(uint32_t& itemDataSize, aclmdlDataset* inferenceOutput, uint32_t idx)
{
    aclDataBuffer* dataBuffer = aclmdlGetDatasetBuffer(inferenceOutput, idx);
    if (dataBuffer == nullptr) {
        ERROR_LOG("Get the %uth dataset buffer from model "
        "inference output failed", idx);
        return nullptr;
    }

    void* dataBufferDev = aclGetDataBufferAddr(dataBuffer);
    if (dataBufferDev == nullptr) {
        ERROR_LOG("Get the %uth dataset buffer address "
        "from model inference output failed", idx);
        return nullptr;
    }

    size_t bufferSize = aclGetDataBufferSizeV2(dataBuffer);
    if (bufferSize == 0) {
        ERROR_LOG("The %uth dataset buffer size of "
        "model inference output is 0", idx);
        return nullptr;
    }

    void* data = nullptr;
    // if (!g_isDevice) {
    //     data = Utils::CopyDataDeviceToLocal(dataBufferDev, bufferSize);
    //     if (data == nullptr) {
    //         ERROR_LOG("Copy inference output to host failed");
    //         return nullptr;
    //     }
    // }
    // else {
        data = dataBufferDev;
    //}

    itemDataSize = bufferSize;
    return data;
}

/**
 *@brief 跟踪检测的后处理
 *@param in modelOutput 模型输出数据
 *@param in W 宽
 *@param in H 高
 *@param in rect 跟踪目标框
 *@return 目标框信息
 */
std::vector<BBox> ModelProcess::PostprocessByTracing(uint32_t W, uint32_t H, Rect &rect, DynamicInfo &dynamicInfo)
{     //进行模型的后处理跟踪 实现了从模型输出中提取边界框（bounding boxes）、进行非极大值抑制（Non-Maximum Suppression, NMS）以过滤冗余检测
    std::vector<BBox> binfo, bboxesNew;
    size_t outDatasetNum = aclmdlGetDatasetNumBuffers(output_);
    if (outDatasetNum != 3)
    {
        ERROR_LOG("outDatasetNum=%zu must be 3", outDatasetNum);
        return binfo;
    }

    kGridSizeX_.clear();
    kGridSizeY_.clear();
    kGridSizeX_.push_back(dynamicInfo.dynamicArr[1] / 8);
    kGridSizeX_.push_back(dynamicInfo.dynamicArr[1] / 16);
    kGridSizeX_.push_back(dynamicInfo.dynamicArr[1] / 32);
    kGridSizeY_.push_back(dynamicInfo.dynamicArr[0] / 8);
    kGridSizeY_.push_back(dynamicInfo.dynamicArr[0] / 16);
    kGridSizeY_.push_back(dynamicInfo.dynamicArr[0] / 32);

    uint numBBoxes = 3;
    uint m_numClasses = 2;
    uint m_BoxTensorLabel = 5 + m_numClasses; //5表示[x, y, w, h, confidence]
    float m_detThresh = 0.1;
    const int stride[3] = {8, 16, 32};
    float m_nmsThresh = 0.4;

    float x, y, w, h, cf, tx, ty, tw, th; // anchors = [[10, 13, 16, 30, 33, 23], [30, 61, 62, 45, 59, 119], [116, 90, 156, 198, 373, 326]]
    for (size_t i = 0; i < outDatasetNum; i++)
    {
        const uint32_t gridrow = kGridSizeY_[i]; // kGridSize[i][0];
        const uint32_t gridcol = kGridSizeX_[i]; // kGridSize[i][1];
        //        cout<<"gridrow:"<<gridrow<<" gridcol:"<<gridcol<<endl;
        uint32_t dataSize = 0;
        float *detectData = (float *)GetInferenceOutputItem(dataSize, output_, i);
        // cout<<detectData[0]<<endl;
        for (uint cx = 0; cx < gridrow; ++cx)
        {
            for (uint cy = 0; cy < gridcol; ++cy)
            {
                for (uint k = 0; k < numBBoxes; ++k)
                {
                    cf = detectData[((k * m_BoxTensorLabel + 4) * gridrow + cx) * gridcol + cy];
                    if(cf < m_detThresh){
                        continue;
                    }
                    // if(cx==0 and cy==0) cout<<tx<<" "<<ty<<" "<<tw<<" "<<th<<" "<<cf<<endl;
                    // find best precision
                    float Maxclass = 0.0;
                    uint32_t Maxclass_Loc = 0xFFFFFFFF;
                    for (uint32_t j = 5; j < m_BoxTensorLabel; ++j)
                    {
                        float class_prob = detectData[((k * m_BoxTensorLabel + j) * gridrow + cx) * gridcol + cy];
                        if (Maxclass < class_prob)
                        {
                            Maxclass = class_prob;
                            Maxclass_Loc = j - 5;
                        }
                    }
                    if (Maxclass_Loc != 0xFFFFFFFF and cf * Maxclass >= m_detThresh)
                    {
                        tx = detectData[((k * m_BoxTensorLabel) * gridrow + cx) * gridcol + cy];
                        ty = detectData[((k * m_BoxTensorLabel + 1) * gridrow + cx) * gridcol + cy];
                        tw = detectData[((k * m_BoxTensorLabel + 2) * gridrow + cx) * gridcol + cy];
                        th = detectData[((k * m_BoxTensorLabel + 3) * gridrow + cx) * gridcol + cy];

                        BBox boundBox;
                        // 推理返回 中心点和宽高
                        x = (tx * 2 - 0.5 + cy) * stride[i];
                        y = (ty * 2 - 0.5 + cx) * stride[i];
                        w = (tw * 2) * (tw * 2) * m_anchor[i][k][0];
                        h = (th * 2) * (th * 2) * m_anchor[i][k][1];

                        // 还原至原图 左上右下
                        boundBox.rect.ltX = (uint32_t)max(((x - w / 2.0)), 0.0) + rect.ltX;
                        boundBox.rect.ltY = (uint32_t)max(((y - h / 2.0)), 0.0) + rect.ltY;
                        boundBox.rect.rbX = (uint32_t)min(((x + w / 2.0)), W * 1.0) + rect.ltX;
                        boundBox.rect.rbY = (uint32_t)min(((y + h / 2.0)), H * 1.0) + rect.ltY;

                        boundBox.score = cf * Maxclass;
                        boundBox.cls = Maxclass_Loc;
                        binfo.push_back(boundBox);
                        //cout << boundBox.rect.ltX << " " << boundBox.rect.ltY << " " << boundBox.rect.rbX << " " << boundBox.rect.rbY << endl;
                    }
                }
            }
        }
        if (!g_isDevice)
        {
            delete[] detectData;
        }
    }
    // NMS
    bboxesNew = Utils::nonMaximumSuppression(m_nmsThresh, binfo);
    INFO_LOG("------------nms---------------");
    for (uint32_t i = 0; i < bboxesNew.size(); ++i)
    {
        INFO_LOG("result: %d,  %.3f,  %d, %d, %d, %d", bboxesNew[i].cls, bboxesNew[i].score, bboxesNew[i].rect.ltX, bboxesNew[i].rect.ltY, bboxesNew[i].rect.rbX, bboxesNew[i].rect.rbY);
    }
    INFO_LOG("binfo.size:%zu  bboxesNew.size:%zu",binfo.size(), bboxesNew.size());
    return bboxesNew;
}


/**
 *@brief nosigmoid后处理
 *@param in W 宽
 *@param in H 高
 *@return 目标框信息
 */
vector<BBox> ModelProcess::Postprocess(uint32_t W, uint32_t H)
{  //解析目标检测信息和应用非极大值抑制（NMS）来优化检测结果

    //auto part1Start = std::chrono::high_resolution_clock::now();
    vector<BBox> binfo, bboxesNew;
    size_t outDatasetNum = aclmdlGetDatasetNumBuffers(output_);
    if (outDatasetNum != 3)
    {
        ERROR_LOG("outDatasetNum=%zu must be 3", outDatasetNum);
        return bboxesNew;
    }
    //auto part1End = std::chrono::high_resolution_clock::now();
    //INFO_LOG("model part1 use time:%ld ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(part1End - part1Start).count());

    //uint32_t modelWidth_ = 960;
    //uint32_t modelHeight_ = 544;
    uint32_t modelWidth_ = g_modelWidth;
    uint32_t modelHeight_ = g_modelHeight;

    float widthScale = float(modelWidth_) / float(W);
    float heightScale = float(modelHeight_) / float(H);
    float Scale = min(widthScale, heightScale);

    uint32_t new_W = (uint32_t)(Scale * W);
    uint32_t new_H = (uint32_t)(Scale * H);
    float dw = (float)(modelWidth_ - new_W) / 2.0;
	float dh = (float)(modelHeight_ - new_H) / 2.0;

	int paddingX_ = int(dw);
	int paddingY_ = int(dh);

    //------------------------------------
    uint numBBoxes = 3;
    //uint m_numClasses = 2;
    uint m_numClasses = 1;
    uint m_BoxTensorLabel = 5 + m_numClasses; //5表示[x, y, w, h, confidence]
    float m_detThresh = 0.1;
    //std::vector<uint32_t> kGridSizeX_{120, 60, 30};
    //std::vector<uint32_t> kGridSizeY_{68, 34, 17};
    //std::vector<uint32_t> kGridSizeX_{84, 42, 21};
    //std::vector<uint32_t> kGridSizeY_{48, 24, 12};
    
    const int stride[3] = {8, 16, 32};
    float m_nmsThresh = 0.4;
    //------------------------------------

    float x, y, w, h, cf, tx, ty, tw, th;
    //auto part2End = std::chrono::high_resolution_clock::now();
    //INFO_LOG("model part2 use time:%ld ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(part2End - part1End).count());
    
    for (size_t i = 0; i < outDatasetNum; i++)
    {
        uint32_t dataSize = 0;
        size_t strideNum = 0;
        float *detectData = (float *)GetInferenceOutputItem(dataSize, output_, i);

        const uint32_t gridrow = kGridSizeY_[i];
        const uint32_t gridcol = kGridSizeX_[i];
        uint32_t gridcolTemp = gridcol + strideNum;
        for (uint cx = 0; cx < gridrow; ++cx)
        {
            for (uint cy = 0; cy < gridcol; ++cy)
            {
                for (uint k = 0; k < numBBoxes; ++k)
                {
                    cf = detectData[((k * m_BoxTensorLabel + 4) * gridrow + cx) * gridcolTemp + cy];
                    if (cf < m_detThresh)
                    {
                        continue;
                    }
                    
                    //获取框类型，和类型置信度
                    float Maxclass = 0.0;
                    uint32_t Maxclass_Loc = 0xFFFFFFFF;
                    for (uint32_t j = 5; j < m_BoxTensorLabel; ++j)
                    {
                        float class_prob = detectData[((k * m_BoxTensorLabel + j) * gridrow + cx) * gridcolTemp + cy];
                        if (Maxclass < class_prob)
                        {
                            Maxclass = class_prob;
                            Maxclass_Loc = j - 5;
                        }
                    }

                    if (Maxclass_Loc != 0xFFFFFFFF && (cf * Maxclass >= m_detThresh))
                    {
                        tx = detectData[((k * m_BoxTensorLabel) * gridrow + cx) * gridcolTemp + cy];
                        ty = detectData[((k * m_BoxTensorLabel + 1) * gridrow + cx) * gridcolTemp + cy];
                        tw = detectData[((k * m_BoxTensorLabel + 2) * gridrow + cx) * gridcolTemp + cy];
                        th = detectData[((k * m_BoxTensorLabel + 3) * gridrow + cx) * gridcolTemp + cy];

                        BBox boundBox;
                        x = (tx * 2 - 0.5 + cy) * stride[i];
                        y = (ty * 2 - 0.5 + cx) * stride[i];
                        w = (tw * 2) * (tw * 2) * m_anchor[i][k][0];
                        h = (th * 2) * (th * 2) * m_anchor[i][k][1];

                        x -= paddingX_;
                        y -= paddingY_;

                        x = (uint32_t)(x / Scale);
                        y = (uint32_t)(y / Scale);
                        w = (uint32_t)(w / Scale);
                        h = (uint32_t)(h / Scale);

                        boundBox.rect.ltX = (uint32_t)max(((x - w / 2.0)), 0.0);
                        boundBox.rect.ltY = (uint32_t)max(((y - h / 2.0)), 0.0);
                        boundBox.rect.rbX = (uint32_t)min(((x + w / 2.0)), W * 1.0);
                        boundBox.rect.rbY = (uint32_t)min(((y + h / 2.0)), H * 1.0);

                        boundBox.score = cf * Maxclass;
                        boundBox.cls = Maxclass_Loc;
                        binfo.push_back(boundBox);
                    }
                }
            }
        }
    }
    //auto part3End = std::chrono::high_resolution_clock::now();
    //INFO_LOG("model part3 use time:%ld ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(part3End - part2End).count());
    /*for (uint32_t i = 0; i < binfo.size(); ++i)
    {
        INFO_LOG("result: %d,  %.3f,  %d, %d, %d, %d", binfo[i].cls, binfo[i].score, binfo[i].rect.ltX, binfo[i].rect.ltY, binfo[i].rect.rbX, binfo[i].rect.rbY);
    }*/
    //NMS
    bboxesNew = Utils::nonMaximumSuppression(m_nmsThresh, binfo);
    INFO_LOG("------------nms---------------");
    for (uint32_t i = 0; i < bboxesNew.size(); ++i)
    {
        INFO_LOG("result: %d,  %.3f,  %d, %d, %d, %d", bboxesNew[i].cls, bboxesNew[i].score, bboxesNew[i].rect.ltX, bboxesNew[i].rect.ltY, bboxesNew[i].rect.rbX, bboxesNew[i].rect.rbY);
    }
    INFO_LOG("binfo.size:%zu  bboxesNew.size:%zu",binfo.size(), bboxesNew.size());
    return bboxesNew;
}


/*
下面，我将逐个分解每个函数的功能及其在模型推理管道中的重要性：

模型的加载与卸载：

ModelProcess() : 构造函数，初始化成员变量并为模型执行搭建必要的结构。
LoadModel() : 从指定的文件路径将模型加载到内存中，为模型执行分配必要的缓冲区。
UnloadModel() : 从内存中卸载模型，释放与模型关联的所有已分配资源。
模型的描述与销毁：

CreateDesc() : 创建模型描述对象，其中包含模型输入、输出以及其他配置信息。
DestroyDesc() : 当不再需要时，销毁模型描述对象。
模型输入管理：

CreateInput() : 准备模型的输入数据，包括为输入张量分配内存并创建数据集来存储所有输入张量。
DestroyInput() : 释放输入数据的内存并销毁输入数据集。
模型输出管理：

CreateOutput() : 准备模型的输出数据，为输出张量分配内存并创建数据集来存储所有输出张量。
DestroyOutput() : 释放输出数据的内存并销毁输出数据集。
动态模型配置：

SetDynamicBatchSize() : 为支持动态批处理的模型设置动态批次大小。
SetDynamicHWSize() : 为支持动态输入形状的模型设置动态高度和宽度。
模型执行：

Execute() : 使用准备好的输入和输出数据集执行模型。
输出处理：

DumpModelOutputResult() : 将模型的输出保存到二进制文件中，便于调试或分析。
PrintModelDescInfo(), PrintDynamicBatchInfo(), PrintDynamicHWInfo(), PrintModelCurOutputDims() : 这些函数打印关于模型及其当前状态的各种信息，对于调试和理解模型的行为非常有用。
后处理：

PostprocessByTracing() : 对模型输出进行后处理，如物体检测任务中的非极大值抑制（NMS），用于细化边界框并过滤低置信度的检测结果。
Postprocess() : 另一个后处理函数，可能与PostprocessByTracing()功能相似，但未提供具体细节。
*/