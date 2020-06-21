#include "Im3dRenderer.h"
#include "Im3dHelpers.h"
#include "ext/im3d_config.h"
#include <im3d.h>
#include <im3d_math.h>
#include "imgui/imgui.h"
#include <dxgi.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include "render/util/DX11Helpers.h"
#include "render/camera/Camera.h"

bool Im3dRenderer::Init(ID3D11Device* d3d11Device, ID3D11DeviceContext* d3d11Context, HWND hwnd, Camera* camera)
{
    d3d11Device_ = d3d11Device;
    d3d11Context_ = d3d11Context;
    hwnd_ = hwnd;
    camera_ = camera;

    //Create point shaders
    pointShader_.VertexShaderBlob = LoadCompileShader("vs_4_0", "assets/shaders/im3d.hlsl", "VERTEX_SHADER\0POINTS\0");
    if (!pointShader_.VertexShaderBlob) return false;
    DxCheck(d3d11Device_->CreateVertexShader(pointShader_.VertexShaderBlob->GetBufferPointer(), pointShader_.VertexShaderBlob->GetBufferSize(), nullptr, &pointShader_.VertexShader), 
        "Im3d point vertex shader creation failed!");

    pointShader_.GeometryShaderBlob = LoadCompileShader("gs_4_0", "assets/shaders/im3d.hlsl", "GEOMETRY_SHADER\0POINTS\0");
    if (!pointShader_.GeometryShaderBlob) return false;
    DxCheck(d3d11Device_->CreateGeometryShader(pointShader_.GeometryShaderBlob->GetBufferPointer(), pointShader_.GeometryShaderBlob->GetBufferSize(), nullptr, &pointShader_.GeometryShader),
        "Im3d point geometry shader creation failed!");

    pointShader_.PixelShaderBlob = LoadCompileShader("ps_4_0", "assets/shaders/im3d.hlsl", "PIXEL_SHADER\0POINTS\0");
    if (!pointShader_.PixelShaderBlob) return false;
    DxCheck(d3d11Device_->CreatePixelShader(pointShader_.PixelShaderBlob->GetBufferPointer(), pointShader_.PixelShaderBlob->GetBufferSize(), nullptr, &pointShader_.PixelShader),
        "Im3d point pixel shader creation failed!");


    //Create line shaders
    lineShader_.VertexShaderBlob = LoadCompileShader("vs_4_0", "assets/shaders/im3d.hlsl", "VERTEX_SHADER\0LINES\0");
    if (!lineShader_.VertexShaderBlob) return false;
    DxCheck(d3d11Device_->CreateVertexShader((DWORD*)lineShader_.VertexShaderBlob->GetBufferPointer(), lineShader_.VertexShaderBlob->GetBufferSize(), nullptr, &lineShader_.VertexShader),
        "Im3d line pixel shader creation failed!");

    lineShader_.GeometryShaderBlob = LoadCompileShader("gs_4_0", "assets/shaders/im3d.hlsl", "GEOMETRY_SHADER\0LINES\0");
    if (!lineShader_.GeometryShaderBlob) return false;
    DxCheck(d3d11Device_->CreateGeometryShader((DWORD*)lineShader_.GeometryShaderBlob->GetBufferPointer(), lineShader_.GeometryShaderBlob->GetBufferSize(), nullptr, &lineShader_.GeometryShader),
        "Im3d line geometry shader creation failed!");

    lineShader_.PixelShaderBlob = LoadCompileShader("ps_4_0", "assets/shaders/im3d.hlsl", "PIXEL_SHADER\0LINES\0");
    if (!lineShader_.PixelShaderBlob) return false;
    DxCheck(d3d11Device_->CreatePixelShader((DWORD*)lineShader_.PixelShaderBlob->GetBufferPointer(), lineShader_.PixelShaderBlob->GetBufferSize(), nullptr, &lineShader_.PixelShader),
        "Im3d line pixel shader creation failed!");


    //Create triangle shaders
    triangleShader_.VertexShaderBlob = LoadCompileShader("vs_4_0", "assets/shaders/im3d.hlsl", "VERTEX_SHADER\0TRIANGLES\0");
    if (!triangleShader_.VertexShaderBlob) return false;
    DxCheck(d3d11Device_->CreateVertexShader((DWORD*)triangleShader_.VertexShaderBlob->GetBufferPointer(), triangleShader_.VertexShaderBlob->GetBufferSize(), nullptr, &triangleShader_.VertexShader),
        "Im3d triangle vertex shader creation failed!");

    triangleShader_.PixelShaderBlob = LoadCompileShader("ps_4_0", "assets/shaders/im3d.hlsl", "PIXEL_SHADER\0TRIANGLES\0");
    if (!triangleShader_.PixelShaderBlob) return false;
    DxCheck(d3d11Device_->CreatePixelShader((DWORD*)triangleShader_.PixelShaderBlob->GetBufferPointer(), triangleShader_.PixelShaderBlob->GetBufferSize(), nullptr, &triangleShader_.PixelShader),
        "Im3d triangle pixel shader creation failed!");


    D3D11_INPUT_ELEMENT_DESC inputDesc[] =
    {
        { "POSITION_SIZE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,   0, (UINT)offsetof(Im3d::VertexData, m_positionSize), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",         0, DXGI_FORMAT_R8G8B8A8_UNORM,       0, (UINT)offsetof(Im3d::VertexData, m_color),        D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    DxCheck(d3d11Device_->CreateInputLayout(inputDesc, 2, pointShader_.VertexShaderBlob->GetBufferPointer(), pointShader_.VertexShaderBlob->GetBufferSize(), &vertexInputLayout_),
        "Im3d vertex input layout creation failed!");
    
    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE; // culling invalid for points/lines (they are view-aligned), valid but optional for triangles
    DxCheck(d3d11Device_->CreateRasterizerState(&rasterizerDesc, &rasterizerState_), "Im3d rasterizer state creation failed!");
    
    D3D11_DEPTH_STENCIL_DESC stencilDesc = {};
    DxCheck(d3d11Device_->CreateDepthStencilState(&stencilDesc, &depthStencilState_), "Im3d depth stencil state creation failed!");

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = true;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    DxCheck(d3d11Device_->CreateBlendState(&blendDesc, &blendState_), "Im3d blend state creation failed!");
    
    D3D11_BUFFER_DESC constantBufferDesc = {};
    constantBufferDesc.ByteWidth = sizeof(DirectX::XMMATRIX) + sizeof(DirectX::XMFLOAT4);
    constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    DxCheck(d3d11Device_->CreateBuffer(&constantBufferDesc, nullptr, &constantBuffer_), "Im3d constant buffer creation failed");

    return true;
}

void Im3dRenderer::HandleResize(int windowWidth, int windowHeight)
{
    windowWidth_ = windowWidth;
    windowHeight_ = windowHeight;
}

void Im3dRenderer::Shutdown()
{
    pointShader_.Release();
    lineShader_.Release();
    triangleShader_.Release();

    ReleaseCOM(vertexInputLayout_);
    ReleaseCOM(rasterizerState_);
    ReleaseCOM(blendState_);
    ReleaseCOM(depthStencilState_);
    ReleaseCOM(constantBuffer_);
    ReleaseCOM(vertexBuffer_);
}

void Im3dRenderer::NewFrame(f32 deltaTime)
{
    Im3d::AppData& ad = Im3d::GetAppData();

    ad.m_deltaTime = deltaTime;
    ad.m_viewportSize = Im3d::Vec2((f32)windowWidth_, (f32)windowHeight_);
    ad.m_viewOrigin = *(Im3d::Vec3*)(&camera_->camPosition);
    auto normalizedViewDir = DirectX::XMVector4Normalize(camera_->camTarget);
    ad.m_viewDirection = *(Im3d::Vec3*)(&normalizedViewDir);
    ad.m_worldUp = Im3d::Vec3(0.0f, 1.0f, 0.0f);// used internally for generating orthonormal bases
    ad.m_projOrtho = false; //Whether or not camera is using orthographic projection

    Im3d::Mat4 camProj = *(Im3d::Mat4*)(&camera_->camProjection); //Todo: Make conversion function for this. Fill out conversion operators in ext/im3d_config.h
    Im3d::Mat4 camWorld = Inverse(*(Im3d::Mat4*)(&camera_->camView));

    //m_projScaleY controls how gizmos are scaled in world space to maintain a constant screen height
    ad.m_projScaleY = tanf(camera_->GetFovRadians() * 0.5f) * 2.0f;

    //World space cursor ray from mouse position; for VR this might be the position/orientation of the HMD or a tracked controller.
    Im3d::Vec2 cursorPos = GetWindowRelativeCursor();
    cursorPos = (cursorPos / ad.m_viewportSize) * 2.0f - 1.0f;
    cursorPos.y = -cursorPos.y; //window origin is top-left, ndc is bottom-left
    Im3d::Vec3 rayOrigin, rayDirection;

    rayOrigin = ad.m_viewOrigin;
    rayDirection.x = cursorPos.x / camProj(0, 0);
    rayDirection.y = cursorPos.y / camProj(1, 1);
    rayDirection.z = 1.0f;
    rayDirection = camWorld * Im3d::Vec4(Normalize(rayDirection), 0.0f);

    ad.m_cursorRayOrigin = rayOrigin;
    ad.m_cursorRayDirection = rayDirection;

    //Set cull frustum planes. This is only required if IM3D_CULL_GIZMOS or IM3D_CULL_PRIMTIIVES is enable via
    //im3d_config.h, or if any of the IsVisible() functions are called.
    //Todo: Remove this dumb cast
    auto viewProj = camera_->GetViewProjMatrix();
    ad.setCullFrustum(*(Im3d::Mat4*)(&viewProj), true);

    //Fill the key state array; using GetAsyncKeyState here but this could equally well be done via the window proc.
    //All key states have an equivalent (and more descriptive) 'Action_' enum.
    ad.m_keyDown[Im3d::Mouse_Left/*Im3d::Action_Select*/] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

    //The following key states control which gizmo to use for the generic Gizmo() function. Here using the left ctrl
    //key as an additional predicate.
    bool ctrlDown = (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0;
    ad.m_keyDown[Im3d::Key_L/*Action_GizmoLocal*/] = ctrlDown && (GetAsyncKeyState(0x4c) & 0x8000) != 0;
    ad.m_keyDown[Im3d::Key_T/*Action_GizmoTranslation*/] = ctrlDown && (GetAsyncKeyState(0x54) & 0x8000) != 0;
    ad.m_keyDown[Im3d::Key_R/*Action_GizmoRotation*/] = ctrlDown && (GetAsyncKeyState(0x52) & 0x8000) != 0;
    ad.m_keyDown[Im3d::Key_S/*Action_GizmoScale*/] = ctrlDown && (GetAsyncKeyState(0x53) & 0x8000) != 0;

    //Enable gizmo snapping by setting the translation/rotation/scale increments to be > 0
    ad.m_snapTranslation = ctrlDown ? 0.1f : 0.0f;
    ad.m_snapRotation = ctrlDown ? (30.0f * (3.141593f / 180.0f)) : 0.0f;
    ad.m_snapScale = ctrlDown ? 0.5f : 0.0f;

    Im3d::NewFrame();
}

void Im3dRenderer::EndFrame()
{
    //Primitive rendering
    Im3d::EndFrame();
    Im3d::AppData& ad = Im3d::GetAppData();

    D3D11_VIEWPORT viewport =
    {
        0.0f, 0.0f, //TopLeftX, TopLeftY
        (f32)windowWidth_, (f32)windowHeight_,
        0.0f, 1.0f //MinDepth, MaxDepth
    };
    d3d11Context_->RSSetViewports(1, &viewport);
    d3d11Context_->OMSetBlendState(blendState_, nullptr, 0xffffffff);
    d3d11Context_->OMSetDepthStencilState(depthStencilState_, 0);
    d3d11Context_->RSSetState(rasterizerState_);

    for (u32 i = 0, n = Im3d::GetDrawListCount(); i < n; ++i)
    {
        auto& drawList = Im3d::GetDrawLists()[i];

        if (drawList.m_layerId == Im3d::MakeId("NamedLayer"))
        {
            // The application may group primitives into layers, which can be used to change the draw state (e.g. enable depth testing, use a different shader)
        }

        //Upload view-proj matrix/viewport size
        struct Layout { Im3d::Mat4 m_viewProj; Im3d::Vec2 m_viewport; };
        Layout* layout = (Layout*)MapBuffer(d3d11Context_, constantBuffer_, D3D11_MAP_WRITE_DISCARD);
        auto viewProj = camera_->GetViewProjMatrix();
        layout->m_viewProj = *(Im3d::Mat4*)(&viewProj); //Dumb cast from DirectX::XMMatrix to Im3d::Mat4
        layout->m_viewport = ad.m_viewportSize;
        UnmapBuffer(d3d11Context_, constantBuffer_);

        //Upload vertex data
        static u32 s_vertexBufferSize = 0;
        if (!vertexBuffer_ || s_vertexBufferSize < drawList.m_vertexCount)
        {
            if (vertexBuffer_)
            {
                vertexBuffer_->Release();
                vertexBuffer_ = nullptr;
            }
            s_vertexBufferSize = drawList.m_vertexCount;

            //Recreate vertex buffer to new size reqs
            D3D11_BUFFER_DESC desc = {};
            desc.ByteWidth = s_vertexBufferSize * sizeof(Im3d::VertexData);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            DxCheck(d3d11Device_->CreateBuffer(&desc, nullptr, &vertexBuffer_));
            //vertexBuffer_ = CreateVertexBuffer(s_vertexBufferSize * sizeof(Im3d::VertexData), D3D11_USAGE_DYNAMIC);
        }
        memcpy(MapBuffer(d3d11Context_, vertexBuffer_, D3D11_MAP_WRITE_DISCARD), drawList.m_vertexData, drawList.m_vertexCount * sizeof(Im3d::VertexData));
        UnmapBuffer(d3d11Context_, vertexBuffer_);

        //Select shader/primitive topo
        switch (drawList.m_primType)
        {
        case Im3d::DrawPrimitive_Points:
            d3d11Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
            d3d11Context_->VSSetShader(pointShader_.VertexShader, nullptr, 0);
            d3d11Context_->GSSetShader(pointShader_.GeometryShader, nullptr, 0);
            d3d11Context_->GSSetConstantBuffers(0, 1, &constantBuffer_);
            d3d11Context_->PSSetShader(pointShader_.PixelShader, nullptr, 0);
            break;
        case Im3d::DrawPrimitive_Lines:
            d3d11Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
            d3d11Context_->VSSetShader(lineShader_.VertexShader, nullptr, 0);
            d3d11Context_->GSSetShader(lineShader_.GeometryShader, nullptr, 0);
            d3d11Context_->GSSetConstantBuffers(0, 1, &constantBuffer_);
            d3d11Context_->PSSetShader(lineShader_.PixelShader, nullptr, 0);
            break;
        case Im3d::DrawPrimitive_Triangles:
            d3d11Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            d3d11Context_->VSSetShader(triangleShader_.VertexShader, nullptr, 0);
            d3d11Context_->PSSetShader(triangleShader_.PixelShader, nullptr, 0);
            break;
        default:
            IM3D_ASSERT(false);
            return;
        };

        u32 stride = sizeof(Im3d::VertexData);
        u32 offset = 0;
        d3d11Context_->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);
        d3d11Context_->IASetInputLayout(vertexInputLayout_);
        d3d11Context_->VSSetConstantBuffers(0, 1, &constantBuffer_);
        d3d11Context_->Draw(drawList.m_vertexCount, 0);

        d3d11Context_->VSSetShader(nullptr, nullptr, 0);
        d3d11Context_->GSSetShader(nullptr, nullptr, 0);
        d3d11Context_->PSSetShader(nullptr, nullptr, 0);
    }

    //Text rendering.
    //This is common to all examples since we're using ImGui to draw the text lists, see im3d_example.cpp.
    DrawTextDrawListsImGui(Im3d::GetTextDrawLists(), Im3d::GetTextDrawListCount());
}

Im3d::Vec2 Im3dRenderer::GetWindowRelativeCursor() const
{
    POINT p = {};
    GetCursorPos(&p);
    ScreenToClient(hwnd_, &p);
    return Im3d::Vec2((f32)p.x, (f32)p.y);
}

void Im3dRenderer::DrawTextDrawListsImGui(const Im3d::TextDrawList _textDrawLists[], u32 _count)
{
    using namespace Im3d;
    // Using ImGui here as a simple means of rendering text draw lists, however as with primitives the application is free to draw text in any conceivable  manner.
    // Invisible ImGui window which covers the screen.
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32_BLACK_TRANS);
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2((f32)windowWidth_, (f32)windowHeight_));
    ImGui::Begin("Invisible", nullptr, 0
        | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoInputs
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoBringToFrontOnFocus
    );

    ImDrawList* imDrawList = ImGui::GetWindowDrawList();
    //Todo: Should use conversion operators here
    auto viewProjDx11 = camera_->GetViewProjMatrix();
    const Mat4 viewProj = *(Im3d::Mat4*)(&viewProjDx11);//Dumb cast from DirectX::XMMatrix to Im3d::Mat4
    for (U32 i = 0; i < _count; ++i)
    {
        const TextDrawList& textDrawList = Im3d::GetTextDrawLists()[i];

        if (textDrawList.m_layerId == Im3d::MakeId("NamedLayer"))
        {
            // The application may group primitives into layers, which can be used to change the draw state (e.g. enable depth testing, use a different shader)
        }

        for (U32 j = 0; j < textDrawList.m_textDataCount; ++j)
        {
            const Im3d::TextData& textData = textDrawList.m_textData[j];
            if (textData.m_positionSize.w == 0.0f || textData.m_color.getA() == 0.0f)
            {
                continue;
            }

            // Project world -> screen space.
            Vec4 clip = viewProj * Vec4(textData.m_positionSize.x, textData.m_positionSize.y, textData.m_positionSize.z, 1.0f);
            Vec2 screen = Vec2(clip.x / clip.w, clip.y / clip.w);

            // Cull text which falls offscreen. Note that this doesn't take into account text size but works well enough in practice.
            if (clip.w < 0.0f || screen.x >= 1.0f || screen.y >= 1.0f)
            {
                continue;
            }

            // Pixel coordinates for the ImGuiWindow ImGui.
            screen = screen * Vec2(0.5f) + Vec2(0.5f);
            screen.y = 1.0f - screen.y; // screen space origin is reversed by the projection.
            //Todo: Should use conversion operators here
            auto windowSize = ImGui::GetWindowSize();
            screen = screen * Vec2(windowSize.x, windowSize.y);

            // All text data is stored in a single buffer; each textData instance has an offset into this buffer.
            const char* text = textDrawList.m_textBuffer + textData.m_textBufferOffset;

            // Calculate the final text size in pixels to apply alignment flags correctly.
            ImGui::SetWindowFontScale(textData.m_positionSize.w); // NB no CalcTextSize API which takes a font/size directly...
            //Todo: Should use conversion operators here
            auto textSizeImVec2 = ImGui::CalcTextSize(text, text + textData.m_textLength);
            Vec2 textSize = Vec2(textSizeImVec2.x, textSizeImVec2.y);
            ImGui::SetWindowFontScale(1.0f);

            // Generate a pixel offset based on text flags.
            Vec2 textOffset = Vec2(-textSize.x * 0.5f, -textSize.y * 0.5f); // default to center
            if ((textData.m_flags & Im3d::TextFlags_AlignLeft) != 0)
            {
                textOffset.x = -textSize.x;
            }
            else if ((textData.m_flags & Im3d::TextFlags_AlignRight) != 0)
            {
                textOffset.x = 0.0f;
            }

            if ((textData.m_flags & Im3d::TextFlags_AlignTop) != 0)
            {
                textOffset.y = -textSize.y;
            }
            else if ((textData.m_flags & Im3d::TextFlags_AlignBottom) != 0)
            {
                textOffset.y = 0.0f;
            }

            // Add text to the window draw list.
            screen = screen + textOffset;
            imDrawList->AddText(nullptr, textData.m_positionSize.w * ImGui::GetFontSize(), ImVec2(screen.x, screen.y), textData.m_color.getABGR(), text, text + textData.m_textLength);
        }
    }

    ImGui::End();
    ImGui::PopStyleColor(1);
}
