cmake_minimum_required(VERSION 2.8)
project(sylar)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function")

include_directories(.)
# 机器统一安装的就不了，自动在 /usr/local/include 里
# include_directories(/apps/sylar/include)

#这个也不用，yaml cpp 是 make install 到 usr 了
#link_directories(/apps/sylar/lib/)

find_library(YAMLCPP libyaml-cpp.a)

set(LIB_SRC 
    sylar/log.cc
    sylar/util.cc
    sylar/config.cc
    )

add_library(sylar SHARED ${LIB_SRC})
#add_library(sylar_static STATIC ${LIB_SRC})
#SET_TARGET_PROPERTIES (sylar_static PROPERTIES OUTPUT_NAME "sylar")

add_executable(test tests/test.cc)
add_dependencies(test sylar)
target_link_libraries(test sylar)

add_executable(test_config tests/test_config.cc)
add_dependencies(test_config sylar)
target_link_libraries(test_config sylar yaml-cpp)
# target_link_libraries(test_config sylar -L/apps/sylar/lib -lyaml-cpp)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)