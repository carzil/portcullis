include(GoogleTest)

enable_testing()
add_subdirectory(gtest)

function(portcullis_test)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(ETEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    add_executable(${ETEST_NAME} ${ETEST_SOURCES})
    target_link_libraries(${ETEST_NAME} PRIVATE portcullis-core spdlog pybind11::embed gtest)
    gtest_add_tests(TARGET ${ETEST_NAME} TEST_LIST test)
    set_tests_properties(${portcullis_reactor_test} PROPERTIES TIMEOUT 5)
endfunction()

portcullis_test(NAME reactor-core-test SOURCES test_reactor_core.cpp)
portcullis_test(NAME reactor-io-test SOURCES test_reactor_io.cpp)
portcullis_test(NAME http-handle-test SOURCES test_http_handle.cpp)
