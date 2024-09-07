#include <iostream>
#include <map>
#include <string>

#include "samplemodel.h"

using namespace std;


std::string get_strModelpath = "model/best.om";

int main(int argc, char *argv[])       // 接收命令行参数
{
    // 图片文件路径
  std::string strImgFile;
  // om数据类型是BGR   是通过AIPP设置， 这个ATC转化得到输出的格式
  std::string strOmType;
  // 数据类型
  std::string strDataType;
    if(argc >3 ){
        strImgFile =argv[1];
        strOmType = argv[2];
        strDataType=argv[3];
    }

    if (strImgFile.empty() || strOmType.empty() || strDataType.empty())
    {
        cout << "Usage:parm faild=" << endl;   
        return -1;
    }


    SampleProcess sampleProcess;
    Result  ret = sampleProcess.Init();
    

  

}