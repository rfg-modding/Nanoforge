
cmake_minimum_required (VERSION 3.8)
project (Project28 VERSION 1.0.0
                  DESCRIPTION "TestApp"
                  LANGUAGES CXX)

# Get list of source files in project src folders recursively
# Places in SOURCES variable
file(GLOB_RECURSE SOURCES 
    "${CMAKE_SOURCE_DIR}/Project28/*.cpp"
    "${CMAKE_SOURCE_DIR}/Project28/*.h"
    # TODO: Hook this up as a static lib once it's got a CMakeLists.txt. For now this works
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/Dependencies/BinaryTools/*.h
    #${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/Dependencies/BinaryTools/*.cpp
)

# Add source to this project's executable.
add_executable (Project28 ${SOURCES})

# Add include directories
target_include_directories(Project28 SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/
    ${CMAKE_SOURCE_DIR}/Project28/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/Common/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/RfgTools++/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/RfgTools++/RfgTools++/
)

# Add statically linked libs
target_link_libraries(Project28 PRIVATE Common)
target_link_libraries(Project28 PRIVATE RfgTools++)