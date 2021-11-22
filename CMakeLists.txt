
cmake_minimum_required (VERSION 3.8)
project ("Nanoforge")

# Set this to true to enable ASAN. It can catch many common memory bugs. More info here: https://docs.microsoft.com/en-us/cpp/sanitizers/asan?view=msvc-160
# It's disabled by default because it causes the app to run much slower. ASAN should be disabled for release builds.
set(ENABLE_ASAN FALSE)
set(ENABLE_PROFILER FALSE)
set(ENABLE_EDIT_AND_CONTINUE FALSE)
# Enables define of the same name. For behavior that should only exist in development builds.
# Separate from DEBUG_BUILD macro for testing dev builds in Release mode.
set(DEVELOPMENT_BUILD FALSE)
# Disable features that aren't compatible with windows 7
set(WIN7_COMPATIBILITY_MODE TRUE)

set(CMAKE_CXX_STANDARD           20)
set(CMAKE_CXX_STANDARD_REQUIRED  ON)
set(CMAKE_CXX_EXTENSIONS         OFF)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Disable windows min/max macros
add_compile_definitions(NOMINMAX)

if(ENABLE_PROFILER)
    add_compile_definitions(TRACY_ENABLE)
endif()
if(DEVELOPMENT_BUILD)
    add_compile_definitions(DEVELOPMENT_BUILD)
endif()
if(WIN7_COMPATIBILITY_MODE)
    add_compile_definitions(WIN7_COMPATIBILITY_MODE)
endif()

# Set options for MSVC compiler (currently the only one Nanoforge is developed with)
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(ENABLE_ASAN)
        set(EXTRA_OPTIONS "/fsanitize=address")
    endif()
endif()

# Set config specific defines
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_definitions(DEBUG_BUILD)
endif()

# Dependencies
add_subdirectory("Dependencies")

# Main projects
# Note: RfgTools++ is separate from the dependencies submodule since it's developed in tandem with Nanoforge. This allows it to have similar compile settings
#       E.g. Warnings are enabled for Nanoforge and RfgTools++, but disabled for third party libs.
add_subdirectory("Dependencies/RfgToolsPlusPlus")
add_subdirectory("Nanoforge")