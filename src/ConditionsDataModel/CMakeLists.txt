
# Add the ConditionsDataModel source directory to the sources
file(GLOB CLASS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

# Create the library or executable (choose one as needed)
add_library(ConditionsDataModel SHARED ${CLASS_SOURCES})

# Optionally, include the ConditionsDataModel directory for headers
target_include_directories(ConditionsDataModel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ConditionsDataModel PUBLIC Base DataModel)

#add_subdirectory(test)
