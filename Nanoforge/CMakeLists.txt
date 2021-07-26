
cmake_minimum_required (VERSION 3.8)
project (Nanoforge VERSION 1.0.0
                  DESCRIPTION "Nanoforge"
                  LANGUAGES CXX)

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
add_executable (Nanoforge ${SOURCES}
    #Need to manually add the dear imgui files)
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/backends/imgui_impl_win32.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/backends/imgui_impl_dx11.cpp
    # tinyxml2 files
    ${CMAKE_SOURCE_DIR}/dependencies/tinyxml2/tinyxml2.cpp
    # imnodes files
    ${CMAKE_SOURCE_DIR}/dependencies/imnodes/imnodes.cpp
    #nativefiledialog files
    ${CMAKE_SOURCE_DIR}/dependencies/nativefiledialog/src/nfd_common.c
    ${CMAKE_SOURCE_DIR}/dependencies/nativefiledialog/src/nfd_win.cpp
    )

# Add include directories
target_include_directories(Nanoforge SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/
    ${CMAKE_SOURCE_DIR}/Nanoforge/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/Common/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/Dependencies/BinaryTools/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/RfgTools++/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/RfgTools++/RfgTools++/
    ${CMAKE_SOURCE_DIR}/Dependencies/imgui/
    ${CMAKE_SOURCE_DIR}/Dependencies/IconFontCppHeaders/
    ${CMAKE_SOURCE_DIR}/Dependencies/
    ${CMAKE_SOURCE_DIR}/Dependencies/DirectXTex/DirectXTex/
    ${CMAKE_SOURCE_DIR}/Dependencies/DirectXTex/
    ${CMAKE_SOURCE_DIR}/Dependencies/tinyxml2/
    ${CMAKE_SOURCE_DIR}/Dependencies/spdlog/include/
    ${CMAKE_SOURCE_DIR}/Dependencies/imnodes/
    ${CMAKE_SOURCE_DIR}/Dependencies/nativefiledialog/src/
    ${CMAKE_SOURCE_DIR}/Dependencies/nativefiledialog/src/include/
)

# Copy assets directory to binary dir after builds
add_custom_command(TARGET Nanoforge POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${CMAKE_SOURCE_DIR}/assets/
                   ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/)

# Add statically linked libs
target_link_libraries(Nanoforge PRIVATE Common)
target_link_libraries(Nanoforge PRIVATE RfgTools++)
target_link_libraries(Nanoforge PRIVATE spdlog)

# Have to manually link pre-built versions of these for the moment since it wasn't playing nice with cmake add_subdirectory
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(Nanoforge PRIVATE ${CMAKE_SOURCE_DIR}/lib/DirectXTex_d.lib)
else()
    target_link_libraries(Nanoforge PRIVATE ${CMAKE_SOURCE_DIR}/lib/DirectXTex.lib)
endif()

# Enable VS edit and continue if using MSVC, in a debug build and ASAN is disabled. ASAN doesn't support edit and continue.
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND NOT ENABLE_ASAN)
        target_compile_options(Nanoforge PRIVATE "/ZI")
        target_link_options(Nanoforge PRIVATE "/SAFESEH:NO")
    endif()
endif()