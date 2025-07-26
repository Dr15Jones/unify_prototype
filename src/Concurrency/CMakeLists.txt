
# Add the Class directory to the sources
file(GLOB CLASS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

# Create the library or executable (choose one as needed)
add_library(Concurrency SHARED ${CLASS_SOURCES})

# If you want to build an executable instead, use:
# add_executable(SignalsSlots ${CLASS_SOURCES})

# Optionally, include the Class directory for headers
target_include_directories(Concurrency PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


target_link_libraries(Concurrency PUBLIC TBB::tbb Utilities)

add_subdirectory(test)