
cmake_minimum_required (VERSION 3.8)
project (NanoforgeDependencies VERSION 1.0.0
                  DESCRIPTION "NanoforgeDependencies"
                  LANGUAGES CXX)

add_subdirectory("spdlog")

# Add source to this project's executable.
add_library(NanoforgeDependencies
    # Dear imgui
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/backends/imgui_impl_win32.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/backends/imgui_impl_dx11.cpp

    # tinyxml2 files
    ${CMAKE_SOURCE_DIR}/dependencies/tinyxml2/tinyxml2.cpp

    # Pugixml files - Note: Trying this library out. May incrementally replace tinyxml2 with this if it works well. I like it's API more and it's supposedly much faster.
    ${CMAKE_SOURCE_DIR}/dependencies/pugixml/src/pugixml.cpp

    # imnodes files
    ${CMAKE_SOURCE_DIR}/dependencies/imnodes/imnodes.cpp

    # nativefiledialog files
    ${CMAKE_SOURCE_DIR}/dependencies/nativefiledialog/src/nfd_common.c
    ${CMAKE_SOURCE_DIR}/dependencies/nativefiledialog/src/nfd_win.cpp

    # Tracy client files
    ${CMAKE_SOURCE_DIR}/dependencies/tracy/TracyClient.cpp

    # WinToast
    ${CMAKE_SOURCE_DIR}/dependencies/WinToast/src/wintoastlib.cpp
)

#TOOD: See if these can be removed here and just exist in nanoforge project
# Add include directories
target_include_directories(NanoforgeDependencies PRIVATE
    SYSTEM INTERFACE PRIVATE
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
    ${CMAKE_SOURCE_DIR}/Dependencies/pugixml/
    ${CMAKE_SOURCE_DIR}/Dependencies/WinToast/src/
)

target_link_libraries(NanoforgeDependencies PRIVATE Common)
target_link_libraries(NanoforgeDependencies PRIVATE spdlog)
# Have to manually link pre-built versions of these for the moment since it wasn't playing nice with cmake add_subdirectory
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(NanoforgeDependencies PRIVATE ${CMAKE_SOURCE_DIR}/lib/DirectXTex_d.lib)
else()
    target_link_libraries(NanoforgeDependencies PRIVATE ${CMAKE_SOURCE_DIR}/lib/DirectXTex.lib)
endif()

# Disable warnings on external libraries
target_compile_options(NanoforgeDependencies PRIVATE /wd26812 /wd4996)