# FindFFmpeg.cmake
# Finds the FFmpeg libraries, will define and export the target:
#     FFmpeg::FFmpeg
#
# also, the following ones are defined
#  FFMPEG_FOUND
#  FFMPEG_INCLUDE_DIRS  - headers
#  FFMPEG_LIBRARIES     - static or dynamic linked libraries
#  FFMPEG_VERSION       - optionnal, may get version if pkg_config exists
#
#
# Searching preference
#    1. user provides path, with FFMPEG_CUSTOME_PATH set
#    2. pkgconfig if exist
#    3. default searching pathes if 1 & 2 not set
#
# Author: pxt
# Project: https://github.com/Xingtao/FFdynamic

include(FindPackageHandleStandardArgs)

# components needed
if (NOT FFmpeg_Find_Libs)
  set(FFmpeg_Find_Libs avformat avcodec avfilter avutil swscale swresample)
endif ()

macro (search_lib_and_path_with_hints lib lib_hints include_hints)
  # string (REPLACE ";" " " FFMPEG_SEARCHING_LIB_HINTS_STR "${lib_hints}")
  # string (REPLACE ";" " " FFMPEG_SEARCHING_INCLUDE_HINTS_STR "${include_hits}")
  string(TOUPPER ${lib} upperlib)
  find_path(FFMPEG_LIB${upperlib}_INLUCDE_DIR
    NAMES lib${lib}/${lib}.h
    HINTS ${include_hints}
    PATH_SUFFIXES include include/ffmpeg
    NO_DEFAULT_PATH
    )
  find_library(FFMPEG_LIB${upperlib}
    NAMES ${lib}
    HINTS ${lib_hints}
    PATH_SUFFIXES lib lib/ffmpeg
    NO_DEFAULT_PATH)
  # append newly found one
  if (FFMPEG_LIB${upperlib} AND FFMPEG_LIB${upperlib}_INLUCDE_DIR)
    list(APPEND FFMPEG_LIBRARIES ${FFMPEG_LIB${upperlib}})
    list(APPEND FFMPEG_INCLUDE_DIRS ${FFMPEG_LIB${upperlib}_INLUCDE_DIR})
    message ("-- Found lib --> " ${FFMPEG_LIB${upperlib}} " and path " ${FFMPEG_LIB${upperlib}_INLUCDE_DIR})
  else()
    message ("-- Not Found lib " ${FFMPEG_LIB${upperlib}} " or path " ${FFMPEG_LIB${upperlib}_INLUCDE_DIR})
  endif()
endmacro ()


# use pkg_config to get library and include dirs
macro (pkgconfig_get_search_lib_and_path_hints lib)
  string(TOUPPER ${lib} upperlib)
  pkg_check_modules(PC_${upperlib} REQUIRED lib${lib})
  set(${lib}_hint ${PC_${upperlib}_LIBRARY_DIRS})
  set(${lib}_include_hint ${PC_${upperlib}_INCLUDE_DIRS})
  list(APPEND pkgconfig_lib_hints ${${lib}_hint})
  list(APPEND pkgconfig_include_hints ${${lib}_include_hint})
endmacro ()


#### start processing: 1. get ffmpeg serach path hints
# if user provides ffmpeg path
if (FFMPEG_CUSTOME_PATH)
  set(FFMPEG_SEARCHING_LIB_HINTS ${FFMPEG_CUSTOME_PATH})
  set(FFMPEG_SEARCHING_INCLUDE_HINTS ${FFMPEG_SEARCHING_LIB_HINTS})
  message ("Will search FFmpeg with user provided dirs: " "${FFMPEG_SEARCHING_LIB_HINTS}")
endif()


# if not, try with pkg_config
find_package(PkgConfig)
if (NOT FFMPEG_CUSTOME_PATH AND PKG_CONFIG_FOUND)
  message ("--** Will use pkg_config to find FFmpeg")
  foreach (lib ${FFmpeg_Find_Libs})
    pkgconfig_get_search_lib_and_path_hints (${lib})
  endforeach()
  list(REMOVE_DUPLICATES pkgconfig_lib_hints)
  list(REMOVE_DUPLICATES pkgconfig_include_hints)
  set (FFMPEG_SEARCHING_LIB_HINTS ${pkgconfig_lib_hints})
  set (FFMPEG_SEARCHING_INCLUDE_HINTS ${pkgconfig_include_hints})
  list(LENGTH pkgconfig_lib_hints pkg_config_hints_len)
  message("-- pkg config lib: " ${FFMPEG_SEARCHING_LIB_HINTS})
  message("-- pkg config include: " ${FFMPEG_SEARCHING_INCLUDE_HINTS})
  if (${pkg_config_hints_len} EQUAL 0)
    set(PKG_CONFIG_FOUND_FFMPEG_FAIL ON)
  else (${pkg_config_hints_len} EQUAL 0)
    set(PKG_CONFIG_FOUND_FFMPEG_FAIL OFF)
  endif()
endif()


# if no user provided path and pkg_config fails, we try with more dirs then default PATH (explictily via HINTS)
if (NOT FFMPEG_CUSTOME_PATH AND PKG_CONFIG_FOUND AND PKG_CONFIG_FOUND_FFMPEG_FAIL)
  # explicit use following searching path (NO_DEFAULT_PATH will be set then)
  set(FFMPEG_SEARCHING_LIB_HINTS /usr /usr/local /sw /opt /opt/local)
  set(FFMPEG_SEARCHING_INCLUDE_HINTS ${FFMPEG_SEARCHING_LIB_HINTS})
  message ("--** Will search FFmpeg in default dirs: " "${FFMPEG_SEARCHING_LIB_HINTS}")
endif ()


#### 2. search and set libraries
if (NOT FFMPEG_FOUND) # may already have cached results
  message("--** Searching FFmpeg with dirs: " "${FFMPEG_SEARCHING_LIB_HINTS}")
  foreach (lib ${FFmpeg_Find_Libs})
    search_lib_and_path_with_hints (${lib} "${FFMPEG_SEARCHING_LIB_HINTS}" "${FFMPEG_SEARCHING_INCLUDE_HINTS}")
  endforeach ()

  list(LENGTH FFmpeg_Find_Libs numOfRequiredLibs)
  list(LENGTH FFMPEG_LIBRARIES numOfFoundLibs)
  if (${numOfRequiredLibs} EQUAL ${numOfFoundLibs})
    set(FFMPEG_FOUND ON)
    mark_as_advanced(FFMPEG_INCLUDE_DIRS FFMPEG_LIBRARIES)
    message("-- Found all required libs: " "${FFMPEG_LIBRARIES}")
  else ()
    message(ERROR_FATAL "Fail. Not found required ffmpeg libs." )
    message(ERROR_FATAL "-- Required: " "${FFmpeg_Find_Libs}")
    message(ERROR_FATAL "-- But Only Found: " "${FFMPEG_LIBRARIES}")
  endif()
endif()

#### 3. set target
if (FFMPEG_FOUND AND NOT TARGET FFmpeg::FFmpeg)
  message("Set target FFmpeg::FFmpeg:\n *Include: "
    "${FFMPEG_INCLUDE_DIRS}" "\n*Libraries: " "${FFMPEG_LIBRARIES}")
  add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
  set_target_properties(FFmpeg::FFmpeg PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "${FFMPEG_LIBRARIES}")
    # IMPORTED_LOCATION "/usr/local")
endif()

#get_target_property(location FFmpeg::FFmpeg IMPORTED_LOCATION)
#message("Test: location: " "${location}")
