cmake_minimum_required(VERSION 3.27)
project(ffreplay)

set(CMAKE_CXX_STANDARD 17)

add_library(ffreplay SHARED
        main.cpp)
add_compile_definitions(ffreplay -D_ITERATOR_DEBUG_LEVEL=0)

option(FFREPLAY_BUILD_INTO_PARENT_DIR "Build into the parent directory" OFF)

if (FFREPLAY_BUILD_INTO_PARENT_DIR)
    set_target_properties(ffreplay PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
endif()