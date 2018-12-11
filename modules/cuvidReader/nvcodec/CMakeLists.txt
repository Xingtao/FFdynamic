#
# Be Note: this is not a standalone CMakeList. it works only under FFdynamic project
#
# Files under this folder from Video_Codec_SDK_8.1.24. And only decoder used. Encoder still use FFmpeg's nvenc interface.

# set(CUDA_INSTALL_DIR /usr/local/cuda)
set(CUDA_INSTALL_DIR /home/web_server/cuda-9.0)
find_package(CUDA REQUIRED)

##########################################
# make stream decode part a static library
set(nvcodecSrc ${CMAKE_CURRENT_SOURCE_DIR}/NvCodec/NvDecoder/NvDecoder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/NvCodec/NvEncoder/NvEncoder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/NvCodec/NvEncoder/NvEncoderCuda.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Utils/BitDepth.cu
  ${CMAKE_CURRENT_SOURCE_DIR}/Utils/ColorSpace.cu
  ${CMAKE_CURRENT_SOURCE_DIR}/Utils/Resize.cu
  )

# a workaround
list(APPEND CUDA_NVCC_FLAGS "-std=c++11")
# this may not work for all cmake versions
set (CMAKE_CXX_FLAGS "-std=c++11")
set (CUDA_PROPAGATE_HOST_FLAGS ON)

cuda_add_library(nvcodec SHARED ${nvcodecSrc})
add_library(nvcodec::nvcodec ALIAS nvcodec)

target_include_directories(nvcodec PUBLIC
  $<BUILD_INTERFACE:${CUDA_INCLUDE_DIRS}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Utils>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/NvCodec>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/NvCodec/NvDecoder>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/NvCodec/NvEncoder>
  )

target_compile_features(nvcodec PUBLIC cxx_auto_type cxx_lambdas)
target_compile_options(nvcodec PUBLIC $<$<CXX_COMPILER_ID:GNU>: -Wall -Wpedantic>
  PUBLIC $<$<CXX_COMPILER_ID:Clang>: -Wall -Wpedantic>
  )

# set (CMAKE_SKIP_BUILD_RPATH true)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/nvcodec/NvCodec/Lib/linux/stubs/x86_64)
target_link_libraries(nvcodec PUBLIC nvcuvid nvidia-encode.so ${CUDA_LIBRARIES})
