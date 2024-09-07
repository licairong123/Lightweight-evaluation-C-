/**
* @file sample_process.cpp
*
* Copyright (C) 2021. Shenshu Technologies Co., Ltd. All rights reserved.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/
#include "sample_process.h"  //�Լ���д��ͷ�ļ�����
#include <chrono>
#include <vector>
#include <unistd.h>
#include <opencv2/opencv.hpp>



using namespace std;

extern std::string g_strModelPath;    //�ⲿ��һ���ַ�������  ����һ���µ��ַ�������õ�һ���ڴ�ռ�
extern bool g_isDevice;       

SampleProcess::SampleProcess() : deviceId_(0), context_(nullptr), stream_(nullptr)
{      
} //���캯���� ���Թ��캯������Ĭ��ֵ���趨

SampleProcess::~SampleProcess()    //�����Ա�������������������ͷ��ڴ�
{
    DestroyResource();
}

Result SampleProcess::InitResource()  //�����������κ����ͣ� ������г�Ա�������趨
{
    // ACL init   ��ʼ��atlas ƽ̨���칹�����  �ԕN���豸�������Դ������ȷ�ĳ�ʼ������
    const char *aclConfigPath = "../src/acl.json";   // ��һ���ַ�λ��
    aclError ret = aclInit(aclConfigPath);   //����ֵ������
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("acl init failed, errorCode = %d.", static_cast<int32_t>(ret));
        return FAILED;
    }
    INFO_LOG("acl init success.");

    // set device
    ret = aclrtSetDevice(deviceId_);   //����������ӿڶ����ʱ�򣬾��Ѿ���һ���ķ���ֵ����
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("acl set device %d failed, errorCode = %d.",
            deviceId_, static_cast<int32_t>(ret));
        return FAILED;
    }
    INFO_LOG("set device %d success.", deviceId_);   //��ӡ��־

    // create context (set current)
    ret = aclrtCreateContext(&context_, deviceId_);
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("acl create context failed, deviceId = %d, errorCode = %d.",
            deviceId_, static_cast<int32_t>(ret));
        return FAILED;
    }
    INFO_LOG("create context success.");

    // create stream
    ret = aclrtCreateStream(&stream_);
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("acl create stream failed, deviceId = %d, errorCode = %d.",
            deviceId_, static_cast<int32_t>(ret));
        return FAILED;
    }
    INFO_LOG("create stream success.");

    // get run mode
    aclrtRunMode runMode;
    ret = aclrtGetRunMode(&runMode);
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("acl get run mode failed, errorCode = %d.", static_cast<int32_t>(ret));
        return FAILED;
    }
    g_isDevice = (runMode == ACL_DEVICE);    //�������״̬�룬�Ƿ���һ���N���豸
    INFO_LOG("get run mode success.");
    return SUCCESS;
}

void SampleProcess::DestroyResource()
{
    aclError ret;
    if (stream_ != nullptr) {
        ret = aclrtDestroyStream(stream_);
        if (ret != ACL_SUCCESS) {
            ERROR_LOG("destroy stream failed, errorCode = %d.", static_cast<int32_t>(ret));
        }
        stream_ = nullptr;
    }
    INFO_LOG("end to destroy stream.");

    if (context_ != nullptr) {
        ret = aclrtDestroyContext(context_);
        if (ret != ACL_SUCCESS) {
            ERROR_LOG("destroy context failed, errorCode = %d.", static_cast<int32_t>(ret));
        }
        context_ = nullptr;
    }
    INFO_LOG("end to destroy context.");

    ret = aclrtResetDevice(deviceId_);    //��λ��ǰ���豸����Դ
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("reset device %d failed, errorCode = %d.",
            deviceId_, static_cast<int32_t>(ret));
    }
    INFO_LOG("end to reset device: %d.", deviceId_);

    ret = aclFinalize();       //ȥ��ʼ�����������ͷŽ����е�ascendcl�����Դ
    if (ret != ACL_SUCCESS) {
        ERROR_LOG("finalize acl failed, errorCode = %d.", static_cast<int32_t>(ret));
    }
    INFO_LOG("end to finalize acl.");

}

Result SampleProcess::Process(const std::string &strImgFile, const std::string &strOmType)
{   //����ͼ���ļ�����ʹ��Ԥ���ص����ѧϰģ�ͽ���Ŀ����
    int ii;
    std::cin >> ii;
    ModelProcess modelProcess;    //����һ��ʵ��
    string omModelPath;
    if (strOmType == "YUV")
    {
        omModelPath = "../model/yuv_vis_drone_bird_original.om";
    }
    else if (strOmType == "BGR")
    {
        omModelPath = "../model/bgr_vis_drone_bird_original.om";
    }
    if (!g_strModelPath.empty())   //�ȼ�������
    {
        omModelPath = g_strModelPath;
    }

    Result ret = modelProcess.LoadModel(omModelPath.c_str());
    if (ret != SUCCESS) {
        ERROR_LOG("execute LoadModel failed");
        return FAILED;
    }

    ret = modelProcess.CreateDesc();
    if (ret != SUCCESS) {
        ERROR_LOG("execute CreateDesc failed");
        return FAILED;
    }

    ret = modelProcess.CreateOutput();
    if (ret != SUCCESS) {
        ERROR_LOG("execute CreateOutput failed");
        return FAILED;
    }
    int jj;
    std::cin >> jj;


    string testFile[] = { strImgFile };

    for (size_t index = 0; index < sizeof(testFile) / sizeof(testFile[0]); ++index) {
        INFO_LOG("start to process file:%s", testFile[index].c_str());

        Rect rect;
        ret = modelProcess.CreateInput2(testFile[index], 0, 0, rect);
        if (ret != SUCCESS) {
            ERROR_LOG("CreateInputBuf failed");
            return FAILED;
        }

        auto start = std::chrono::high_resolution_clock::now();
        ret = modelProcess.Execute();
        if (ret != SUCCESS) {
            ERROR_LOG("execute inference failed");
            modelProcess.DestroyInput();
            return FAILED;
        }
        auto executeEnd = std::chrono::high_resolution_clock::now();               //���ڵ�һ��ʱ��   ����Ǿ�̬��Ա����������Ҫָ������ʵ��
        INFO_LOG("model Execute use time:%ld ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(executeEnd - start).count());

        //modelProcess.OutputModelResult(modelId);
        //modelProcess.DumpModelOutputResult();

        vector<BBox> boxes = modelProcess.Postprocess(1920, 1080);     //ģ�ͺ���������һ�����ļ�����    �����е�һ������<BBox>
        auto postEnd = std::chrono::high_resolution_clock::now();
        INFO_LOG("model Post use time:%ld ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(postEnd - start).count());

        cv::Mat rgbImg = cv::imread(testFile[index]);          //设备通常使用BGR格式。因此，当您使用cv2.imread()函数读取图像时，即使图像文件本身是RGB格式，OpenCV也会将其转换为BGR格式
        for (size_t i = 0; i < boxes.size(); ++i)   
        {
            INFO_LOG("lx: %d, ly: %d, rx: %d, ry: %d, score: %lf; class id: %d",
            boxes[i].rect.ltX, boxes[i].rect.ltY, boxes[i].rect.rbX, boxes[i].rect.rbY, boxes[i].score, boxes[i].cls);
            cv::Scalar colorTemp(0, 255, 0);  //����һ��ʵ�����󣬾��Ƕ�ͨ��ģ����
            if (boxes[i].cls == 1)       //  ����趨����������boxes =1 �� �Ǿ�ִ��һ�������ǩ
            {
                colorTemp = cv::Scalar(0, 0, 255);
            }
            cv::rectangle(rgbImg, cv::Rect(boxes[i].rect.ltX, boxes[i].rect.ltY, boxes[i].rect.rbX - boxes[i].rect.ltX, boxes[i].rect.rbY - boxes[i].rect.ltY),
                          colorTemp);     //����һ�����еĺ�������ʵ����������Ч��ָ��

            std::string strCLS = std::to_string(boxes[i].cls);
            cv::putText(rgbImg, strCLS, cv::Point(boxes[i].rect.ltX, std::max(0, (int)(boxes[i].rect.ltY - 6))), cv::FONT_HERSHEY_COMPLEX, 0.5, colorTemp, 1, 8, 0);
            std::string strScore = std::to_string(boxes[i].score);
            strScore = strScore.substr(0, 5);
            cv::putText(rgbImg, strScore, cv::Point(boxes[i].rect.ltX + 15, std::max(0, (int)(boxes[i].rect.ltY - 6))), cv::FONT_HERSHEY_COMPLEX, 0.5, colorTemp, 1, 8, 0);
        }
        std::string strResult = testFile[index].substr(0, testFile[index].find_last_of('.')) + "_result.jpg";
        cv::imwrite(strResult, rgbImg);

        int iCnt = 0;
        while(iCnt < 3)
        {
            sleep(1);
            iCnt++;
        }
        // release model input buffer
        modelProcess.DestroyInput();
    }

    return SUCCESS;
}

