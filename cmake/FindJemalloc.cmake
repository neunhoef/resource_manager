# Locate jemalloc library
# This module defines:
#  JEMALLOC_FOUND - if false, do not try to link to jemalloc
#  JEMALLOC_LIBRARIES - libraries to link against
#  JEMALLOC_INCLUDE_DIRS - directories to include

find_path(JEMALLOC_INCLUDE_DIR
  NAMES jemalloc/jemalloc.h
  PATHS
    /usr/local/include
    /usr/include
    /opt/local/include
)

find_library(JEMALLOC_LIBRARY
  NAMES jemalloc
  PATHS
    /usr/local/lib
    /usr/lib
    /usr/lib/x86_64-linux-gnu
    /opt/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Jemalloc DEFAULT_MSG JEMALLOC_LIBRARY JEMALLOC_INCLUDE_DIR)

if(JEMALLOC_FOUND)
  set(JEMALLOC_LIBRARIES ${JEMALLOC_LIBRARY})
  set(JEMALLOC_INCLUDE_DIRS ${JEMALLOC_INCLUDE_DIR})
  mark_as_advanced(JEMALLOC_INCLUDE_DIR JEMALLOC_LIBRARY)
endif() 