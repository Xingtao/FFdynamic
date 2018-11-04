cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(libffdyna_proto VERSION 1.0 LANGUAGES CXX)

# require a c++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# generate proto sources
set(ial_proto_dir ${CMAKE_CURRENT_SOURCE_DIR}/ial)
set(dyna_detect_proto_dir ${CMAKE_CURRENT_SOURCE_DIR}/dynaDnnDetect)
set(ffdyna_origin_proto
  ${CMAKE_CURRENT_SOURCE_DIR}/davWaveSetting.proto
  ${CMAKE_CURRENT_SOURCE_DIR}/davStreamletSetting.proto
  ${CMAKE_CURRENT_SOURCE_DIR}/appGlobalSetting.proto
  ${ial_proto_dir}/ialConfig.proto
  ${ial_proto_dir}/ialRequest.proto
  ${dyna_detect_proto_dir}/dynaDnnDetect.proto
  )

foreach(protofile ${ffdyna_origin_proto})
  get_filename_component(filename ${protofile} NAME)
  configure_file(${protofile} ${CMAKE_CURRENT_BINARY_DIR}/${filename})
  list(APPEND proto_build_src ${CMAKE_CURRENT_BINARY_DIR}/${filename})
  message ("cp " ${protofile} " to " ${CMAKE_CURRENT_BINARY_DIR}/${filename})
endforeach()

protobuf_generate_cpp(FFDYNA_PROTO_SRCS FFDYNA_PROTO_HDRS ${proto_build_src})
protobuf_generate_python(FFDYNA_PYTHON_PROTO_SRCS ${proto_build_src})
add_custom_target(python_protos_for_test ALL DEPENDS ${FFDYNA_PYTHON_PROTO_SRCS})

# build target
add_library(ffdynaProto STATIC ${FFDYNA_PROTO_SRCS}) # static for simplicity
add_library(ffdynaProto::ffdynaProto ALIAS ffdynaProto)
target_include_directories(ffdynaProto
  INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(ffdynaProto INTERFACE cxx_auto_type cxx_lambdas)
target_compile_options(ffdynaProto INTERFACE $<$<CXX_COMPILER_ID:GNU>: -Wall -Wpedantic>
  INTERFACE $<$<CXX_COMPILER_ID:Clang>: -Wall -Wpedantic>)
target_link_libraries(ffdynaProto PUBLIC protobuf::libprotobuf)

# TODO: install part
