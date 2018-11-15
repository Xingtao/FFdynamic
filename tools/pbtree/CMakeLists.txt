#
# This file is part of FFdynamic - https://github.com/Xingtao/FFdynamic.git
# Initially, it aims to do translation between: Protobuf2 <--> Json. And . . . no time.
# So, only Protobuf3 <---> Json right now.
#
cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(libpbtree VERSION 1.0 LANGUAGES CXX)

# find Boost::system
find_package(Boost REQUIRED COMPONENTS system)

## find protobuf.
## if 'FindProtobuf' is an old version, define the target protobuf::libprotobuf as imported
find_package(Protobuf REQUIRED)
if (NOT TARGET protobuf::libprotobuf)
  message("protobuf::libprotobuf not a target, make it manully. location: "
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

# header only library
add_library(pbtree INTERFACE)
add_library(pbtree::pbtree ALIAS pbtree)
target_include_directories(pbtree
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(pbtree INTERFACE cxx_auto_type cxx_lambdas)
target_compile_options(pbtree INTERFACE $<$<CXX_COMPILER_ID:GNU>: -Wall -Wpedantic>
  INTERFACE $<$<CXX_COMPILER_ID:Clang>: -Wall -Wpedantic>)
target_link_libraries(pbtree INTERFACE boost_system protobuf::libprotobuf)

# testing
set(test_pbtree_proto ${CMAKE_CURRENT_SOURCE_DIR}/test.proto ${CMAKE_CURRENT_SOURCE_DIR}/test2.proto)
protobuf_generate_cpp(TEST_PROTO_SRCS TEST_PROTO_HDRS ${test_pbtree_proto})
# get_filename_component(TEST_PROTO_HDR_DIR ${TEST_PROTO_HDRS} DIRECTORY)
set(test_proto_dir ${CMAKE_CURRENT_BINARY_DIR})
add_executable(testPbtree testPbtree.cpp ${TEST_PROTO_SRCS})
target_include_directories(testPbtree PUBLIC ${test_proto_dir})
target_link_libraries(testPbtree pbtree::pbtree)
