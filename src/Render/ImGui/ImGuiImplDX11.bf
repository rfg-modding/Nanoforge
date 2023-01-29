using Direct3D;
using Common;
using System;
using ImGui;
using Win32;

namespace Nanoforge.Render.ImGui
{
    //Based on imgui_impl_dx11.cpp example implementation in main imgui repo
    public class ImGuiImplDX11
    {
        public ID3D11DeviceContext* Context = null;
        public ID3D11Device* Device = null;
        public IDXGIFactory* Factory = null;
        public ID3D11Buffer* VertexBuffer = null;
        public ID3D11Buffer* IndexBuffer = null;
        public ID3D11VertexShader* VertexShader = null;
        public ID3D11InputLayout* InputLayout = null;
        public ID3D11Buffer* VertexConstantBuffer = null;
        public ID3D11PixelShader* PixelShader = null;
        public ID3D11SamplerState* FontSampler = null;
        public ID3D11ShaderResourceView* FontTextureView = null;
        public ID3D11RasterizerState* RasterizerState = null;
        public ID3D11BlendState* BlendState = null;
        public ID3D11DepthStencilState* DepthStencilState = null;
        int VertexBufferSize = 5000;
        int IndexBufferSize = 10000;

        struct VertexConstantBuffer
        {
            public f32[4][4] MVP;
        }

        struct BACKUP_DX11_STATE
        {
            public u32 ScissorRectsCount;
            public u32 ViewportsCount;
            public RECT[D3D11.D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] ScissorRects;
            public D3D11_VIEWPORT[D3D11.D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] Viewports;
            public ID3D11RasterizerState* RS;
            public ID3D11BlendState* BlendState;
            public f32[4] BlendFactor;
            public u32 SampleMask;
            public u32 StencilRef;
            public ID3D11DepthStencilState* DepthStencilState;
            public ID3D11ShaderResourceView* PSShaderResource;
            public ID3D11SamplerState* PSSampler;
            public ID3D11PixelShader* PS;
            public ID3D11VertexShader* VS;
            public ID3D11GeometryShader* GS;
            public u32 PSInstancesCount;
			public u32 VSInstancesCount;
			public u32 GSInstancesCount;
            public ID3D11ClassInstance*[256] PSInstances;
            public ID3D11ClassInstance*[256] VSInstances;
            public ID3D11ClassInstance*[256] GSInstances; //256 is max according to PSSetShader documentation
            public D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology;
            public ID3D11Buffer* IndexBuffer;
			public ID3D11Buffer* VertexBuffer;
			public ID3D11Buffer* VSConstantBuffer;
            public u32 IndexBufferOffset;
			public u32 VertexBufferStride;
			public u32 VertexBufferOffset;
            public DXGI_FORMAT IndexBufferFormat;
            public ID3D11InputLayout* InputLayout;
        };

        public bool Init(ID3D11DeviceContext* context, ID3D11Device* device, IDXGIFactory* factory)
        {
            ImGui.IO* io = ImGui.GetIO();
            io.BackendRendererName = "DX11";
            io.BackendFlags |= ImGui.BackendFlags.RendererHasVtxOffset;  //We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
            io.BackendFlags |= ImGui.BackendFlags.RendererHasViewports;  //We can create multi-viewports on the Renderer side (optional)
            Context = context;
            Device = device;
            Factory = factory;
            Device.AddRef();
            Context.AddRef();
            return true;
        }

        public void Shutdown()
        {
            ImGui.IO* io = ImGui.GetIO();
            InvalidateDeviceObjects();
            ReleaseCOM!(Factory);
            ReleaseCOM!(Device);
            ReleaseCOM!(Context);
            io.BackendRendererName = null;
            io.BackendRendererUserData = null;
        }

        public void NewFrame()
        {
            if (FontSampler == null)
                CreateDeviceObjects();
        }

