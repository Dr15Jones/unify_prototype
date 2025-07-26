cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
 set(CMAKE_CXX_STANDARD_REQUIRED True)
 
project(unify_prototype VERSION 1.0 LANGUAGES CXX)
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} 
#  ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Actually search for the library
find_package(TBB REQUIRED)
find_package(Catch2)

include(CTest)
enable_testing()

add_subdirectory(src)
