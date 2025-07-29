
find_package(Catch2)

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_catch2*.cc")
add_executable(testBase ${TEST_SOURCES}  )
target_link_libraries(testBase PUBLIC Base Catch2::Catch2)

include(CTest)
include(Catch)
catch_discover_tests(testBase)