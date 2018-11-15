# FindGlog.cmake
# Finds the Glog libraries and will define the TARGET: Glog::Glog
# Author: pxt
# Project: https://github.com/Xingtao/FFdynamic
#

include(FindPackageHandleStandardArgs)

# components needed
if (NOT Glog_Find_Libs)
  set(Glog_Find_Libs glog)
endif ()

#### start processing: 1. get glog serach path hints
# if user provides glog path
if (GLOG_CUSTOME_PATH)
  set(GLOG_SEARCHING_LIB_HINTS ${GLOG_CUSTOME_PATH})
  set(GLOG_SEARCHING_INCLUDE_HINTS ${GLOG_SEARCHING_LIB_HINTS})
  message ("Will search Glog with user provided dirs: " "${GLOG_SEARCHING_LIB_HINTS}")
endif()

# if not, try with pkg_config
find_package(PkgConfig)
if (NOT GLOG_CUSTOME_PATH AND PKG_CONFIG_FOUND)
  message ("--** Will use pkg_config to find Glog")
  pkg_check_modules(PC_GLOG REQUIRED libglog)
  set (GLOG_SEARCHING_LIB_HINTS ${PC_GLOG_LIBRARY_DIRS})
  set (GLOG_SEARCHING_INCLUDE_HINTS ${PC_GLOG_INCLUDE_DIRS})

  list(LENGTH GLOG_SEARCHING_LIB_HINTS GLOG_SEARCHING_LIB_HINTS_LEN)
  message("-- pkg config lib: " ${GLOG_SEARCHING_LIB_HINTS})
  message("-- pkg config include: " ${GLOG_SEARCHING_INCLUDE_HINTS})
  if (${GLOG_SEARCHING_LIB_HINTS_LEN} EQUAL 0)
    set(PKG_CONFIG_FOUND_GLOG_FAIL ON)
  else (${GLOG_SEARCHING_LIB_HINTS_LEN} EQUAL 0)
    set(PKG_CONFIG_FOUND_GLOG_FAIL OFF)
  endif()
endif()


# if no user provided path and pkg_config fails, we try with more dirs then default PATH (explictily via HINTS)
if (NOT GLOG_CUSTOME_PATH AND PKG_CONFIG_FOUND AND PKG_CONFIG_FOUND_GLOG_FAIL)
  # explicit use following searching path (NO_DEFAULT_PATH will be set then)
  set(GLOG_SEARCHING_LIB_HINTS /usr /usr/local /sw /opt /opt/local /usr/lib/x86_64-linux-gnu)
  set(GLOG_SEARCHING_INCLUDE_HINTS ${GLOG_SEARCHING_LIB_HINTS})
  message ("--** Will search Glog in default dirs: " "${GLOG_SEARCHING_LIB_HINTS}")
endif ()


#### 2. search and set libraries
if (NOT GLOG_FOUND) # may already have cached results
  message("--** Searching Glog with dirs: " "${GLOG_SEARCHING_LIB_HINTS}")
  find_path(GLOG_LIBGLOG_INLUCDE_DIR
    NAMES logging.h
    HINTS ${GLOG_SEARCHING_INCLUDE_HINTS}
    PATH_SUFFIXES include glog include/glog
    NO_DEFAULT_PATH
    )
  find_library(GLOG_LIBGLOG
    NAMES glog
    HINTS ${GLOG_SEARCHING_LIB_HINTS}
    PATH_SUFFIXES lib glog lib/glog
    NO_DEFAULT_PATH)
  # append newly found one
  if (GLOG_LIBGLOG AND GLOG_LIBGLOG_INLUCDE_DIR)
    set(GLOG_LIBRARIES ${GLOG_LIBGLOG})
    set(GLOG_INCLUDE_DIRS ${GLOG_LIBGLOG_INLUCDE_DIR})
    message ("-- Found lib --> " ${GLOG_LIBRARIES} " and path " ${GLOG_INCLUDE_DIRS})
    set(GLOG_FOUND ON)
  else()
    message ("-- Not Found lib --> " ${GLOG_LIBRARIES} " or path " ${GLOG_INCLUDE_DIRS})
  endif()
endif()

#### 3. set target
if (GLOG_FOUND AND NOT TARGET Glog::Glog)
  message("Set target Glog::Glog:\n *Include: "
    "${GLOG_INCLUDE_DIRS}" "\n*Libraries: " "${GLOG_LIBRARIES}")
  add_library(Glog::Glog INTERFACE IMPORTED)
  set_target_properties(Glog::Glog PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${GLOG_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "${GLOG_LIBRARIES}")
    # IMPORTED_LOCATION "/usr/local")
endif()

#get_target_property(location Glog::Glog IMPORTED_LOCATION)
#message("Test: location: " "${location}")