        public void RenderDrawData(ImGui.DrawData* drawData)
        {
            if (drawData.DisplaySize.x <= 0.0f || drawData.DisplaySize.y <= 0.0f)
                return;

            //Create and grow vertex/index buffers if needed
            if (VertexBuffer == null || VertexBufferSize < drawData.TotalVtxCount)
            {
                ReleaseCOM!(VertexBuffer);
                VertexBufferSize = drawData.TotalVtxCount + 5000;

                D3D11_BUFFER_DESC desc = .();
                ZeroMemory(&desc);
                desc.Usage = .DYNAMIC;
                desc.ByteWidth = (u32)(VertexBufferSize * sizeof(ImGui.DrawVert));
                desc.BindFlags = (u32)D3D11_BIND_FLAG.VERTEX_BUFFER;
                desc.CPUAccessFlags = (u32)D3D11_CPU_ACCESS_FLAG.WRITE;
                desc.MiscFlags = 0;
                DxRequired!(Device.CreateBuffer(desc, null, &VertexBuffer), "Failed to create Dear ImGui vertex buffer");
            }
            if (IndexBuffer == null || IndexBufferSize < drawData.TotalIdxCount)
            {
                ReleaseCOM!(IndexBuffer);
                IndexBufferSize = drawData.TotalIdxCount + 10000;

                D3D11_BUFFER_DESC desc = .();
                ZeroMemory(&desc);
                desc.Usage = .DYNAMIC;
                desc.ByteWidth = (u32)(IndexBufferSize * sizeof(ImGui.DrawIdx));
                desc.BindFlags = (u32)D3D11_BIND_FLAG.INDEX_BUFFER;
                desc.CPUAccessFlags = (u32)D3D11_CPU_ACCESS_FLAG.WRITE;
                desc.MiscFlags = 0;
                DxRequired!(Device.CreateBuffer(desc, null, &IndexBuffer), "Failed to create Dear ImGui index buffer");
            }

            //Upload indices & vertices to GPU
            D3D11_MAPPED_SUBRESOURCE vertexResource = .();
            D3D11_MAPPED_SUBRESOURCE indexResource = .();
            DxRequired!(Context.Map(VertexBuffer, 0, .WRITE_DISCARD, 0, &vertexResource), "Failed to map ImGui vertex buffer");
            DxRequired!(Context.Map(IndexBuffer, 0, .WRITE_DISCARD, 0, &indexResource), "Failed to map ImGui index buffer");

            ImGui.DrawVert* verticesDest = (ImGui.DrawVert*)vertexResource.pData;
            ImGui.DrawIdx* indicesDest = (ImGui.DrawIdx*)indexResource.pData;
            for (int i in 0..<drawData.CmdListsCount)
            {
                ImGui.DrawList* commandList = drawData.CmdLists[i];
                Internal.MemCpy(verticesDest, commandList.VtxBuffer.Data, commandList.VtxBuffer.Size * sizeof(ImGui.DrawVert));
                Internal.MemCpy(indicesDest, commandList.IdxBuffer.Data, commandList.IdxBuffer.Size * sizeof(ImGui.DrawIdx));
                verticesDest += commandList.VtxBuffer.Size;
                indicesDest += commandList.IdxBuffer.Size;
            }
            Context.Unmap(VertexBuffer, 0);
            Context.Unmap(IndexBuffer, 0);

            //Setup orthographic projection matrix into our constant buffer
            //Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
            {
                D3D11_MAPPED_SUBRESOURCE mappedResource = .();
                DxRequired!(Context.Map(VertexConstantBuffer, 0, .WRITE_DISCARD, 0, &mappedResource), "Failed to map ImGui orthographic projection matrix");

                VertexConstantBuffer* constantBuffer = (VertexConstantBuffer*)mappedResource.pData;
                f32 L = drawData.DisplayPos.x;
                f32 R = drawData.DisplayPos.x + drawData.DisplaySize.x;
                f32 T = drawData.DisplayPos.y;
                f32 B = drawData.DisplayPos.y + drawData.DisplaySize.y;
                f32[4][4] mvp =
				.(
                    .(2.0f / (R - L),    0.0f,              0.0f,         0.0f),
                    .(0.0f,              2.0f / (T - B),    0.0f,         0.0f),
                    .(0.0f,              0.0f,              0.5f,         0.0f),
                    .((R + L) / (L - R), (T + B) / (B - T), 0.5f,         1.0f)
				);
                Internal.MemCpy(&constantBuffer.MVP, &mvp[0], 4 * 4 * sizeof(f32));
                Context.Unmap(VertexConstantBuffer, 0);
            }

            //Backup DX state that will be modified to restore it afterwards (unfortunately this is very ugly looking and verbose. Close your eyes!)
            BACKUP_DX11_STATE old = .();
            old.ScissorRectsCount = D3D11.D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            old.ViewportsCount = D3D11.D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            Context.RSGetScissorRects(out old.ScissorRectsCount, &old.ScissorRects[0]);
            Context.RSGetViewports(out old.ViewportsCount, &old.Viewports[0]);
            Context.RSGetState(&old.RS);
            Context.OMGetBlendState(&old.BlendState, &old.BlendFactor, &old.SampleMask);
            Context.OMGetDepthStencilState(&old.DepthStencilState, &old.StencilRef);
            Context.PSGetShaderResources(0, 1, &old.PSShaderResource);
            Context.PSGetSamplers(0, 1, &old.PSSampler);
            old.PSInstancesCount = 256;
            old.VSInstancesCount = 256;
            old.GSInstancesCount = 256;
            Context.PSGetShader(&old.PS, &old.PSInstances, &old.PSInstancesCount);
            Context.VSGetShader(&old.VS, &old.VSInstances, &old.VSInstancesCount);
            Context.VSGetConstantBuffers(0, 1, &old.VSConstantBuffer);
            Context.GSGetShader(&old.GS, &old.GSInstances, &old.GSInstancesCount);

            Context.IAGetPrimitiveTopology(out old.PrimitiveTopology);
            Context.IAGetIndexBuffer(&old.IndexBuffer, &old.IndexBufferFormat, &old.IndexBufferOffset);
            Context.IAGetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset);
            Context.IAGetInputLayout(&old.InputLayout);

