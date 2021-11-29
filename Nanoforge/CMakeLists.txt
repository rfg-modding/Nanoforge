
cmake_minimum_required (VERSION 3.8)
project (Nanoforge VERSION 1.0.0
                  DESCRIPTION "Nanoforge"
                  LANGUAGES CXX)

# Set asset folder path depending on build type.
# Repo folder is used in debug builds and the exe local folder in release builds.
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_definitions(-DASSET_FOLDER_PATH=\"${CMAKE_SOURCE_DIR}/assets/\")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DASSET_FOLDER_PATH=\"./assets/\")
endif()

# Get list of source files in project src folders recursively
# Places in SOURCES variable
file(GLOB_RECURSE SOURCES
    "${CMAKE_SOURCE_DIR}/Nanoforge/*.cpp"
    "${CMAKE_SOURCE_DIR}/Nanoforge/*.h"
    # TODO: Hook this up as a static lib once it's got a CMakeLists.txt. For now this works
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/Dependencies/BinaryTools/*.h
    )

add_compile_definitions(IMGUI_IMPL_WIN32_DISABLE_LINKING_XINPUT IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)

# Add source to this project's executable.
add_executable(Nanoforge ${SOURCES})

# Add include directories
target_include_directories(Nanoforge PRIVATE
    ${CMAKE_SOURCE_DIR}/
    ${CMAKE_SOURCE_DIR}/Nanoforge/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/Common/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/Dependencies/BinaryTools/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/RfgTools++/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/RfgTools++/RfgTools++/
    ${CMAKE_SOURCE_DIR}/Dependencies/
    SYSTEM INTERFACE PRIVATE # The compiler should ignore warnings in folders below this
    ${CMAKE_SOURCE_DIR}/Dependencies/imgui/
    ${CMAKE_SOURCE_DIR}/Dependencies/IconFontCppHeaders/
    ${CMAKE_SOURCE_DIR}/Dependencies/DirectXTex/DirectXTex/
    ${CMAKE_SOURCE_DIR}/Dependencies/DirectXTex/
    ${CMAKE_SOURCE_DIR}/Dependencies/tinyxml2/
    ${CMAKE_SOURCE_DIR}/Dependencies/spdlog/include/
    ${CMAKE_SOURCE_DIR}/Dependencies/imnodes/
    ${CMAKE_SOURCE_DIR}/Dependencies/nativefiledialog/src/
    ${CMAKE_SOURCE_DIR}/Dependencies/nativefiledialog/src/include/
    ${CMAKE_SOURCE_DIR}/Dependencies/imgui_markdown/
    ${CMAKE_SOURCE_DIR}/Dependencies/tracy/
)

# Copy assets directory to binary dir after builds
add_custom_command(TARGET Nanoforge POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${CMAKE_SOURCE_DIR}/assets/
                   ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/)

# Add statically linked libs
target_link_libraries(Nanoforge PRIVATE NanoforgeDependencies)
target_link_libraries(Nanoforge PRIVATE Winmm.dll) # Needed for timeBeginPeriod in Application.cpp
target_link_libraries(Nanoforge PRIVATE RfgTools++)

target_compile_options(Nanoforge PRIVATE /EHsc /W3 ${EXTRA_OPTIONS})

# Enable VS edit and continue if using MSVC, in a debug build and ASAN and the profiler are disabled. ASAN and tracy don't support edit and continue.
if(ENABLE_EDIT_AND_CONTINUE AND NOT ENABLE_ASAN AND NOT ENABLE_PROFILER)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            target_compile_options(Nanoforge PRIVATE "/ZI")
            target_link_options(Nanoforge PRIVATE "/SAFESEH:NO")
        endif()
    endif()
endif()