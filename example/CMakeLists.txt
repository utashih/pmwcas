cmake_minimum_required (VERSION 3.12)
project(pmwcas-usecase)

include(FetchContent)


FetchContent_Declare(
        pmwcas
        GIT_REPOSITORY https://github.com/XiangpengHao/pmwcas.git
        GIT_TAG dev
)
FetchContent_GetProperties(pmwcas)
if (NOT pmwcas_POPULATED)
    FetchContent_Populate(pmwcas)
    add_subdirectory(${pmwcas_SOURCE_DIR})
    include_directories(${pmwcas_SOURCE_DIR} ${pmwcas_SOURCE_DIR}/include)
    PMWCAS_DEFINE_ENV()
endif()


add_executable(mwcas_test mwcas_test.cc)
target_link_libraries(mwcas_test pmwcas)
