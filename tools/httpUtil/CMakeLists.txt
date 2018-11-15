#
#   1. This project is forked and modified from 'https://github.com/eidheim/Simple-Web-Server'
#   2. For asynchronized read/write, be aware our buffer/socket outlives the operation.
#

cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(libhttputil VERSION 1.0 LANGUAGES CXX)

set(CMAKE_VERBOSE_MAKEFILE ON)

# only depends one Boost::system
find_package(Boost REQUIRED COMPONENTS system)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


# header only library
add_library(httputil INTERFACE)
add_library(httputil::httputil ALIAS httputil)

# include
target_include_directories(httputil
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(httputil INTERFACE cxx_auto_type cxx_lambdas)
target_compile_options(httputil INTERFACE $<$<CXX_COMPILER_ID:GNU>: -Wall -Wpedantic>
  INTERFACE $<$<CXX_COMPILER_ID:Clang>: -Wall -Wpedantic>)

# Boost::system: may be of problem if FindBoost shipped with CMake is not consistent
# with installed boost version, so use boost_system directly. not as a target.
target_link_libraries(httputil INTERFACE boost_system Threads::Threads)

# Installation instructions
include(GNUInstallDirs)

install(TARGETS httputil
    EXPORT httputil-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
  httpClient.h httpCommon.h httpServer.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# version
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/HttpUtilConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Export the targets to a script
install(EXPORT httputil-targets
  FILE HttpUtilTargets.cmake
  NAMESPACE HttpUtil::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

#configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/HttpUtilConfig.cmake.in
#    ${CMAKE_CURRENT_BINARY_DIR}/HttpUtilConfig.cmake
#    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}
#)

# Install the config, configversion and custom find modules
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/HttpUtilConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

## Exporting from the build tree
export(EXPORT httputil-targets FILE ${CMAKE_CURRENT_BINARY_DIR}/HttpUtilTargets.cmake NAMESPACE HttpUtil::)

#Register package in user's package registry
export(PACKAGE HttpUtil)

# testing
enable_testing()
add_executable(testHttpUtil testHttpUtil.cpp)
target_link_libraries(testHttpUtil httputil::httputil --coverage)
add_test(Tester testHttpUtil COMMAND testHttpUtil)
