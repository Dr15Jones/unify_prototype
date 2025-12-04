
# Add the Class directory to the sources
file(GLOB CLASS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

# Create the library or executable (choose one as needed)
add_library(ControlFlow SHARED ${CLASS_SOURCES})

# Optionally, include the Class directory for headers
target_include_directories(ControlFlow PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ControlFlow PUBLIC DataProductBase DataModel Base Concurrency)

add_subdirectory(test)