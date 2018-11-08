cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(dehaze VERSION 1.0 LANGUAGES CXX)

# opencv 3.4.0 or newer
find_package(OpenCV 3.4.0 REQUIRED)

## find FFdynamic; set FFdynamic's cmake path (change it to your build)
list(INSERT CMAKE_MODULE_PATH 0 /usr/local/FFdynamic/cmake)

find_package(FFmpeg REQUIRED)
if(NOT TARGET FFmpeg::FFmpeg)
  message(FATAL_ERROR, "Cannot find FFmpeg::FFmpeg target.")
endif()

find_package(Glog REQUIRED)
if(NOT TARGET Glog::Glog)
  message(FATAL_ERROR, "Cannot find Glog::Glog target")
endif()

find_package(FFdynamic REQUIRED)

add_executable(testDehazor testDehazor.cpp ffdynaDehazor.cpp dehazor.cpp testCommon.cpp)
target_include_directories(testDehazor
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  PUBLIC ${OpenCV_INCLUDE_DIRS})

target_compile_features(testDehazor PUBLIC cxx_auto_type cxx_lambdas)
target_compile_options(testDehazor  PUBLIC $<$<CXX_COMPILER_ID:GNU>: -Wall -Wpedantic>
  PUBLIC $<$<CXX_COMPILER_ID:Clang>: -Wall -Wpedantic>)
target_link_libraries(testDehazor PUBLIC FFdynamic::FFdynamic ${OpenCV_LIBS})
