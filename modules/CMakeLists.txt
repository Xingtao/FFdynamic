cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(modules VERSION 1.0 LANGUAGES CXX)

# find opencv
find_package(opencv 3.4.0 REQUIRED)

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

find_package(FFdynamic)
if (TARGET ffdynamic::ffdynamic)
  message("ffdynamic found -->")
else(TARGET ffdynamic::ffdynamic)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../FFdynamic ${CMAKE_CURRENT_BINARY_DIR}/FFdynamic)
  if (TARGET ffdynamic::ffdynamic)
    message("build ffdynamic inside module folder for tests <--")
  endif()
endif()

########################################################
# here we build opencv plugin module: libcvmodule
set(OpencvModuleSrc
  cvDnnDetect/cvDnnDetect.cpp
  cvPostDraw/cvPostDraw.cpp
  moduleStreamlet/cvStreamlet.cpp
  )

add_library(cvmodule_plugin SHARED ${OpencvModuleSrc}) # Be Note: only shared library supported right now

target_include_directories(ffdynamic
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/moduleEvent/>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/moduleStreamlet>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cvPostdraw>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cvDnnDetect>
      $<BUILD_INTERFACE:${OpenCV_INCLUDE_DIRS}>
      $<INSTALL_INTERFACE:include>
    )

add_library(cvmodule_plugin::cvmodule_plugin ALIAS cvmodule_plugin)

target_link_libraries(cvmodule_plugin PUBLIC ffdynamic::ffdynamic ${OpenCV_INCLUDE_DIRS})
target_compile_features(cvmodule_plugin PUBLIC cxx_auto_type cxx_lambdas cxx_variadic_templates)
target_compile_options(cvmodule_plugin
  PUBLIC $<$<CXX_COMPILER_ID:GNU>:-Wall -Wpedantic -g -O2 -fPIC>
  PUBLIC $<$<CXX_COMPILER_ID:Clang>:-Wall -Wpedantic -g -O2>
  )


##########################################
# each plugin module has an accompany test
add_executable(cvDnnDetect moduleTest/testCvDnnDetect.cpp moduleTest/testCommon.cpp)

set(bins cvDnnDetect)

foreach (bin ${bins})
  target_include_directories(${bin}
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/moduleTest>
  )
  target_compile_features(${bin} PUBLIC cxx_auto_type cxx_lambdas)
  target_compile_options(${bin} PUBLIC $<$<CXX_COMPILER_ID:GNU>: -Wall -Wpedantic>
                                PUBLIC $<$<CXX_COMPILER_ID:Clang>: -Wall -Wpedantic>)
  target_link_libraries(${bin} PUBLIC cvmodule_plugin::cvmodule_plugin)
endforeach()
