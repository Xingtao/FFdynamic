cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(interactive-live VERSION 1.0 LANGUAGES CXX)
# try not use  c++14
# set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

## find Boost::system
find_package(FFdynamic)
if (TARGET ffdynamic::ffdynamic)
  message("ffdynamic found -->")
else(TARGET ffdynamic::ffdynamic)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../FFdynamic ${CMAKE_CURRENT_BINARY_DIR}/FFdynamic)
  if (TARGET ffdynamic::ffdynamic)
    message("build ffdynamic inside ial service <--")
  endif()
endif()

## find Boost::system
find_package(Boost REQUIRED COMPONENTS system)

## find protobuf.
## if 'FindProtobuf' is an old version, define the target protobuf::libprotobuf as imported
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

## use libraries from tools: httpUtil a& pbtree
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

#################################################################
# ial service
add_executable(ialService ialMain.cpp ialService.cpp ../appCommon/appService.cpp)

target_include_directories(ialService
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../appCommon
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  )

target_compile_features(ialService PUBLIC cxx_auto_type cxx_lambdas)

target_compile_options(ialService
  PUBLIC $<$<CXX_COMPILER_ID:GNU>: -Wall -Wpedantic>
  PUBLIC $<$<CXX_COMPILER_ID:Clang>: -Wall -Wpedantic>)

target_link_libraries(ialService
  PUBLIC ffdynamic::ffdynamic httputil::httputil pbtree::pbtree ffdynaProto::ffdynaProto protobuf::libprotobuf
  PUBLIC boost_system Threads::Threads)
