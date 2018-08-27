cmake_minimum_required(VERSION 3.9)

project(portcullis VERSION 0.1 LANGUAGES C CXX ASM)

include(ExternalProject)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wno-pragmas -Wno-deprecated-declarations -fvisibility=hidden -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -D_DEBUG_ -fsanitize=address -DASAN_BUILD -DSPDLOG_DEBUG_ON")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

# prevent building tests by default
set(BUILD_TESTING OFF CACHE BOOL "Build tests")

set(PORTCULLIS_TESTING OFF CACHE BOOL "Build portcullis tests")

add_subdirectory(contrib/pybind11)
add_subdirectory(contrib/spdlog)

find_package(Threads)

set(Python_ADDITIONAL_VERSIONS 3.6)
find_package(PythonInterp REQUIRED)
find_package(PythonLibs REQUIRED)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}/src
    src/
    contrib/picohttpparser/
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

set(RESOURCES
    helpers.py
)

foreach(res ${RESOURCES})
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/res/${res}.cpp
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_res.py ${res} ${CMAKE_CURRENT_SOURCE_DIR}/res/${res} ${CMAKE_CURRENT_BINARY_DIR}/res/${res}.cpp
        COMMENT "Packing resource '${res}'..."
        DEPENDS gen_res.py res/${res}
    )
    set(GENERATED_RESOURCES ${GENERATED_RESOURCES} res/${res}.cpp)
endforeach()

set(EXECUTABLE_SOURCES
    src/main.cpp
    src/python/module.cpp
    src/util/resource.cpp
)

set(CORE_SOURCES
    src/core/context.cpp
    src/core/service.cpp

    src/coro/context.S
    src/coro/coro.cpp
    src/coro/handle.cpp
    src/coro/tcp_handle.cpp
    src/coro/reactor.cpp

    src/python/wrappers.cpp

    src/util/generic.cpp
    src/util/network/address.cpp
    src/util/python.cpp
    src/util/system.cpp
)

add_library(picohttpparser STATIC contrib/picohttpparser/picohttpparser.c)

add_library(portcullis-core STATIC ${CORE_SOURCES})
target_link_libraries(portcullis-core PRIVATE ${PYTHON_LIBRARIES} pybind11::embed spdlog systemd ${CMAKE_THREAD_LIBS_INIT} picohttpparser)

set_source_files_properties(${GENERATED_RESOURCES} PROPERTIES GENERATED TRUE)
add_executable(portcullis ${EXECUTABLE_SOURCES} ${GENERATED_RESOURCES})
target_link_libraries(portcullis PRIVATE portcullis-core ${PYTHON_LIBRARIES} pybind11::embed spdlog)

if (PORTCULLIS_TESTING)
    add_subdirectory(tests)
endif()
