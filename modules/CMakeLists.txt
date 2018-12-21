cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(modules VERSION 1.0 LANGUAGES CXX)

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
    message("build ffdynamic inside module folder for tests")
  endif()
endif()

########################################################
# here we build opencv plugin module: libcvmodule
# find opencv
find_package(opencv 3.4.0 REQUIRED)
set(cvModuleSrc
  cvPostDraw/cvPostDraw.cpp
  objDetect/cvDnnDetect/cvDnnDetect.cpp
  )
set(DEPEND_LIBS ${OpenCV_LIBS})
set(DEPEND_INCS ${OpenCV_INCLUDE_DIRS})

#######################################################################
# CUDA needed if DARKNET_CUDA or CUVID is defined
if (DARKNET_CUDA OR CUVID)
  find_package(CUDA REQUIRED)
  set(DEPEND_LIBS ${DEPEND_LIBS} ${CUDA_LIBRARIES})
  set(DEPEND_INCS ${DEPEND_INCS} ${CUDA_INCLUDE_DIRS})
  # TODO: what about cuDnn for darknet ?
endif()

#######################################################################
# build darknet when DARKNET=ON, also pass DARKNET_INC, DARKNET_LIB
if (DARKNET)
  # require cuda/cudnn if darknet compiled with CUDA/CUDNN option on
  if (NOT DARKNET_LIB OR NOT DARKNET_INC_DIR)
    message(FATAL_ERROR, " ERROR ==> Should provide 'darknet' library and include path")
  endif ()
  set(darknetModuleSrc objDetect/darknetDetect/darknetDetect.cpp)
  set(DEPEND_LIBS ${DEPEND_LIBS} ${DARKNET_LIB})
  set(DEPEND_INCS ${DEPEND_INCS} ${DARKNET_INC_DIR})
endif()

########################################################################
# build libmodule_common
set(ModuleSrc
  ${cvModuleSrc}
  ${darknetModuleSrc}
  moduleStreamlet/objDetectStreamlet.cpp
  )

add_library(module_plugin SHARED ${ModuleSrc}) # Be Note: only shared library supported right now
target_include_directories(ffdynamic
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cvPostdraw>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/objDetect>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/objDetect/cvDnnDetect>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/objDetect/darknetDetect>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/moduleEvent>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/moduleStreamlet>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/moduleCommon>
      $<BUILD_INTERFACE:${DEPEND_INCS}>
      $<INSTALL_INTERFACE:include>
      )

add_library(module_plugin::module_plugin ALIAS module_plugin)
target_link_libraries(module_plugin PUBLIC ffdynamic::ffdynamic ${DEPEND_LIBS})
target_compile_features(module_plugin PUBLIC cxx_auto_type cxx_lambdas cxx_variadic_templates)
target_compile_options(module_plugin
  PUBLIC $<$<CXX_COMPILER_ID:GNU>:-Wall -Wpedantic -g -O2 -fPIC>
  PUBLIC $<$<CXX_COMPILER_ID:Clang>:-Wall -Wpedantic -g -O2>
  )

################################################
# TESTS: each plugin module has an accompany test
add_executable(testObjDnnDetect moduleTest/testObjDnnDetect.cpp moduleTest/testCommon.cpp)
set(bins testObjDnnDetect)

foreach (bin ${bins})
  target_include_directories(${bin}
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/moduleTest>
  )
  target_compile_features(${bin} PUBLIC cxx_auto_type cxx_lambdas)
  target_compile_options(${bin} PUBLIC $<$<CXX_COMPILER_ID:GNU>: -Wall -Wpedantic>
                                PUBLIC $<$<CXX_COMPILER_ID:Clang>: -Wall -Wpedantic>)
  target_link_libraries(${bin} PUBLIC module_plugin::module_plugin)
endforeach()
