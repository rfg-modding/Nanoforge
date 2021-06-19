
cmake_minimum_required (VERSION 3.8)
project ("Nanoforge")

# Set this to true to enable ASAN. It can catch many common memory bugs. More info here: https://docs.microsoft.com/en-us/cpp/sanitizers/asan?view=msvc-160
# It's disabled by default because it causes the app to run much slower. ASAN should be disabled for release builds.
set(ENABLE_ASAN FALSE)

set(CMAKE_CXX_STANDARD           20)
set(CMAKE_CXX_STANDARD_REQUIRED  ON)
set(CMAKE_CXX_EXTENSIONS         OFF)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set options for MSVC compiler (currently the only one Nanoforge is developed with)
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(ENABLE_ASAN)
        set(EXTRA_OPTIONS "/fsanitize=address")
    endif()

    set(CMAKE_CXX_FLAGS "/experimental:external /external:anglebrackets /external:W0 /EHsc ${EXTRA_OPTIONS}")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# Set config specific defines
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_definitions(DEBUG_BUILD)
endif()

# Include sub-projects.
add_subdirectory("Dependencies/RfgToolsPlusPlus")
add_subdirectory("Dependencies/spdlog")
add_subdirectory("Nanoforge")