            SetupRenderState(drawData);

            //Render command lists
            //(Because we merged all buffers into a single one, we maintain our own offset into them)
            int globalIdxOffset = 0;
            int globalVtxOffset = 0;
            ImGui.Vec2 clipOffset = drawData.DisplayPos;
            for (int i in 0..<drawData.CmdListsCount)
            {
                ImGui.DrawList* commandList = drawData.CmdLists[i];
                for (int j in 0..<commandList.CmdBuffer.Size)
                {
                    ImGui.DrawCmd* command = &commandList.CmdBuffer.Data[j];
                    if (command.UserCallback != null)
                    {
                        //User callback, registered via ImDrawList::AddCallback()
                        //(ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                        ImGui.DrawCallback resetCallback = null; //ImGui.DrawCallback_ResetRenderState;
                        if (ImGui.DrawCallback_ResetRenderState != (.)(void*)-1 && ImGui.DrawCallback_ResetRenderState != null) //For some reason it's defaulted to 0xFFFFFFFF instead of null......
                            resetCallback = *ImGui.DrawCallback_ResetRenderState;

                        if (command.UserCallback == resetCallback)
                            SetupRenderState(drawData);
                        else
                            command.UserCallback(commandList, command);
                    }
                    else
                    {
                        ImGui.Vec2 clipMin = .(command.ClipRect.x - clipOffset.x, command.ClipRect.y - clipOffset.y);
                        ImGui.Vec2 clipMax = .(command.ClipRect.z - clipOffset.x, command.ClipRect.w - clipOffset.y);
                        if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                            continue;

                        //Apply scissor/clipping rectangle
                        RECT scissorRect;
                        scissorRect.left = (i32)clipMin.x;
                        scissorRect.top = (i32)clipMin.y;
                        scissorRect.right = (i32)clipMax.x;
                        scissorRect.bottom = (i32)clipMax.y;
                        Context.RSSetScissorRects(1, &scissorRect);

                        //Bind texture & draw
                        ID3D11ShaderResourceView* textureSource = (ID3D11ShaderResourceView*)command.GetTexID();
                        Context.PSSetShaderResources(0, 1, &textureSource);
                        Context.DrawIndexed(command.ElemCount, (u32)(command.IdxOffset + globalIdxOffset), (i32)(command.VtxOffset + globalVtxOffset));
                    }
                }

                globalIdxOffset += commandList.IdxBuffer.Size;
                globalVtxOffset += commandList.VtxBuffer.Size;
            }

            //Restore modified DX state
            Context.RSSetScissorRects(old.ScissorRectsCount, &old.ScissorRects[0]);
            Context.RSSetViewports(old.ViewportsCount, &old.Viewports[0]);
            Context.RSSetState(old.RS);
            ReleaseCOM!(old.RS);
            Context.OMSetBlendState(old.BlendState, &old.BlendFactor[0], old.SampleMask);
            ReleaseCOM!(old.BlendState);
            Context.OMSetDepthStencilState(old.DepthStencilState, old.StencilRef);
            ReleaseCOM!(old.DepthStencilState);

