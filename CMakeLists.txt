cmake_minimum_required (VERSION 3.12)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")

project(pmwcas)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-long-long -pedantic -fPIC -march=native")

if (CMAKE_BUILD_TYPE EQUAL "Release")
  message("-- Release mode, all optimizations enabled")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Ofast")
endif ()


FUNCTION(PMWCAS_DEFINE_ENV)
  # Set backend to PMDK by default to build persistent version
  set(PMEM_BACKEND "PMDK" CACHE STRING "Persistent memory backend type")
  string(TOUPPER ${PMEM_BACKEND} PMEM_BACKEND)

  # Both volatile and persistent versions are supported, by setting PMEM_BACKEND to:
  # PMDK    : use PMDK for persistence
  # EMU     : use simple shared memory for emulating persistent memory. This
  #           should only be used for experimental and profiling purpose. No real
  #           persistence is guaranteed.
  # VOLATILE: turn off persistence and build a volatile version, no persistence
  #           whatsoever. Equivalent to the original MwCAS operation.
  #
  # If persistent memory support is turned on, in the code we define both PMEM and
  # the corresponding macro for the backend. Code that is agnostic to the backend
  # is wrapped by PMEM; code that is specific to the backend is wrapped around by
  # PMEMEMU (for using emulation) or PMDK (for using PMDK).
  if(${PMEM_BACKEND} STREQUAL "PMDK")
    add_definitions(-DPMEM)
    add_definitions(-DPMDK)
    message(STATUS "Persistence support: PMDK")
  elseif(${PMEM_BACKEND} STREQUAL "EMU")
    add_definitions(-DPMEM)
    add_definitions(-DPMEMEMU)
    message(STATUS "Persistence support: PMDK")
    message(STATUS "Persistence support: emulation")
  elseif(${PMEM_BACKEND} STREQUAL "VOLATILE")
    message(STATUS "Persistence support: off")
  else()
    message(FATAL_ERROR "Unsupported persistent memory backend: ${PMEM_BACKEND}")
  endif()

  # Descriptor capacity - default four words max
  set(DESC_CAP "4" CACHE STRING "Descriptor capacity")
  add_definitions(-DDESC_CAP=${DESC_CAP})
  message(STATUS "Descirptor capacity: ${DESC_CAP}")
ENDFUNCTION()

# The PMWCAS_DEFINE_ENV is used for libraries depend on it to properly setup compile definitions.
PMWCAS_DEFINE_ENV()

# Turn off RTM by default - only allowed when persistence is turned off (i.e.,
# PMEM_BACKEND == VOLATILE
option(WITH_RTM "Use RTM for installing descriptors" OFF)
if(WITH_RTM)
  add_definitions(-DRTM)
  message("-- RTM configured")
endif()

##################### PMDK ####################
if(${PMEM_BACKEND} STREQUAL "PMDK")
  set(PMDK_LIB_PATH "/usr/local/lib" CACHE STRING "PMDK lib install path")
  add_library(pmemobj SHARED IMPORTED)
  set_property(TARGET pmemobj PROPERTY IMPORTED_LOCATION ${PMDK_LIB_PATH}/libpmemobj.so)
endif()
##############################################


include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/src)

# Google Test
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.8.1
)
FetchContent_GetProperties(googletest)
if (NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif ()
include(GoogleTest)


# Google Log
FetchContent_Declare(
        glog
        GIT_REPOSITORY https://github.com/google/glog.git
        GIT_TAG v0.4.0
)
FetchContent_GetProperties(glog)
if (NOT glog_POPULATED)
  FetchContent_Populate(glog)
  set(WITH_GFLAGS OFF CACHE BOOL "we don't want gflags")
  add_subdirectory(${glog_SOURCE_DIR} ${glog_BINARY_DIR})
endif()


# Set the list of libraries that make up the pmwcas stack
set (PMWCAS_LINK_LIBS
  glog::glog
)

# Set the list of libraries that use pmwcas
set (PMWCAS_APPS_LINK_LIBS
  pmwcas
  glog::glog
  gtest_main 
)

set(LINUX_ENVIRONMENT_SOURCES
  src/environment/environment_linux.cc
)

# Set the link libraries to for test compilation
set (PMWCAS_TEST_LINK_LIBS ${PMWCAS_LINK_LIBS} "-lpthread -lnuma -fPIC")

# Set the link libraries to for benchmark binary compilation
set (PMWCAS_BENCHMARK_LINK_LIBS ${PMWCAS_BENCHMARK_LINK_LIBS} gflags::gflags "-fPIC -lpthread -lnuma")

enable_testing()

#Function to automate building test binaries with appropriate environment sources
FUNCTION(ADD_PMWCAS_TEST TEST_NAME)
  add_executable(${TEST_NAME} ${TEST_NAME}.cc ${PROJECT_SOURCE_DIR}/${LINUX_ENVIRONMENT_SOURCES})
  target_link_libraries(${TEST_NAME} ${PMWCAS_TEST_LINK_LIBS} ${PMWCAS_APPS_LINK_LIBS})
  gtest_add_tests(TARGET ${TEST_NAME})
ENDFUNCTION()

# Build each subdirectory
add_subdirectory(src/util)
add_subdirectory(src/environment)
add_subdirectory(src/common)
add_subdirectory(src/mwcas)


# Generate a shared library for applications to link
set_property(GLOBAL APPEND PROPERTY PMWCAS_SRC ${LINUX_ENVIRONMENT_SOURCES})

get_property(PMWCAS_SRC GLOBAL PROPERTY PMWCAS_SRC)
add_library(pmwcas SHARED ${PMWCAS_SRC})
if(${PMEM_BACKEND} STREQUAL "PMDK")
  set (PMWCAS_LINK_LIBS
    ${PMWCAS_LINK_LIBS}
    pmemobj
  )
endif()
target_link_libraries(pmwcas PUBLIC ${PMWCAS_LINK_LIBS})
