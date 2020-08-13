#pragma once

struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D10Blob;
struct ID3D11GeometryShader;

//Todo: Consider also using this for the rest of the renderer code
//Shader struct used by Im3d
struct Im3dShader
{
    ID3D10Blob* VertexShaderBlob = nullptr;
    ID3D11VertexShader* VertexShader = nullptr;
    ID3D10Blob* GeometryShaderBlob = nullptr;
    ID3D11GeometryShader* GeometryShader = nullptr;
    ID3D10Blob* PixelShaderBlob = nullptr;
    ID3D11PixelShader* PixelShader = nullptr;

    void Release();
};