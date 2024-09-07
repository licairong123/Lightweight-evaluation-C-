cmake_minimum_required(VERSION 3.29.0)
project(Detection)

set(CMAKE_CXX_STANDARD 17)


#变量和目录设置
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)   #动态库和exe文件都指定在bin目录下

#CMAKE_LIBRARY_OUTPUT_DIRECTORY   linux下动态库的输出路径
#CMAKE_ARCHIVE_OUTPUT_DIRECTORY   linux下静态库的输出路径

#动态库
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LIST)     # 自动收集指定目录下的所有源文件，并保存到SRC_LIST变量中
add_library(commom                 #设置动态库的名字， 依赖于SRC_LIST中的源文件 
SHARED                             
 ${SRC_LIST}
 )

target_include_directories(commom  
PUBLIC
${PROJECT_SOURCE_DIR}/include
)                                       #添加源文件的头文件的搜索路径



# 可执行文件
add_executable(Detection_main main.cpp)


target_include_directories(Detection_main
PUBLIC
${PROJECT_SOURCE_DIR}/include
)                                       #添加头文件的搜索路径
     # 包括头文件目录

# 链接库
target_link_libraries(Detection_main
PUBLIC
commom
)


install(TARGETS Detection_main DESTINATION bin)  # 安装可执行文件

