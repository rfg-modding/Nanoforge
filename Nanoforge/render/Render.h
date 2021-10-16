#pragma once
#include "common/Typedefs.h"
#include "render/resources/Material.h"
#include <unordered_map>

class Config;

//Global render state and resources
class Render
{
public:
    static void Init(ComPtr<ID3D11Device> d3d11Device, ComPtr<ID3D11DeviceContext> d3d11Context, Config* config);
    static Material* GetMaterial(std::string_view name);
    static void ReloadEditedShaders(); //Reload material shaders if needed

private:
    static std::unordered_map<string, Material> materials_;
};