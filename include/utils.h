/**
* @file utils.h
*
* Copyright (C) 2021. Shenshu Technologies Co., Ltd. All rights reserved.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/
#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <vector>
#include <string>
// #include "acl/acl.h"
#include <vector>

#define INFO_LOG(fmt, ...) fprintf(stdout, "[INFO]  " fmt "\n", ##__VA_ARGS__)
#define WARN_LOG(fmt, ...) fprintf(stdout, "[WARN]  " fmt "\n", ##__VA_ARGS__)
#define ERROR_LOG(fmt, ...) fprintf(stdout, "[ERROR] " fmt "\n", ##__VA_ARGS__)

#ifdef _WIN32
#define S_ISREG(m) (((m) & 0170000) == (0100000))
#endif

typedef enum Result {
    SUCCESS = 0,
    FAILED = 1
} Result;

struct Rect
{
    uint32_t ltX = 0;
    uint32_t ltY = 0;
    uint32_t rbX = 0;
    uint32_t rbY = 0;
};

struct BBox
{
    Rect rect;
    float score;
    uint32_t cls;
};

typedef enum DynamicType {
    DYNAMIC_BATCH = 0,
    DYNAMIC_HW = 1
} DynamicType;

typedef struct DynamicInfo {
    DynamicType dynamicType = DYNAMIC_BATCH;
    uint32_t imageW = 0;
    uint32_t imageH = 0;
    uint64_t dynamicArr[2] = {0};
} DynamicInfo;

typedef struct ImageMemoryInfo {
    size_t imageDataSize = 0;
    size_t imageInfoSize = 0;
    void *imageDataBuf = nullptr;
    void *imageInfoBuf = nullptr;
} ImageMemoryInfo;

class Utils {
public:
    /**
    * @brief create device buffer of file
    * @param [in] fileName: file name
    * @param [out] fileSize: size of file
    * @return device buffer of file
    */
    // static void* GetDeviceBufferOfFile(const std::string& fileName, const svp_acl_mdl_io_dims& dims,
    //     size_t stride, size_t dataSize);

    /**
    * @brief create buffer of file
    * @param [in] fileName: file name
    * @param [out] fileSize: size of file
    * @return buffer of pic
    */
    static void* ReadBinFile(const std::string& fileName, uint32_t& fileSize);

    static Result ReadFloatFile(const std::string& fileName, std::vector<float>& detParas);

    static Result GetFileSize(const std::string& fileName, uint32_t& fileSize);

    // static void* ReadBinFileWithStride(const std::string& fileName, const svp_acl_mdl_io_dims& dims,
    //     size_t stride, size_t dataSize);

    // static void* ReadImgFileWithStride(const std::string& fileName, const svp_acl_mdl_io_dims& dims,
    //     size_t stride, size_t dataSize);

    static void InitData(int8_t* data, size_t dataSize);

    /**
     * @brief Check whether the path is a file.
     * @param [in] fileName: fold to check
     * @return result
     */
    static Result CheckPathIsFile(const std::string &fileName);

    /**
     *@brief nms处理
     *@param in nmsThresh nms阈值
     *@param in binfo 目标信息结构体
     *@return 目标信息结构体
     */
    static std::vector<BBox> nonMaximumSuppression(const float nmsThresh, std::vector<BBox> binfo);

    static std::string m_strOmType;
    static std::string m_strDataType;

};

#endif
