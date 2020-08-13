
# Top level project. Treating this like a VS solution
cmake_minimum_required (VERSION 3.8)
project ("RFViewer")

## Update git submodules
#include("GitSubmodule.cmake")
#
## Error if submodules not loaded. Just check one of them for now
#if(NOT EXISTS "${PROJECT_SOURCE_DIR}/dependencies/RfgToolsPlusPlus/CMakeLists.txt")
#    message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
#endif()

set(CMAKE_CXX_STANDARD           20)
set(CMAKE_CXX_STANDARD_REQUIRED  ON)
set(CMAKE_CXX_EXTENSIONS         OFF)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set config specific defines
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_definitions(DEBUG_BUILD)
endif()

# Include sub-projects.
add_subdirectory("Dependencies/RfgToolsPlusPlus")
#add_subdirectory("Dependencies/RfgToolsPlusPlus/Common") # TODO: Move into own git submodule
add_subdirectory("RFViewer")