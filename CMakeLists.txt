cmake_minimum_required(VERSION 3.29.0)
project(Detection)

set(CMAKE_CXX_STANDARD 17)


#变量和目录设置
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)   #动态库和exe文件都指定在bin目录下

#CMAKE_LIBRARY_OUTPUT_DIRECTORY   linux下动态库的输出路径
#CMAKE_ARCHIVE_OUTPUT_DIRECTORY   linux下静态库的输出路径

#动态库
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LIST)
add_library(commom
SHARED
 ${SRC_LIST}
 )

target_include_directories(commom  
PUBLIC
${PROJECT_SOURCE_DIR}/include
)



# 可执行文件
add_executable(Detection_main main.cpp)


target_include_directories(Detection_main
PUBLIC
${PROJECT_SOURCE_DIR}/include
)


# 链接库
target_link_libraries(Detection_main
PUBLIC
commom
)




