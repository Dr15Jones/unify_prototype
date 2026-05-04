
find_package(Catch2)

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_catch2*.cc")
add_executable(testConcurrency ${TEST_SOURCES}  )
target_link_libraries(testConcurrency PUBLIC Concurrency Catch2::Catch2WithMain)

file(GLOB TEST2_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test2_catch2*.cc")
add_executable(testConcurrency2 ${TEST2_SOURCES}  )
target_link_libraries(testConcurrency2 PUBLIC Concurrency Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(testConcurrency2)
catch_discover_tests(testConcurrency)