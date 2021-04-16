cmake_minimum_required(VERSION 2.8)
project(sylar)

# 为了让 cmake 能够把 __FILE__ 这个宏，换成相对路径。这样在用到路径的地方，能够保护好全路径不被暴露。
# 会自动的加入 make 参数，可以观察一下
# cmake 会出现一个 define 的警告（因为修改了系统级的macro），需要 -W 处理一下
include (cmake/utils.cmake)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function")

# add_link_option(-rdynamic)

include_directories(.)
# 机器统一安装的就不了，自动在 /usr/local/include 里
# include_directories(/apps/sylar/include)

#这个也不用，yaml cpp 是 make install 到 usr 了
#lib64 居然不是自动引入。。
link_directories(/usr/local/lib64/)
# find_library(YAMLCPP yaml-cpp)
# message(DEBUG ${YAMLCPP})

set(LIB_SRC 
    sylar/log.cc
    sylar/fiber.cc
    sylar/util.cc
    sylar/config.cc
    sylar/thread.cc
    )

add_library(sylar SHARED ${LIB_SRC})

# __FILE__改写，不然会警告（如果设定等级比较高，可能会直接报错）
add_definitions(-Wno-builtin-macro-redefined)

force_redefine_file_macro_for_sources(sylar)  # __FILE__ 相对路径

set(LIB_LIB
    sylar
    pthread
    yaml-cpp
    )

#add_library(sylar_static STATIC ${LIB_SRC})
#SET_TARGET_PROPERTIES (sylar_static PROPERTIES OUTPUT_NAME "sylar")

# 在一些其他的编译平台（比如macos），可能会在编译 config.cc.o 链接文件的时候，就需要先依赖 yaml-cpp.a 了
# add_dependencies(sylar yaml-cpp)

add_executable(test tests/test.cc)
add_dependencies(test sylar)

force_redefine_file_macro_for_sources(test)  # __FILE__ 相对路径

#这个为什么也要加 ${YAMLCPP}？不知道，不然会报错，额
target_link_libraries(test ${LIB_LIB})

add_executable(test_config tests/test_config.cc)
add_dependencies(test_config sylar)

force_redefine_file_macro_for_sources(test_config)  # __FILE__ 相对路径

# target_link_libraries(test_config sylar ${YAMLCPP})
target_link_libraries(test_config ${LIB_LIB})
# target_link_libraries(test_config sylar -L/apps/sylar/lib -lyaml-cpp)

add_executable(test_thread tests/test_thread.cc)
add_dependencies(test_thread sylar)
target_link_libraries(test_thread ${LIB_LIB})
force_redefine_file_macro_for_sources(test_thread)

add_executable(test_util tests/test_util.cc)
add_dependencies(test_util sylar)
target_link_libraries(test_util ${LIB_LIB})
force_redefine_file_macro_for_sources(test_util)

add_executable(test_fiber tests/test_fiber.cc)
add_dependencies(test_fiber sylar)
target_link_libraries(test_fiber ${LIB_LIB})
force_redefine_file_macro_for_sources(test_fiber)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)