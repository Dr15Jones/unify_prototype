
# Add the Class directory to the sources
file(GLOB CLASS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

# Create the library or executable (choose one as needed)
add_library(DataProductBase SHARED ${CLASS_SOURCES})

# Optionally, include the Class directory for headers
target_include_directories(DataProductBase PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(DataProductBase PUBLIC )

#add_subdirectory(test)