            Context.PSSetShaderResources(0, 1, &old.PSShaderResource);
            ReleaseCOM!(old.PSShaderResource);
            Context.PSSetSamplers(0, 1, &old.PSSampler);
            ReleaseCOM!(old.PSSampler);
            Context.PSSetShader(old.PS, &old.PSInstances[0], old.PSInstancesCount);
            ReleaseCOM!(old.PS);
            for (int i in 0..<256)
                ReleaseCOM!(old.PSInstances[i]);

            Context.VSSetShader(old.VS, &old.VSInstances[0], old.VSInstancesCount);
            ReleaseCOM!(old.VS);
            Context.VSSetConstantBuffers(0, 1, &old.VSConstantBuffer);
            ReleaseCOM!(old.VSConstantBuffer);
            Context.GSSetShader(old.GS, &old.GSInstances[0], old.GSInstancesCount);
            ReleaseCOM!(old.GS);
            for (int i in 0..<256)
                ReleaseCOM!(old.VSInstances[i]);

            Context.IASetPrimitiveTopology(old.PrimitiveTopology);
            Context.IASetIndexBuffer(old.IndexBuffer, old.IndexBufferFormat, old.IndexBufferOffset);
            ReleaseCOM!(old.IndexBuffer);
            Context.IASetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset);
            ReleaseCOM!(old.VertexBuffer);
            Context.IASetInputLayout(old.InputLayout);
            ReleaseCOM!(old.InputLayout);
        }

        private bool CreateDeviceObjects()
        {
            if (Device == null)
                return false;
            if (FontSampler != null)
                InvalidateDeviceObjects();

            //By using D3DCompile() from <d3dcompiler.h> / d3dcompiler.lib, we introduce a dependency to a given version of d3dcompiler_XX.dll (see D3DCOMPILER_DLL_A)
            //If you would like to use this DX11 sample code but remove this dependency you can:
            //    1) compile once, save the compiled shader blobs into a file or source code and pass them to CreateVertexShader()/CreatePixelShader() [preferred solution]
            //    2) use code to detect any version of the DLL and grab a pointer to D3DCompile from the DLL.
            //See https://github.com/ocornut/imgui/pull/638 for sources and details.

            //Create the vertex shader
            {
                char8* vertexShaderSource =
    				@"""
    				cbuffer vertexBuffer : register(b0)
                    {
                        float4x4 ProjectionMatrix;
                    };
                    struct VS_INPUT
                    {
                        float2 pos : POSITION;
                        float4 col : COLOR0;
                        float2 uv  : TEXCOORD0;
                    };
                    struct PS_INPUT
                    {
                        float4 pos : SV_POSITION;
                        float4 col : COLOR0;
                        float2 uv  : TEXCOORD0;
                    };
    
                    PS_INPUT main(VS_INPUT input)
                    {
                        PS_INPUT output;
                        output.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.0f, 1.0f));
                        output.col = input.col;
                        output.uv  = input.uv;
    				    return output;
                    }
""";

                ID3DBlob* vertexShaderBlob;
                if (Direct3D.D3DCompile(vertexShaderSource, (u32)StringView(vertexShaderSource).Length, null, null, null, "main", "vs_4_0", 0, 0, out vertexShaderBlob, null) < 0)
                    return false;
                if (Device.CreateVertexShader(vertexShaderBlob.GetBufferPointer(), vertexShaderBlob.GetBufferSize(), null, &VertexShader) != Win32.S_OK)
                {
                    vertexShaderBlob.Release();
                    return false;
                }

                //Create input layout
                D3D11_INPUT_ELEMENT_DESC[?] layout =
                .(
                    .("POSITION", 0, .R32G32_FLOAT,   0, offsetof(ImGui.DrawVert, pos), .VERTEX_DATA, 0),
                    .("TEXCOORD", 0, .R32G32_FLOAT,   0, offsetof(ImGui.DrawVert, uv),  .VERTEX_DATA, 0),
                    .("COLOR",    0, .R8G8B8A8_UNORM, 0, offsetof(ImGui.DrawVert, col), .VERTEX_DATA, 0)
				);
                if (Device.CreateInputLayout(&layout[0], 3, vertexShaderBlob.GetBufferPointer(), vertexShaderBlob.GetBufferSize(), &InputLayout) != Win32.S_OK)
                {
                    vertexShaderBlob.Release();
                    return false;
                }
                vertexShaderBlob.Release();

                //Create constant buffer
                {
                    D3D11_BUFFER_DESC desc = .();
                    desc.ByteWidth = sizeof(VertexConstantBuffer);
                    desc.Usage = .DYNAMIC;
                    desc.BindFlags = (u32)D3D11_BIND_FLAG.CONSTANT_BUFFER;
                    desc.CPUAccessFlags = (u32)D3D11_CPU_ACCESS_FLAG.WRITE;
                    desc.MiscFlags = 0;
                    Device.CreateBuffer(desc, null, &VertexConstantBuffer);
                }
            }

            //Create the pixel shader
            {
                char8* pixelShaderSource =
                    @"""
                    struct PS_INPUT
                    {
                        float4 pos : SV_POSITION;
                        float4 col : COLOR0;
                        float4 uv : TEXCOORD0;
                    };

                    sampler sampler0;
                    Texture2D texture0;

                    float4 main(PS_INPUT input) : SV_TARGET
                    {
                        float4 outCol = input.col * texture0.Sample(sampler0, input.uv);
                        return outCol;
                    }
                    """;

                ID3DBlob* pixelShaderBlob;
                if (Direct3D.D3DCompile(pixelShaderSource, (u32)StringView(pixelShaderSource).Length, null, null, null, "main", "ps_4_0", 0, 0, out pixelShaderBlob, null) < 0)
                    return false;
                if (Device.CreatePixelShader(pixelShaderBlob.GetBufferPointer(), pixelShaderBlob.GetBufferSize(), null, &PixelShader) != Win32.S_OK)
                {
                    pixelShaderBlob.Release();
                    return false;
                }
            }

            //Create the blending setup
            {
                D3D11_BLEND_DESC desc = .();
                ZeroMemory(&desc);
                desc.AlphaToCoverageEnable = 0;
                desc.RenderTarget[0].BlendEnable = 1;
                desc.RenderTarget[0].SrcBlend = .SRC_ALPHA;
                desc.RenderTarget[0].DestBlend = .INV_SRC_ALPHA;
                desc.RenderTarget[0].BlendOp = .ADD;
                desc.RenderTarget[0].SrcBlendAlpha = .ONE;
                desc.RenderTarget[0].DestBlendAlpha = .INV_SRC_ALPHA;
                desc.RenderTarget[0].BlendOpAlpha = .ADD;
                desc.RenderTarget[0].RenderTargetWriteMask = (int)D3D11_COLOR_WRITE_ENABLE.ALL;
                Device.CreateBlendState(desc, &BlendState);
            }

            //Create the rasterizer state
            {
                D3D11_RASTERIZER_DESC desc = .();
                ZeroMemory(&desc);
                desc.FillMode = .SOLID;
                desc.CullMode = .NONE;
                desc.ScissorEnable = 1;
                desc.DepthClipEnable = 1;
                Device.CreateRasterizerState(desc, &RasterizerState);
            }

            //Create the depth stencil state
            {
                D3D11_DEPTH_STENCIL_DESC desc = .();
                ZeroMemory(&desc);
                desc.DepthEnable = 1;
                desc.DepthWriteMask = .ALL;
                desc.DepthFunc = .ALWAYS;
                desc.StencilEnable = 0;
                desc.FrontFace.StencilFailOp = .KEEP;
                desc.FrontFace.StencilPassOp = .KEEP;
                desc.FrontFace.StencilDepthFailOp = .KEEP;
                desc.FrontFace.StencilFunc = .ALWAYS;
                desc.BackFace = desc.FrontFace;
                Device.CreateDepthStencilState(desc, &DepthStencilState);
            }

            CreateFontsTexture();
            return true;
        }

        private void CreateFontsTexture()
        {
            ImGui.IO* io = ImGui.GetIO();
            u8* pixels = null;
            i32 width = 0;
            i32 height = 0;
            io.Fonts.GetTexDataAsRGBA32(out pixels, out width, out height);

            //Upload texture to GPU
            {
                D3D11_TEXTURE2D_DESC desc = .();
                ZeroMemory(&desc);
                desc.Width = (u32)width;
                desc.Height = (u32)height;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = .R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = .DEFAULT;
                desc.BindFlags = .SHADER_RESOURCE;
                desc.CPUAccessFlags = 0;

                ID3D11Texture2D* texture = null;
                D3D11_SUBRESOURCE_DATA subResource = .();
                subResource.pSysMem = pixels;
                subResource.SysMemPitch = desc.Width * 4;
                subResource.SysMemSlicePitch = 0;
                DxRequired!(Device.CreateTexture2D(desc, &subResource, &texture), "Failed to create Dear ImGui font texture");

                //Create texture view
                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = .();
                ZeroMemory(&srvDesc);
                srvDesc.Format = .R8G8B8A8_UNORM;
                srvDesc.ViewDimension = .D3D_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = desc.MipLevels;
                srvDesc.Texture2D.MostDetailedMip = 0;
                DxRequired!(Device.CreateShaderResourceView(texture, &srvDesc, &FontTextureView), "Failed to create Dear ImGui font texture SRV");
                texture.Release();
            }

            //Store texture ID
            io.Fonts.SetTexID((ImGui.TextureID)FontTextureView);

            //Create texture sample
            {
                D3D11_SAMPLER_DESC desc = .();
                ZeroMemory(&desc);
                desc.Filter = .MIN_MAG_MIP_LINEAR;
                desc.AddressU = .WRAP;
                desc.AddressV = .WRAP;
                desc.AddressW = .WRAP;
                desc.MipLODBias = 0.0f;
                desc.ComparisonFunc = .ALWAYS;
                desc.MinLOD = 0.0f;
                desc.MaxLOD = 0.0f;
                Device.CreateSamplerState(desc, &FontSampler);
            }
        }

        private void InvalidateDeviceObjects()
        {
            if (Device == null)
                return;

            ReleaseCOM!(FontSampler);
            if (FontTextureView == null)
            {
                ReleaseCOM!(FontTextureView);
                ImGui.GetIO().Fonts.SetTexID(null);
            }
            ReleaseCOM!(IndexBuffer);
            ReleaseCOM!(VertexBuffer);
            ReleaseCOM!(BlendState);
            ReleaseCOM!(DepthStencilState);
            ReleaseCOM!(RasterizerState);
            ReleaseCOM!(PixelShader);
            ReleaseCOM!(VertexConstantBuffer);
            ReleaseCOM!(InputLayout);
            ReleaseCOM!(VertexShader);
        }

        private void SetupRenderState(ImGui.DrawData* drawData)
        {
            //Setup viewport
            D3D11_VIEWPORT viewport;
            ZeroMemory(&viewport);
            viewport.Width = drawData.DisplaySize.x;
            viewport.Height = drawData.DisplaySize.y;
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
            Context.RSSetViewports(1, &viewport);

            //Setup shader and vertex buffers
            u32 stride = sizeof(ImGui.DrawVert);
            u32 offset = 0;
            Context.IASetInputLayout(InputLayout);
            Context.IASetVertexBuffers(0, 1, &VertexBuffer, &stride, &offset);
            Context.IASetIndexBuffer(IndexBuffer, sizeof(ImGui.DrawIdx) == 2 ? DXGI_FORMAT.R16_UINT : DXGI_FORMAT.R32_UINT, 0);
            Context.IASetPrimitiveTopology(.D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            Context.VSSetShader(VertexShader, null, 0);
            Context.VSSetConstantBuffers(0, 1, &VertexConstantBuffer);
            Context.PSSetShader(PixelShader, null, 0);
            Context.PSSetSamplers(0, 1, &FontSampler);
            Context.GSSetShader(null, null, 0);
            Context.HSSetShader(null, null, 0);
            Context.DSSetShader(null, null, 0);
            Context.CSSetShader(null, null, 0);

            //Setup blend state
            f32[4] blendFactor = .(0.0f, 0.0f, 0.0f, 0.0f);
            Context.OMSetBlendState(BlendState, &blendFactor[0], 0xFFFFFFFF);
            Context.OMSetDepthStencilState(DepthStencilState, 0);
            Context.RSSetState(RasterizerState);
        }
    }
}