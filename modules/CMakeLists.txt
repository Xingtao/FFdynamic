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

# each plugin module has an accompany test
add_executable(cvDnnDetect cvDnnDetect/cvDnnDetect.cpp moduleCLI/testCvDnnDetect.cpp cliCommon.cpp)

set(bins cvDnnDetect)

foreach (bin ${bins})
  target_include_directories(${bin}
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/moduleCLI>
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/moduleEvent>
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cvDnnDetect>
    PUBLIC ${OpenCV_INCLUDE_DIRS}
  )
  target_compile_features(${bin} PUBLIC cxx_auto_type cxx_lambdas)
  target_compile_options(${bin} PUBLIC $<$<CXX_COMPILER_ID:GNU>: -Wall -Wpedantic>
                                PUBLIC $<$<CXX_COMPILER_ID:Clang>: -Wall -Wpedantic>)
  target_link_libraries(${bin} PUBLIC FFdynamic::FFdynamic ${OpenCV_LIBS})
endforeach()
