#include "Im3dShader.h"
#include "render/util/DX11Helpers.h"
#include <d3d11.h>

void Im3dShader::Release()
{
    ReleaseCOM(VertexShaderBlob);
    ReleaseCOM(VertexShader);
    ReleaseCOM(GeometryShaderBlob);
    ReleaseCOM(GeometryShader);
    ReleaseCOM(PixelShaderBlob);
    ReleaseCOM(PixelShader);
}