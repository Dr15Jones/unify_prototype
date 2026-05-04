
find_package(Catch2)

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_catch2*.cc")
add_executable(testDataModel ${TEST_SOURCES}  )
target_link_libraries(testDataModel PUBLIC DataModel Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(testDataModel)