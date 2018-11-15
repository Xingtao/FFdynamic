## This file is part of project FFdynamic - 'https://github.com/Xingtao/FFdynamic'
## It could be used as a standalone library build file.

cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(ffdynamic VERSION 1.1.3 LANGUAGES CXX)

# Search cmake folder for dependencies
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_VERBOSE_MAKEFILE OFF)

# require a c++14
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# dependencies
find_package(FFmpeg REQUIRED)
if(NOT TARGET FFmpeg::FFmpeg)
  message(FATAL_ERROR, "Cannot find FFmpeg::FFmpeg target.")
endif()

find_package(Glog REQUIRED)
if(NOT TARGET Glog::Glog)
  message(FATAL_ERROR, "Cannot find Glog::Glog target")
endif()

#### Target & Properties ###########################################

# shared libraries need PIC
# set_property(TARGET objlib PROPERTY POSITION_INDEPENDENT_CODE 1)

set(FFdynamicSrc
  davBasis/davProc.cpp
  davBasis/davProcBuf.cpp
  davBasis/davUtil.cpp
  davBasis/davMessager.cpp
  davBasis/davDict.cpp
  davBasis/davWave.cpp
  davImpl/davImpl.cpp
  davImpl/davImplTravel.cpp
  davImpl/dataRelay/dataRelay.cpp
  davImpl/filter/ffmpegFilter.cpp
  davImpl/filter/filterGeneral.cpp
  davImpl/filter/scaleFilter.cpp
  davImpl/demux/ffmpegDemux.cpp
  davImpl/mux/ffmpegMux.cpp
  davImpl/videoEncode/ffmpegVideoEncode.cpp
  davImpl/videoDecode/ffmpegVideoDecode.cpp
  davImpl/audioEncode/ffmpegAudioEncode.cpp
  davImpl/audioDecode/ffmpegAudioDecode.cpp
  davImpl/videoMix/videoMix.cpp
  davImpl/videoMix/cellMixer/cellMixer.cpp
  davImpl/videoMix/cellMixer/cellLayout.cpp
  davImpl/videoMix/cellMixer/cellSetting.cpp
  davImpl/videoMix/cellMixer/cellScaleSyncer.cpp
  davImpl/audioMix/audioMix.cpp
  davImpl/audioMix/audioSyncer.cpp
  davStreamlet/davStreamlet.cpp
  davStreamlet/davStreamletBuilder.cpp
  davTools/audioResample/audioResample.cpp
  davTools/globalSignalHandle/globalSignalHandle.cpp
  )

add_library(ffdynamic SHARED ${FFdynamicSrc}) # Be Note: only shared library supported right now

target_include_directories(ffdynamic
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/davBasis>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/davImpl>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/davEvent>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/davStreamlet>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/davTools/globalSignalHandle>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/davTools/audioResample>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/davTools/imageToRawFrame>
      $<INSTALL_INTERFACE:include>
    PRIVATE
      davImpl/dataRelay   davImpl/demux       davImpl/mux
      davImpl/audioDecode davImpl/audioEncode davImpl/audioMix
      davImpl/filter      davImpl/videoDecode davImpl/videoEncode
      davImpl/videoMix    davImpl/videoMix/cellMixer
    )

add_library(ffdynamic::ffdynamic ALIAS ffdynamic)

target_link_libraries(ffdynamic PUBLIC FFmpeg::FFmpeg Glog::Glog)
target_compile_features(ffdynamic PUBLIC cxx_auto_type cxx_lambdas cxx_variadic_templates)
target_compile_options(ffdynamic
  PUBLIC $<$<CXX_COMPILER_ID:GNU>:-Wall -Wpedantic -g -O2 -fPIC>
  PUBLIC $<$<CXX_COMPILER_ID:Clang>:-Wall -Wpedantic -g -O2>
  )

#### Installation #########################################################
include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_PREFIX}/cmake)

install(TARGETS ffdynamic
  EXPORT ffdynamic-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

#This is required so that the exported target has the name JSONUtils and not jsonutils
set_target_properties(ffdynamic PROPERTIES EXPORT_NAME FFdynamic)

# export the targets to a script
install(EXPORT ffdynamic-targets
  FILE FFdynamicTargets.cmake
  NAMESPACE FFdynamic::
  DESTINATION ${INSTALL_CONFIGDIR}
  )

# create version
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/FFdynamicProcConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
    )

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FFdynamicConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/FFdynamicConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

# install cmakes
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/FFdynamicProcConfigVersion.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/FFdynamicConfig.cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindFFmpeg.cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindGlog.cmake
  DESTINATION ${INSTALL_CONFIGDIR}
  )

# install headers
install(FILES
  davBasis/davWave.h
  davBasis/davMessager.h
  davBasis/davUtil.h
  davBasis/davOption.h
  davBasis/davDict.h
  davBasis/davProcBuf.h
  davBasis/davProc.h
  davBasis/davTransmitor.h
  davBasis/davProcCtx.h
  davImpl/davImpl.h
  davImpl/davImplFactory.h
  davImpl/davImplUtil.h
  davImpl/davImplEventProcess.h
  davImpl/davImplTravel.h
  davImpl/ffmpegHeaders.h
  davStreamlet/davStreamlet.h
  davStreamlet/davStreamletBuilder.h
  davEvent/davDynamicEvent.h
  davEvent/davPeerEvent.h
  davTools/audioResample/audioResample.h
  davTools/imageToRawFrame/imageToRawFrame.h
  davTools/globalSignalHandle/globalSignalHandle.h
  davTools/imageToRawFrame/fmtScale.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

# exporting from the build tree
export(EXPORT ffdynamic-targets FILE ${CMAKE_CURRENT_BINARY_DIR}/FFdynamicTargets.cmake NAMESPACE FFdynamic::)

# register package in user's package registry
export(PACKAGE FFdynamic)

##### unit tests ##########################
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/davTests)
