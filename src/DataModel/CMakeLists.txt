
# Add the Class directory to the sources
file(GLOB CLASS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

# Create the library or executable (choose one as needed)
add_library(DataModel SHARED ${CLASS_SOURCES})

# Optionally, include the Class directory for headers
target_include_directories(DataModel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(DataModel PUBLIC Base)

add_subdirectory(test)

