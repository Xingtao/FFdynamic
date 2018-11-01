cmake_minimum_required(VERSION 3.2 FATAL_ERROR

# Dynamic Add/Delete Object Detectors with live streaming
project(ObjDetectors VERSION 1.0 LANGUAGES CXX)

## Dependency Check
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# opencv 3.4.0 or newer
find_package(opencv 3.4.0 REQUIRED)
## find Boost::system
find_package(Boost REQUIRED COMPONENTS system)
find_package(Glog REQUIRED)

find_package(FFdynamic)
if (TARGET ffdynamic::ffdynamic)
  message("use installed ffdynamic library ")
if (TARGET ffdynamic::ffdynamic)
  message("build ffdynamic inside dynaObjDetector service --> ")
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../FFdynamic ${CMAKE_CURRENT_BINARY_DIR}/FFdynamic)
endif()

## find protobuf.
# if 'FindProtobuf' is an old version, define the target protobuf::libprotobuf as imported
find_package(Protobuf REQUIRED)
if (NOT TARGET protobuf::libprotobuf)
  message("protobuf::libprotobuf not export as a target, make it manully. location: "
    ${PROTOBUF_LIBRARY} ", include " ${PROTOBUF_INCLUDE_DIRS})
  add_library(protobuf::libprotobuf SHARED IMPORTED GLOBAL)
  set_target_properties(protobuf::libprotobuf PROPERTIES
    IMPORTED_LOCATION ${PROTOBUF_LIBRARY}
    INTERFACE_LINK_LIBRARIES ${PROTOBUF_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${PROTOBUF_INCLUDE_DIRS}
    )
else (NOT TARGET protobuf::libprotobuf)
  message("target protobuf::libprotobuf found")
endif()

## use libraries from tools: httpUtil & pbtree
if (TARGET httputil::httputil)
  message("[httputil found as imported target]")
else(TARGET httputil::httputil)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../tools/httpUtil ${CMAKE_CURRENT_BINARY_DIR}/tools/httpUtil)
  if (TARGET httputil::httputil)
    message("[build ffdynamic inside ial service]")
  endif()
endif()

if (TARGET pbtree::pbtree)
  message("[pbtree found as imported target]")
else(TARGET pbtree::pbtree)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../tools/pbtree ${CMAKE_CURRENT_BINARY_DIR}/tools/pbtree)
  if (TARGET pbtree::pbtree)
    message("[build pbtree inside ial service]")
  endif()
endif()

## proto library
if (NOT TARGET ffdynaProto::ffdynaProto)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../appCommon/protos ${CMAKE_CURRENT_BINARY_DIR}/protos)
  if (TARGET ffdynaProto::ffdynaProto)
    message("[build ffdynaProto inside ffdynamic service]")
  endif()
else (NOT TARGET ffdynaProto::ffdynaProto)
  message("[find ffdynaProto::ffdynaProto target]")
endif()

########################################################
# Dynamic Add/Delete Object Detectors with live streaming
add_executable(dynaDetector appService.cpp dynaDetectorService.cpp dynaDetectorMain.cpp)
target_include_directories(ialService
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../appCommon
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  )
target_compile_features(dynaDetector PUBLIC cxx_auto_type cxx_lambdas)
target_compile_options(dynaDetector
  PUBLIC $<$<CXX_COMPILER_ID:GNU>: -Wall -Wpedantic>
  PUBLIC $<$<CXX_COMPILER_ID:Clang>: -Wall -Wpedantic>)
target_link_libraries(dynaDetector
  PUBLIC ffdynamic::ffdynamic httputil::httputil pbtree::pbtree ffdynaProto::ffdynaProto protobuf::libprotobuf
  PUBLIC boost_system Threads::Threads)
