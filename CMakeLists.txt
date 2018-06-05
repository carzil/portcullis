cmake_minimum_required(VERSION 3.9)

project(portcullis VERSION 0.1)

include(ExternalProject)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wno-pragmas -Wno-deprecated-declarations")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fsanitize=address -D_DEBUG_")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

# externalproject_add(hyperscan_git
#     GIT_REPOSITORY https://github.com/intel/hyperscan.git
#     GIT_TAG "release-4.4"
#     PREFIX "${CMAKE_CURRENT_BINARY_DIR}/hyperscan"
#     CONFIGURE_COMMAND ""
#     UPDATE_COMMAND ""
#     INSTALL_COMMAND ""
# )

# prevent building tests by default
set(BUILD_TESTING OFF CACHE BOOL "Build tests")

add_subdirectory(libs/pybind11)
add_subdirectory(libs/spdlog)

find_package(Threads)

set(Python_ADDITIONAL_VERSIONS 3.6)
find_package(PythonInterp REQUIRED)
find_package(PythonLibs REQUIRED)

include_directories(
    ${SPDLOG_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}/src
    src/
)

execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.in
    ${CMAKE_CURRENT_BINARY_DIR}/src/version.h
)

file(GLOB_RECURSE resources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/res "res/**")

foreach(res ${resources})
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/res/${res}.cpp
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_res.py ${res} ${CMAKE_CURRENT_SOURCE_DIR}/res/${res} ${CMAKE_CURRENT_BINARY_DIR}/res/${res}.cpp
        COMMENT "Packing resource '${res}'..."
    )
    set(GENERATED_RESOURCES ${GENERATED_RESOURCES} res/${res}.cpp)
endforeach()

set(SOURCES
    src/main.cpp
    src/service.cpp
    src/resource.cpp
    src/loop.cpp
    src/handle.cpp
    src/buffer.cpp
)

set_source_files_properties(${GENERATED_RESOURCES} PROPERTIES GENERATED TRUE)
add_executable(portcullis ${SOURCES} ${GENERATED_RESOURCES})
target_link_libraries(portcullis PRIVATE ${PYTHON_LIBRARIES} pybind11::embed spdlog ${CMAKE_THREAD_LIBS_INIT})
