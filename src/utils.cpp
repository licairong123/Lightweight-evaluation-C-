/**
* @file utils.cpp
*
* Copyright (C) 2021. Shenshu Technologies Co., Ltd. All rights reserved.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/
#include "utils.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sys/stat.h>          //结构体stat
//#include "acl/svp_acl.h"
#include <opencv2/opencv.hpp>

using namespace std;  

extern uint32_t g_modelWidth;
extern uint32_t g_modelHeight;
extern bool g_isDevice;

std::string Utils::m_strOmType;            //全局变量
std::string Utils::m_strDataType;


void Utils::InitData(int8_t* data, size_t dataSize)
{
    for (size_t i = 0; i < dataSize; i++) {
        data[i] = 0;
    }
}

Result Utils::GetFileSize(const std::string& fileName, uint32_t& fileSize)       // 这个都是引用
{
    std::ifstream binFile(fileName, std::ifstream::binary);        // 文件流操作   创建了这么一个对象，这个对象就是文件流对象
    if (binFile.is_open() == false) {                           //这个就代表了一种无法打开文件
        ERROR_LOG("open file %s failed", fileName.c_str());
        return FAILED;
    }
    binFile.seekg(0, binFile.end);
    int binFileBufferLen = binFile.tellg();
    if (binFileBufferLen == 0) {
        ERROR_LOG("binfile is empty, filename is %s", fileName.c_str());
        binFile.close();
        return FAILED;
    }
    fileSize = static_cast<uint32_t>(binFileBufferLen);           //是一种类型转换相当于int（）python的int()
    binFile.close();
    return SUCCESS;
}

Result Utils::ReadFloatFile(const std::string& fileName, std::vector<float>& detParas)   //这个就是确定的引用的值， 得到的值就是引用的值  数组引用
{
    struct stat sBuf;
    int fileStatus = stat(fileName.data(), &sBuf);    //取址
    if (fileStatus == -1) {
        ERROR_LOG("failed to get file %s", fileName.c_str());
        return FAILED;
    }
    if (S_ISREG(sBuf.st_mode) == 0) {
        ERROR_LOG("%s is not a file, please enter a file", fileName.c_str());
        return FAILED;
    }
    std::ifstream txtFile;
    txtFile.open(fileName);
    if (txtFile.is_open() == false) {
        ERROR_LOG("open file %s failed", fileName.c_str());
        return FAILED;
    }
    float c;
    while (!txtFile.eof()) {
        if (!txtFile.good()) {
            return FAILED;
        }
        txtFile >> c;
        detParas.push_back(c);
    }
    return SUCCESS;
}

void* Utils::ReadBinFile(const std::string & fileName, uint32_t & fileSize)     //  这个是指向任意类型数据的指针
{
    struct stat sBuf;      // 定义了一个结构体的变量
    int fileStatus = stat(fileName.data(), &sBuf);           // 状态信息，返回0表示成功，-1表示失败
    if (fileStatus == -1) {
        ERROR_LOG("failed to get file %s", fileName.c_str());
        return nullptr;
    }
    if (S_ISREG(sBuf.st_mode) == 0) {                                   //获取状态信息的类型和权限，返回0表示成功，-1表示失败
        ERROR_LOG("%s is not a file, please enter a file", fileName.c_str());
        return nullptr;
    }
    std::ifstream binFile(fileName, std::ifstream::binary);          // 文件流操作  打开这个文件二进制
    if (binFile.is_open() == false) {
        ERROR_LOG("open file %s failed", fileName.c_str());
        return nullptr;                                     // 返回一个空指针
    }
    binFile.seekg(0, binFile.end);                   //返回一个文件末尾的迭代器的位置
    int binFileBufferLen = binFile.tellg();                  //返回当前的位置到文件末尾的字节数
    if (binFileBufferLen == 0) {
        ERROR_LOG("binfile is empty, filename is %s", fileName.c_str());
        binFile.close();
        return nullptr;
    }
    binFile.seekg(0, binFile.beg);               //返回一个文件开头的迭代器的位置
    void* binFileBufferData = nullptr;
    aclError ret = ACL_SUCCESS;
    if (!g_isDevice) {
        ret = aclrtMallocHost(&binFileBufferData, binFileBufferLen);
        if (binFileBufferData == nullptr) {
            ERROR_LOG("malloc binFileBufferData failed, errorCode = %d.", static_cast<int32_t>(ret));
            binFile.close();
            return nullptr;
        }
    } else {
        ret = aclrtMalloc(&binFileBufferData, binFileBufferLen, ACL_MEM_MALLOC_NORMAL_ONLY);
        if (ret != ACL_SUCCESS) {
            ERROR_LOG("malloc device buffer failed. size is %u, errorCode = %d.",
                binFileBufferLen, static_cast<int32_t>(ret));
            binFile.close();
            return nullptr;
        }
    }
    binFile.read(static_cast<char *>(binFileBufferData), binFileBufferLen);     // 读取文件内容
    binFile.close();
    fileSize = static_cast<uint32_t>(binFileBufferLen);
    return binFileBufferData;
}

// void* Utils::ReadBinFileWithStride(const std::string& fileName, const svp_acl_mdl_io_dims& dims,
//     size_t stride, size_t dataSize)
// {
//     struct stat sBuf;
//     int fileStatus = stat(fileName.data(), &sBuf);
//     if (fileStatus == -1) {
//         ERROR_LOG("failed to get file %s", fileName.c_str());
//         return nullptr;
//     }

//     if (S_ISREG(sBuf.st_mode) == 0) {
//         ERROR_LOG("%s is not a file, please enter a file", fileName.c_str());
//         return nullptr;
//     }

//     std::ifstream binFile(fileName, std::ifstream::binary);
//     if (binFile.is_open() == false) {
//         ERROR_LOG("open file %s failed", fileName.c_str());
//         return nullptr;
//     }
//     binFile.seekg(0, binFile.end);
//     int binFileBufferLen = binFile.tellg();
//     if (binFileBufferLen == 0) {
//         ERROR_LOG("binfile is empty, filename is %s", fileName.c_str());
//         binFile.close();
//         return nullptr;
//     }
//     binFile.seekg(0, binFile.beg);
//     void* binFileBufferData = nullptr;
//     int64_t loopTimes = 1;
//     for (size_t loop = 0; loop < dims.dim_count - 1; loop++) {
//         loopTimes *= dims.dims[loop];
//     }
//     size_t bufferSize = loopTimes * stride;
//     svp_acl_error ret = svp_acl_rt_malloc(&binFileBufferData, bufferSize, SVP_ACL_MEM_MALLOC_NORMAL_ONLY);
//     if (ret != SVP_ACL_SUCCESS) {
//         ERROR_LOG("malloc device buffer failed. size is %u", binFileBufferLen);
//         binFile.close();
//         return nullptr;
//     }
//     //InitData(static_cast<int8_t*>(binFileBufferData), bufferSize);
//     memset_s(binFileBufferData, bufferSize, 0, bufferSize);

//     int64_t dimValue = dims.dims[dims.dim_count - 1];
//     size_t lineSize = dimValue * dataSize;
//     for (int64_t loop = 0; loop < loopTimes; loop++) {
//         binFile.read((static_cast<char *>(binFileBufferData) + loop * stride), lineSize);
//     }

//     binFile.close();
//     return binFileBufferData;
// }

// void* Utils::GetDeviceBufferOfFile(const std::string& fileName, const svp_acl_mdl_io_dims& dims,
//     size_t stride, size_t dataSize)
// {
//     if (m_strDataType == "bf") // binary file
//     {
//         return Utils::ReadBinFileWithStride(fileName, dims, stride, dataSize);
//     }
//     else if (m_strDataType == "img")
//     {
//         return Utils::ReadImgFileWithStride(fileName, dims, stride, dataSize);
//     }
//     return nullptr;
// }

std::vector<BBox> Utils::nonMaximumSuppression(const float nmsThresh, std::vector<BBox> binfo)
{
    auto overlap1D = [](float x1min, float x1max, float x2min, float x2max) -> float
    {
        float left = std::max(x1min, x2min);
        float right = std::min(x1max, x2max);
        return right - left;
    };
    auto computeIoU = [&overlap1D](BBox &bbox1, BBox &bbox2) -> float
    {
        float overlapX = overlap1D(bbox1.rect.ltX, bbox1.rect.rbX, bbox2.rect.ltX, bbox2.rect.rbX);
        float overlapY = overlap1D(bbox1.rect.ltY, bbox1.rect.rbY, bbox2.rect.ltY, bbox2.rect.rbY);
        if (overlapX <= 0 or overlapY <= 0)
        {
            return 0;
        }
        float area1 = (bbox1.rect.rbX - bbox1.rect.ltX) * (bbox1.rect.rbY - bbox1.rect.ltY);
        float area2 = (bbox2.rect.rbX - bbox2.rect.ltX) * (bbox2.rect.rbY - bbox2.rect.ltY);
        float overlap2D = overlapX * overlapY;
        float u = area1 + area2 - overlap2D;
        return (u > -0.000001 && u < 0.000001) ? 0 : overlap2D / u;    //防止除0     三元运算符
    }; 

    std::stable_sort(binfo.begin(), binfo.end(),
                     [](const BBox &b1, const BBox &b2)
                     { return b1.score > b2.score; });
    std::vector<BBox> out;                         // 数组是一个边界框的集合
    for (auto &i : binfo)           // 这个是引用这个变量 ，不是拷贝， i是binfo的别名 ，指向binfo的地址
    {
        bool keep = true;
        for (auto &j : out)
        {
            if (keep)
            {
                float overlap = computeIoU(i, j);
                keep = overlap <= nmsThresh;
            }
            else
            {
                break;
            }
        }
        if (keep)
        {
            out.push_back(i);     //添加到列表中
        }
    }
    return out;
}

// void *Utils::ReadImgFileWithStride(const std::string &fileName, const svp_acl_mdl_io_dims &dims,
//                                    size_t stride, size_t dataSize)
// {
//     struct stat sBuf;
//     int fileStatus = stat(fileName.data(), &sBuf);
//     if (fileStatus == -1) {
//         ERROR_LOG("failed to get file %s", fileName.c_str());
//         return nullptr;
//     }

//     if (S_ISREG(sBuf.st_mode) == 0) {
//         ERROR_LOG("%s is not a file, please enter a file", fileName.c_str());
//         return nullptr;
//     }

//     cv::Mat image = cv::imread(fileName);

//     // 缩放图像大小并保持纵横比
//     cv::Size targetSize(g_modelWidth, g_modelHeight);
//     cv::Mat resizedImage;
//     cv::resize(image, resizedImage, targetSize, 0, 0, cv::INTER_LINEAR);

//     void* binFileBufferData = nullptr;
//     int64_t loopTimes = 1;
//     for (size_t loop = 0; loop < dims.dim_count - 1; loop++) {
//         loopTimes *= dims.dims[loop];
//     }
//     if (m_strOmType == "YUV")
//     {
//         // 注：yuv时loopTimes = dims.dims[0]*dims.dims1[1]*dims.dims[2] / 2;
//         //  yuv的高度是  h*3/2。因此申请的空间是BGR的3个通道的一半即可
//         loopTimes /= 2;
//     }

//     size_t bufferSize = loopTimes * stride;
//     svp_acl_error ret = svp_acl_rt_malloc(&binFileBufferData, bufferSize, SVP_ACL_MEM_MALLOC_NORMAL_ONLY);
//     if (ret != SVP_ACL_SUCCESS) {
//         ERROR_LOG("malloc device buffer failed. size is %zu", bufferSize);
//         return nullptr;
//     }
//     //InitData(static_cast<int8_t*>(binFileBufferData), bufferSize);
//     memset_s(binFileBufferData, bufferSize, 0, bufferSize);

//     int64_t dimValue = dims.dims[dims.dim_count - 1];
//     size_t lineSize = dimValue * dataSize;
//     INFO_LOG("dimvalue:%ld,  linesize:%zu,  loopTimes:%ld stride:%zu \n", dimValue, lineSize, loopTimes, stride);
//     if (m_strOmType == "YUV")
//     {
//         cv::Mat yuvImg;
//         cv::cvtColor(resizedImage, yuvImg, cv::COLOR_BGR2YUV_I420);
//         for (int64_t loop = 0; loop < loopTimes; loop++)
//         {
//             memcpy((static_cast<char *>(binFileBufferData) + loop * stride), (yuvImg.data + loop * lineSize), lineSize);
//         }
//     }
//     else if (m_strOmType == "BGR")
//     {
//         std::vector<cv::Mat> channels;
//         cv::split(resizedImage, channels);
//         int oneChnn = channels[0].rows;
//         for (int64_t loop = 0; loop < loopTimes; loop++)
//         {
//             int i = loop/oneChnn;
//             memcpy((static_cast<char *>(binFileBufferData) + loop * stride), (channels[i].data + (loop - i * oneChnn) * lineSize), lineSize);
//         }
//     }
//     return binFileBufferData;
// }

Result Utils::CheckPathIsFile(const std::string &fileName)     //函数传递的时候通常使用引用
{
#if defined(_MSC_VER)
    DWORD bRet = GetFileAttributes((LPCSTR)fileName.c_str());
    if (bRet == FILE_ATTRIBUTE_DIRECTORY) {
        ERROR_LOG("%s is not a file, please enter a file", fileName.c_str());
        return FAILED;
    }
#else
    struct stat sBuf;                   //状态信息文件
    int fileStatus = stat(fileName.data(), &sBuf);     //取位置的符号
    if (fileStatus == -1) {
        ERROR_LOG("failed to get file");
        return FAILED;       //返回一个宏失败的命令
    }
    if (S_ISREG(sBuf.st_mode) == 0) {
        ERROR_LOG("%s is not a file, please enter a file", fileName.c_str());
        return FAILED;
    }
#endif

    return SUCCESS;
}
