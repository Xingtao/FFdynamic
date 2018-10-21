# NOTE: this is a standalone CMake file. It is not included in FFdynamic build.

cmake_minimum_required(VERSION 3.2)

project(tools VERSION 1.1.3 LANGUAGES CXX)

set(CMAKE_VERBOSE_MAKEFILE off)

# Search cmake folder for dependencies
# get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)

find_package(FFmpeg REQUIRED)
if(NOT TARGET FFmpeg::FFmpeg)
  message(FATAL_ERROR, "Cannot find FFmpeg::FFmpeg target")
endif()

#### Target & Properties ###########################################
add_library(davtools STATIC #SHARED
  audioMixer/audioMixer.cpp
  audioResample/audioResample.cpp
  globalSignalHandle/globalSignalHandle.cpp
  )

target_include_directories(davtools
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/audioMixer>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/audioResample>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/globalSignalHandle>
    )

add_library(davtools::davtools ALIAS davtools)

target_link_libraries(davtools PUBLIC FFmpeg::FFmpeg)
target_compile_features(davtools PUBLIC cxx_auto_type cxx_lambdas)
target_compile_options(davtools
  PUBLIC $<$<CXX_COMPILER_ID:GNU>:-Wall -Wpedantic -O3 -fPIC>
  PUBLIC $<$<CXX_COMPILER_ID:Clang>:-Wall -Wpedantic -O3>)

# register package in user's package registry
export(PACKAGE Davtools)
