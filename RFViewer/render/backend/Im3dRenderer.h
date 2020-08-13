#pragma once
#include "common/Typedefs.h"
#include "Im3dShader.h"
#include <im3d.h>
#include <ext/WindowsWrapper.h>

struct IDXGIFactory;
struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11Buffer;
struct ID3D10Blob;
struct ID3D11ShaderResourceView;
struct ID3D11SamplerState;
struct ID3D11GeometryShader;
struct ID3D11RasterizerState;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
class Camera;

//Todo: Replace this with a primitive renderer built around the needs of this application

//Initializes im3d manages any data it needs access to
class Im3dRenderer
{
public:
    bool Init(ID3D11Device* d3d11Device, ID3D11DeviceContext* d3d11Context, HWND hwnd, Camera* camera);
    void HandleResize(int windowWidth, int windowHeight);
    void Shutdown();
    void NewFrame(f32 deltaTime);
    void EndFrame();

private:
    //Another function pulled from the im3d dx11 example
    Im3d::Vec2 GetWindowRelativeCursor() const;
    void DrawTextDrawListsImGui(const Im3d::TextDrawList _textDrawLists[], u32 _count);

    Im3dShader pointShader_;
    Im3dShader lineShader_;
    Im3dShader triangleShader_;
    ID3D11InputLayout* vertexInputLayout_ = nullptr;
    ID3D11RasterizerState* rasterizerState_ = nullptr;
    ID3D11BlendState* blendState_ = nullptr;
    ID3D11DepthStencilState* depthStencilState_ = nullptr;
    ID3D11Buffer* constantBuffer_ = nullptr;
    ID3D11Buffer* vertexBuffer_ = nullptr;

    int windowWidth_ = 800;
    int windowHeight_ = 800;

    ID3D11Device* d3d11Device_ = nullptr;
    ID3D11DeviceContext* d3d11Context_ = nullptr;
    Camera* camera_ = nullptr;
    HWND hwnd_ = nullptr;
};