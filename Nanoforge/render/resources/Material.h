#pragma once
#include "common/Typedefs.h"
#include "render/resources/Shader.h"
#include <d3d11.h>

using Microsoft::WRL::ComPtr;
class Config;

//Wraps information for rendering a specific mesh format, such as vertex layout and shaders.
class Material
{
public:
	Material() { }
	Material(ComPtr<ID3D11Device> d3d11Device, const string& shaderName, const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout);
	void Init(ComPtr<ID3D11Device> d3d11Device, const string& shaderName, const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout);
	void Use(ComPtr<ID3D11DeviceContext> d3d11Context);
	void TryShaderReload();
	bool Ready() { return initialized_; }

private:
	ComPtr<ID3D11InputLayout> vertexLayout_;
	Shader shader_;
	bool initialized_ = false;
};