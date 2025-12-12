
# Add the Class directory to the sources
file(GLOB CLASS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

# Create the library or executable (choose one as needed)
add_library(ComponentBase SHARED ${CLASS_SOURCES})

# Optionally, include the Class directory for headers
target_include_directories(ComponentBase PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ComponentBase PUBLIC ProductHandling ControlFlow)

add_subdirectory(test)