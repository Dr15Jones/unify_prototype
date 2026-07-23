
# Add the ConditionsHandling source directory to the sources
file(GLOB CLASS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

# Create the library or executable (choose one as needed)
add_library(ConditionsHandling SHARED ${CLASS_SOURCES})

# Optionally, include the ConditionsHandling directory for headers
target_include_directories(ConditionsHandling PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ConditionsHandling PUBLIC Base DataModel ConditionsDataModel Concurrency)

add_subdirectory(test)
