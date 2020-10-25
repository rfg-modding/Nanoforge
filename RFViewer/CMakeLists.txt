
cmake_minimum_required (VERSION 3.8)
project (RFViewer VERSION 1.0.0
                  DESCRIPTION "RFViewer"
                  LANGUAGES CXX)

# Get list of source files in project src folders recursively
# Places in SOURCES variable
file(GLOB_RECURSE SOURCES 
    "${CMAKE_SOURCE_DIR}/RFViewer/*.cpp"
    "${CMAKE_SOURCE_DIR}/RFViewer/*.h"
    # TODO: Hook this up as a static lib once it's got a CMakeLists.txt. For now this works
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/Dependencies/BinaryTools/*.h
    #${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/Dependencies/BinaryTools/*.cpp
    )

# Add source to this project's executable.
add_executable (RFViewer ${SOURCES}
    #Need to manually add the dear imgui files)
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/backends/imgui_impl_win32.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/backends/imgui_impl_dx11.cpp
    #Do the same with im3d
    ${CMAKE_SOURCE_DIR}/Dependencies/im3d/im3d.cpp
    #List imgui-node-editor files
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor/NodeEditor/Include/imgui_node_editor.h
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor/NodeEditor/Source/crude_json.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor/NodeEditor/Source/crude_json.h
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor/NodeEditor/Source/imgui_bezier_math.h
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor/NodeEditor/Source/imgui_bezier_math.inl
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor/NodeEditor/Source/imgui_canvas.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor/NodeEditor/Source/imgui_canvas.h
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor/NodeEditor/Source/imgui_extra_math.h
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor/NodeEditor/Source/imgui_extra_math.inl
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor/NodeEditor/Source/imgui_node_editor_api.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor/NodeEditor/Source/imgui_node_editor_internal.h
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor/NodeEditor/Source/imgui_node_editor_internal.inl
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor/NodeEditor/Source/imgui_node_editor.cpp
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor/NodeEditor/Support/imgui_node_editor.natvis
    # tinyxml2 files
    ${CMAKE_SOURCE_DIR}/dependencies/tinyxml2/tinyxml2.cpp
    # imnodes files
    ${CMAKE_SOURCE_DIR}/dependencies/imnodes/imnodes.cpp
    )

# Add include directories
target_include_directories(RFViewer SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/
    ${CMAKE_SOURCE_DIR}/RFViewer/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/Common/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/Dependencies/BinaryTools/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/RfgTools++/
    ${CMAKE_SOURCE_DIR}/Dependencies/RfgToolsPlusPlus/RfgTools++/RfgTools++/
    ${CMAKE_SOURCE_DIR}/Dependencies/imgui/
    ${CMAKE_SOURCE_DIR}/Dependencies/IconFontCppHeaders/
    ${CMAKE_SOURCE_DIR}/Dependencies/
    ${CMAKE_SOURCE_DIR}/Dependencies/imgui-node-editor/NodeEditor/Include/
    ${CMAKE_SOURCE_DIR}/Dependencies/imgui-node-editor/NodeEditor/Source/
    ${CMAKE_SOURCE_DIR}/Dependencies/im3d
    ${CMAKE_SOURCE_DIR}/Dependencies/tinyxml2
    ${CMAKE_SOURCE_DIR}/Dependencies/spdlog/include/
    ${CMAKE_SOURCE_DIR}/Dependencies/imnodes/
)

# Copy assets directory to binary dir after builds
add_custom_command(TARGET RFViewer POST_BUILD 
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${CMAKE_SOURCE_DIR}/assets/
                   ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/)

# Add statically linked libs
target_link_libraries(RFViewer PRIVATE Common)
target_link_libraries(RFViewer PRIVATE RfgTools++)
target_link_libraries(RFViewer PRIVATE spdlog)