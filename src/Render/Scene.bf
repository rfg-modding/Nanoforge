using Nanoforge.Render.Resources;
using System.Collections;
using Nanoforge.Misc;
using Common.Math;
using Direct3D;
using Common;
using System;
using Win32;
using Common.Misc;
using System.Threading;
using Nanoforge.Rfg;

namespace Nanoforge.Render
{
    //Standalone group of primitives and meshes to render. Each editor tab with a 3D viewport such as the map editor or mesh viewers have their own scene.
	public class Scene
	{
        public bool Active = true; //Gets redrawn each frame if active
        public Camera3D Camera = new .() ~delete _;
        public Vec4 ClearColor = .(0.0f, 0.0f, 0.0f, 1.0f);
        public PerFrameConstants PerFrameConstants;
        public f32 TotalTime { get; private set; } = 0.0f;
        public u32 ViewWidth { get; private set; } = 512;
        public u32 ViewHeight { get; private set; } = 512;
        public ID3D11ShaderResourceView* View => _viewTexture.ShaderResourceView;
        public bool ErrorOccurred { get; private set; } = false;

        public append List<RenderObject> RenderObjects ~ClearAndDeleteItems!(_);
        public append Dictionary<Material, List<RenderObject>> ObjectsByMaterial ~ClearDictionaryAndDeleteValues!(_);
        private append Monitor _renderObjectCreationLock;

        private ID3D11Device* _device = null;
        private ID3D11DeviceContext* _context = null;
        private ID3D11RasterizerState* _meshRasterizerState ~ReleaseCOM(&_);
        private ID3D11RasterizerState* _primitiveRasterizerState ~ReleaseCOM(&_);
        private D3D11_VIEWPORT _viewport;
        private append Texture2D _viewTexture;
        private append Texture2D _depthBufferTexture;
        private append Buffer _perFrameConstantsBuffer;
        private append Buffer _perObjectConstantsBuffer;

        //For linelist primitives
        private Material _lineListMaterial = null;
        private append List<ColoredVertex> _lineVertices;
        private append Buffer _lineVertexBuffer;

        //For solid shaded triangle list primitives
        private Material _triangleListMaterial = null;
        private append List<ColoredVertex> _triangleListVertices;
        private append Buffer _triangleListVertexBuffer;

        //For triangle list primitives with basic lighting
        private Material _litTriangleListMaterial = null;
        private append List<ColoredVertex> _litTriangleListVertices;
        private append Buffer _litTriangleListVertexBuffer;

        private bool _primitiveBufferNeedsUpdate = true; //Set to true when the cpu side buffers have changed and need to be sent to the GPU
        public bool PrimitiveMaterialsSet => _lineListMaterial != null && _triangleListMaterial != null && _litTriangleListMaterial != null;

        //We currently only delete textures when we close a map, so we don't need fancy reference counting or anything like that just yet.
        public append List<Texture2D> Textures ~ClearAndDeleteItems(_);

        [CRepr, RequiredSize(16)]
        private struct ColoredVertex
        {
            public Vec3 Position;
            public u8 r, g, b, a;

            public this(Vec3 position, Vec4 color)
            {
                Position = position;
                r = (u8)(color.x * 255.0f);
                g = (u8)(color.y * 255.0f);
                b = (u8)(color.z * 255.0f);
                a = (u8)(color.w * 255.0f);
            }
        }

        [CRepr, RequiredSize(28)]
        private struct ColoredVertexLit
        {
            public Vec3 Position;
            public Vec3 Normal;
            public u8 r, g, b, a;

            public this(Vec3 position, Vec3 normal, Vec4 color)
            {
                Position = position;
                Normal = normal;
                r = (u8)(color.x * 255.0f);
                g = (u8)(color.y * 255.0f);
                b = (u8)(color.z * 255.0f);
                a = (u8)(color.w * 255.0f);
            }
        }

        [OnCompile(.TypeInit)]
        private static void ComptimeSizeChecks()
        {
            //D3D11 constant buffers must align to 16 bytes
            Runtime.Assert(strideof(PerFrameConstants) % 16 == 0);
            Runtime.Assert(strideof(PerObjectConstants) % 16 == 0);
        }

        public this(ID3D11Device* device, ID3D11DeviceContext* context)
        {
            _device = device;
            _context = context;
            Init();
            InitPrimitiveState();
            if (ErrorOccurred)
                Logger.Fatal("An error occurred during scene initialization. Rendering for that scene is now disabled.");

            Camera.Init(.(312.615f, 56.846f, -471.078f), 80.0f, .(ViewWidth, ViewHeight), 1.0f, 10000.0f); 
        }

        public void Draw(f32 deltaTime)
        {
            if (ErrorOccurred || !PrimitiveMaterialsSet)
                return;

            TotalTime += deltaTime;

            //Reload edited shaders
            RenderMaterials.ReloadEditedShaders();

            _context.ClearRenderTargetView(_viewTexture.RenderTargetView, (f32*)&ClearColor);
            _context.ClearDepthStencilView(_depthBufferTexture.DepthStencilView, .DEPTH | .STENCIL, 1.0f, 0);
            _context.OMSetRenderTargets(1, _viewTexture.RenderTargetViewPointer, _depthBufferTexture.DepthStencilView);
            _context.RSSetViewports(1, &_viewport);

            //Update per-frame constant buffer
            PerFrameConstants.Time += deltaTime;
            PerFrameConstants.ViewPos = .(Camera.Position.x, Camera.Position.y, Camera.Position.z, 1.0f);
            PerFrameConstants.ViewportDimensions = .(ViewWidth, ViewHeight);
            _perFrameConstantsBuffer.SetData(_context, &PerFrameConstants);
            _context.PSSetConstantBuffers(0, 1, &_perFrameConstantsBuffer.Ptr);

            //Prepare state to render triangle strip meshes
            _context.VSSetConstantBuffers(0, 1, &_perObjectConstantsBuffer.Ptr);
            _context.RSSetState(_meshRasterizerState);
            _context.IASetPrimitiveTopology(.D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

            //Render meshes
            for (var kv in ObjectsByMaterial)
            {
                //Batch render objects by material
                Material material = kv.key;
                material.Use(_context);
                for (RenderObject renderObject in kv.value)
                    renderObject.Draw(_context, _perObjectConstantsBuffer, Camera);
            }

            //Prepare state to render primitives
            _context.RSSetState(_primitiveRasterizerState);

            //Send primitive vertices to gpu if needed
            if (_primitiveBufferNeedsUpdate)
                UpdatePrimitiveBuffers();

            //Fill per-object buffer for use on all primitives
            {
                PerObjectConstants constants;
                Mat4 rotation = .Identity;
                Mat4 translation = Mat4.Translation(0.0f, 0.0f, 0.0f);
                Mat4 scale = Mat4.Scale(1.0f, 1.0f, 1.0f);
                Mat4 model = translation * rotation * scale;
                constants.MVP = model * Camera.View * Camera.Projection;
                constants.MVP = Mat4.Transpose(constants.MVP);
                constants.Rotation = rotation;

                //_perObjectConstantsBuffer.UseAsConstantBuffer(_context, 1);
                _context.GSSetConstantBuffers(0, 1, &_perObjectConstantsBuffer.Ptr);
                _context.GSSetConstantBuffers(0, 1, &_perFrameConstantsBuffer.Ptr);
                _perObjectConstantsBuffer.SetData(_context, &constants);
            }

            u32 offset = 0;
            u32 strideColoredVertex = sizeof(ColoredVertex);
            u32 strideLitVertex = sizeof(ColoredVertexLit);

            //Draw linelist primitives
            _lineListMaterial.Use(_context);
            _context.IASetPrimitiveTopology(.D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
            _context.IASetVertexBuffers(0, 1, &_lineVertexBuffer.Ptr, &strideColoredVertex, &offset);
            _context.Draw((u32)_lineVertices.Count, 0);

            //Draw unlit triangle list primitives
            offset = 0;
            _triangleListMaterial.Use(_context);
            _context.IASetPrimitiveTopology(.D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            _context.IASetVertexBuffers(0, 1, &_triangleListVertexBuffer.Ptr, &strideColoredVertex, &offset);
            _context.Draw((u32)_triangleListVertices.Count, 0);

            //Draw lit triangle list primitives
            offset = 0;
            _litTriangleListMaterial.Use(_context);
            _context.IASetPrimitiveTopology(.D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            _context.IASetVertexBuffers(0, 1, &_litTriangleListVertexBuffer.Ptr, &strideLitVertex, &offset);
            _context.Draw((u32)_litTriangleListVertices.Count, 0);

            ClearPrimitiveVertexBuffers();
        }

        public Result<RenderObject> CreateRenderObject(StringView materialName, Mesh mesh, Vec3 position, Mat3 rotation)
        {
            ScopedLock!(_renderObjectCreationLock);
            if (RenderMaterials.GetMaterial(materialName) case .Ok(Material material))
            {
                RenderObject renderObject = new .(mesh, position, rotation);
                RenderObjects.Add(renderObject);
                if (!ObjectsByMaterial.ContainsKey(material))
                    ObjectsByMaterial[material] = new List<RenderObject>();

                ObjectsByMaterial[material].Add(renderObject);
                return renderObject;
            }
            else
            {
                return .Err;
            }
        }

        public Result<RenderChunk> CreateRenderChunk(StringView materialName, Mesh mesh, Vec3 position, Mat3 rotation, ChunkVariant variant)
        {
            ScopedLock!(_renderObjectCreationLock);
            if (RenderMaterials.GetMaterial(materialName) case .Ok(Material material))
            {
                RenderChunk renderChunk = new .(mesh, position, rotation, variant);
                RenderObjects.Add(renderChunk);
                if (!ObjectsByMaterial.ContainsKey(material))
                    ObjectsByMaterial[material] = new List<RenderObject>();

                ObjectsByMaterial[material].Add(renderChunk);
                return renderChunk;
            }
            else
            {
                return .Err;
            }
        }

        private void UpdatePrimitiveBuffers()
        {
            //Update line list vertex buffer
            {
                _lineVertexBuffer.ResizeIfNeeded(_device, sizeof(ColoredVertex) * (u32)_lineVertices.Count);
                D3D11_MAPPED_SUBRESOURCE mappedResource = .();
                ZeroMemory(&mappedResource);
                _context.Map(_lineVertexBuffer.Ptr, 0, .WRITE_DISCARD, 0, &mappedResource);
                Internal.MemCpy(mappedResource.pData, _lineVertices.Ptr, sizeof(ColoredVertex) * (u32)_lineVertices.Count);
                _context.Unmap(_lineVertexBuffer.Ptr, 0);
            }

            //Update triangle list vertex buffer
            {
                _triangleListVertexBuffer.ResizeIfNeeded(_device, sizeof(ColoredVertex) * (u32)_triangleListVertices.Count);
                D3D11_MAPPED_SUBRESOURCE mappedResource = .();
                ZeroMemory(&mappedResource);
                _context.Map(_triangleListVertexBuffer.Ptr, 0, .WRITE_DISCARD, 0, &mappedResource);
                Internal.MemCpy(mappedResource.pData, _triangleListVertices.Ptr, sizeof(ColoredVertex) * (u32)_triangleListVertices.Count);
                _context.Unmap(_triangleListVertexBuffer.Ptr, 0);
            }

            //Update lit triangle list vertex buffer
            {
                _litTriangleListVertexBuffer.ResizeIfNeeded(_device, sizeof(ColoredVertex) * (u32)_litTriangleListVertices.Count);
                D3D11_MAPPED_SUBRESOURCE mappedResource = .();
                ZeroMemory(&mappedResource);
                _context.Map(_litTriangleListVertexBuffer.Ptr, 0, .WRITE_DISCARD, 0, &mappedResource);
                Internal.MemCpy(mappedResource.pData, _litTriangleListVertices.Ptr, sizeof(ColoredVertex) * (u32)_litTriangleListVertices.Count);
                _context.Unmap(_litTriangleListVertexBuffer.Ptr, 0);
            }
        }

        public void Resize(u32 width, u32 height)
        {
            if (ViewWidth != width || ViewHeight != height)
            {
                ViewWidth = width;
                ViewHeight = height;
                _viewport.TopLeftX = 0.0f;
                _viewport.TopLeftY = 0.0f;
                _viewport.Width = ViewWidth;
                _viewport.Height = ViewHeight;
                _viewport.MinDepth = 0.0f;
                _viewport.MaxDepth = 1.0f;

                //Recreate scene resources
                Init();
                Camera.Resize(width, height);
            }
        }

        private void Init()
        {
            //Setup texture that camera view will be stored in
            DXGI_FORMAT sceneViewFormat = .R32G32B32A32_FLOAT;
            if (_viewTexture.Init(_device, ViewWidth, ViewHeight, sceneViewFormat, .RENDER_TARGET | .SHADER_RESOURCE) case .Err)
            {
                Logger.Error("Failed to initialize view texture for a scene.");
                ErrorOccurred = true;
                return;
            }
            _viewTexture.CreateRenderTargetView(); //Allows us to use texture as a render target
            _viewTexture.CreateShaderResourceView(); //Lets shaders view texture. Used by dear imgui to draw textures in gui
#if DEBUG
            _viewTexture.SetDebugName("SceneViewTexture");
#endif

            //Setup per frame constants buffer
            if (_perFrameConstantsBuffer.Init(_device, sizeof(PerFrameConstants), .CONSTANT_BUFFER) case .Err)
            {
                Logger.Error("Failed to initialize the per frame constants buffer for a scene");
                ErrorOccurred = true;
                return;
            }

            //Setup per object constants buffer
            if (_perObjectConstantsBuffer.Init(_device, sizeof(PerObjectConstants), .CONSTANT_BUFFER) case .Err)
            {
                Logger.Error("Failed to initialize the per object constants buffer for a scene");
                ErrorOccurred = true;
                return;
            }

            //Setup mesh rasterizer state
            D3D11_RASTERIZER_DESC desc;
            ZeroMemory(&desc);
            desc.FillMode = .SOLID;
            desc.CullMode = .BACK;
            desc.FrontCounterClockwise = 0;
            desc.DepthBias = 0;
            desc.DepthBiasClamp = 0;
            desc.SlopeScaledDepthBias = 0;
            desc.DepthClipEnable = 1;
            desc.ScissorEnable = 0;
            desc.MultisampleEnable = 0;
            desc.AntialiasedLineEnable = 0;
            if (FAILED!(_device.CreateRasterizerState(&desc, &_meshRasterizerState)))
            {
                Logger.Error("Failed to initialize the mesh rasterizer state for a scene");
                ErrorOccurred = true;
                return;
            }

            //Initialize depth buffer
            if (_depthBufferTexture.Init(_device, ViewWidth, ViewHeight, .D24_UNORM_S8_UINT, .DEPTH_STENCIL) case .Err)
            {
                Logger.Error("Failed to initialize the depth buffer for a scene");
                ErrorOccurred = true;
                return;
            }
            _depthBufferTexture.CreateDepthStencilView();

            //Initialize render target
            _context.OMSetRenderTargets(1, _viewTexture.RenderTargetViewPointer, _depthBufferTexture.DepthStencilView);
            _context.RSSetViewports(1, &_viewport);
        }

        private void InitPrimitiveState()
        {
            //Setup primitive rasterizer state
            D3D11_RASTERIZER_DESC desc;
            ZeroMemory(&desc);
            desc.FillMode = .SOLID;
            desc.CullMode = .NONE;
            desc.FrontCounterClockwise = 0;
            desc.DepthBias = 0;
            desc.DepthBiasClamp = 0;
            desc.SlopeScaledDepthBias = 0;
            desc.DepthClipEnable = 1;
            desc.ScissorEnable = 0;
            desc.MultisampleEnable = 0;
            desc.AntialiasedLineEnable = 0;
            if (FAILED!(_device.CreateRasterizerState(&desc, &_primitiveRasterizerState)))
            {
                Logger.Error("Failed to initialize the primitive rasterizer state for a scene");
                ErrorOccurred = true;
                return;
            }

            //Create primitive vertex buffers
            let initialPrimitiveVertices = 25000; //Initial size of primitive buffers. Made fairly large to reduce the need for resizes at runtime. They cause lag when large enough.
            if (_lineVertexBuffer.Init(_device, initialPrimitiveVertices * sizeof(ColoredVertex), .VERTEX_BUFFER, null, .DYNAMIC, .WRITE) case .Err)
            {
                Logger.Error("Failed to initialize vertex buffer for line list primitives");
                ErrorOccurred = true;
                return;
            }
            if (_triangleListVertexBuffer.Init(_device, initialPrimitiveVertices * sizeof(ColoredVertex), .VERTEX_BUFFER, null, .DYNAMIC, .WRITE) case .Err)
            {
                Logger.Error("Failed to initialize vertex buffer for triangle list primitives");
                ErrorOccurred = true;
                return;
            }
            if (_litTriangleListVertexBuffer.Init(_device, initialPrimitiveVertices * sizeof(ColoredVertexLit), .VERTEX_BUFFER, null, .DYNAMIC, .WRITE) case .Err)
            {
                Logger.Error("Failed to initialize vertex buffer for lit triangle list primitives");
                ErrorOccurred = true;
                return;
            }

            //Get primitive materials
            if (RenderMaterials.GetMaterial("Linelist") case .Ok(var mat))
                _lineListMaterial = mat;
            if (RenderMaterials.GetMaterial("SolidTriList") case .Ok(var mat))
                _triangleListMaterial = mat;
            if (RenderMaterials.GetMaterial("LitTriList") case .Ok(var mat))
                _litTriangleListMaterial = mat;

            if (!PrimitiveMaterialsSet)
            {
                Logger.Fatal("Failed to load primitive renderer materials");
                ErrorOccurred = true;
                return;
            }

            _primitiveBufferNeedsUpdate = true;
        }

        private void ClearPrimitiveVertexBuffers()
        {
            _lineVertices.Clear();
            _triangleListVertices.Clear();
            _litTriangleListVertices.Clear();
            _primitiveBufferNeedsUpdate = true;
        }

        public void DrawLine(Vec3 start, Vec3 end, Vec4 color)
        {
            _lineVertices.Add(.(start, color));
            _lineVertices.Add(.(end, color));
            _primitiveBufferNeedsUpdate = true;
        }

        public void DrawQuad(Vec3 bottomLeft, Vec3 topLeft, Vec3 topRight, Vec3 bottomRight, Vec4 color)
        {
            DrawLine(bottomLeft, topLeft, color);
            DrawLine(topLeft, topRight, color);
            DrawLine(topRight, bottomRight, color);
            DrawLine(bottomRight, bottomLeft, color);
        }

        public void DrawBox(Vec3 min, Vec3 max, Vec4 color)
        {
            Vec3 size = max - min;
            Vec3 bottomLeftFront = min;
            Vec3 bottomLeftBack = min + .(size.x, 0.0f, 0.0f);
            Vec3 bottomRightFront = min + .(0.0f, 0.0f, size.z);
            Vec3 bottomRightBack = min + .(size.x, 0.0f, size.z);

            Vec3 topRightBack = max;
            Vec3 topLeftBack = .(bottomLeftBack.x, max.y, bottomLeftBack.z);
            Vec3 topRightFront = .(bottomRightFront.x, max.y, bottomRightFront.z);
            Vec3 topLeftFront = .(min.x, max.y, min.z);

            //Draw quads for the front and back faces
            DrawQuad(bottomLeftFront, topLeftFront, topRightFront, bottomRightFront, color);
            DrawQuad(bottomLeftBack, topLeftBack, topRightBack, bottomRightBack, color);

            //Draw lines connecting the two faces
            DrawLine(bottomLeftFront, bottomLeftBack, color);
            DrawLine(topLeftFront, topLeftBack, color);
            DrawLine(topRightFront, topRightBack, color);
            DrawLine(bottomRightFront, bottomRightBack, color);

            _primitiveBufferNeedsUpdate = true;
        }

        public void DrawBoxRotated(Vec3 min, Vec3 max, Mat3 orient, Vec3 position, Vec4 color)
        {
            Vec3 size = max - min;
            Vec3 bottomLeftFront = min;
            Vec3 bottomLeftBack = min + .(size.x, 0.0f, 0.0f);
            Vec3 bottomRightFront = min + .(0.0f, 0.0f, size.z);
            Vec3 bottomRightBack = min + .(size.x, 0.0f, size.z);

            Vec3 topRightBack = max;
            Vec3 topLeftBack = .(bottomLeftBack.x, max.y, bottomLeftBack.z);
            Vec3 topRightFront = .(bottomRightFront.x, max.y, bottomRightFront.z);
            Vec3 topLeftFront = .(min.x, max.y, min.z);

            bottomLeftFront -= position;
            bottomLeftBack -= position;
            bottomRightFront -= position;
            bottomRightBack -= position;
            topRightBack -= position;
            topLeftBack -= position;
            topRightFront -= position;
            topLeftFront -= position;

            bottomLeftFront = orient.RotatePoint(bottomLeftFront);
            bottomLeftBack = orient.RotatePoint(bottomLeftBack);
            bottomRightFront = orient.RotatePoint(bottomRightFront);
            bottomRightBack = orient.RotatePoint(bottomRightBack);
            topRightBack = orient.RotatePoint(topRightBack);
            topLeftBack = orient.RotatePoint(topLeftBack);
            topRightFront = orient.RotatePoint(topRightFront);
            topLeftFront = orient.RotatePoint(topLeftFront);

            bottomLeftFront += position;
            bottomLeftBack += position;
            bottomRightFront += position;
            bottomRightBack += position;
            topRightBack += position;
            topLeftBack += position;
            topRightFront += position;
            topLeftFront += position;

            //Draw quads for the front and back faces
            DrawQuad(bottomLeftFront, topLeftFront, topRightFront, bottomRightFront, color);
            DrawQuad(bottomLeftBack, topLeftBack, topRightBack, bottomRightBack, color);

            //Draw lines connecting the two faces
            DrawLine(bottomLeftFront, bottomLeftBack, color);
            DrawLine(topLeftFront, topLeftBack, color);
            DrawLine(topRightFront, topRightBack, color);
            DrawLine(bottomRightFront, bottomRightBack, color);

            _primitiveBufferNeedsUpdate = true;
        }

        public void DrawQuadSolid(Vec3 bottomLeft, Vec3 topLeft, Vec3 topRight, Vec3 bottomRight, Vec4 color)
        {
            //First triangle
            _triangleListVertices.Add(.(bottomLeft, color));
            _triangleListVertices.Add(.(topLeft, color));
            _triangleListVertices.Add(.(topRight, color));

            //Second triangle
            _triangleListVertices.Add(.(topRight, color));
            _triangleListVertices.Add(.(bottomRight, color));
            _triangleListVertices.Add(.(bottomLeft, color));

            _primitiveBufferNeedsUpdate = true;
        }

        public void DrawBoxSolid(Vec3 min, Vec3 max, Vec4 color)
        {
            Vec3 size = max - min;
            Vec3 bottomLeftFront = min;
            Vec3 bottomLeftBack = min + .(size.x, 0.0f, 0.0f);
            Vec3 bottomRightFront = min + .(0.0f, 0.0f, size.z);
            Vec3 bottomRightBack = min + .(size.x, 0.0f, size.z);

            Vec3 topRightBack = max;
            Vec3 topLeftBack = .(bottomLeftBack.x, max.y, bottomLeftBack.z);
            Vec3 topRightFront = .(bottomRightFront.x, max.y, bottomRightFront.z);
            Vec3 topLeftFront = .(min.x, max.y, min.z);

            //Draw quads for each face
            DrawQuadSolid(bottomLeftFront, topLeftFront, topRightFront, bottomRightFront, color);     //Front
            DrawQuadSolid(bottomLeftBack, topLeftBack, topRightBack, bottomRightBack, color);         //Back
            DrawQuadSolid(bottomLeftBack, topLeftBack, topLeftFront, bottomLeftFront, color);         //Left
            DrawQuadSolid(bottomRightFront, topRightFront, topRightBack, bottomRightBack, color);     //Right
            DrawQuadSolid(topLeftFront, topLeftBack, topRightBack, topRightFront, color);             //Top
            DrawQuadSolid(bottomLeftFront, bottomLeftBack, bottomRightBack, bottomRightFront, color); //Bottom

            _primitiveBufferNeedsUpdate = true;
        }

        public void DrawQuadLit(Vec3 bottomLeft, Vec3 topLeft, Vec3 topRight, Vec3 bottomRight, Vec4 color)
        {
            //First triangle
            _litTriangleListVertices.Add(.(bottomLeft, color));
            _litTriangleListVertices.Add(.(topLeft, color));
            _litTriangleListVertices.Add(.(topRight, color));

            //Second triangle
            _litTriangleListVertices.Add(.(topRight, color));
            _litTriangleListVertices.Add(.(bottomRight, color));
            _litTriangleListVertices.Add(.(bottomLeft, color));

            _primitiveBufferNeedsUpdate = true;
        }

        public void DrawBoxLit(Vec3 min, Vec3 max, Vec4 color)
        {
            Vec3 size = max - min;
            Vec3 bottomLeftFront = min;
            Vec3 bottomLeftBack = min + .(size.x, 0.0f, 0.0f);
            Vec3 bottomRightFront = min + .(0.0f, 0.0f, size.z);
            Vec3 bottomRightBack = min + .(size.x, 0.0f, size.z);

            Vec3 topRightBack = max;
            Vec3 topLeftBack = .(bottomLeftBack.x, max.y, bottomLeftBack.z);
            Vec3 topRightFront = .(bottomRightFront.x, max.y, bottomRightFront.z);
            Vec3 topLeftFront = .(min.x, max.y, min.z);

            //Draw quads for each face
            DrawQuadLit(bottomLeftFront, topLeftFront, topRightFront, bottomRightFront, color);     //Front
            DrawQuadLit(bottomLeftBack, topLeftBack, topRightBack, bottomRightBack, color);         //Back
            DrawQuadLit(bottomLeftBack, topLeftBack, topLeftFront, bottomLeftFront, color);         //Left
            DrawQuadLit(bottomRightFront, topRightFront, topRightBack, bottomRightBack, color);     //Right
            DrawQuadLit(topLeftFront, topLeftBack, topRightBack, topRightFront, color);             //Top
            DrawQuadLit(bottomLeftFront, bottomLeftBack, bottomRightBack, bottomRightFront, color); //Bottom

            _primitiveBufferNeedsUpdate = true;
        }
	}

    //D3D11 constant buffer size must be a multiple of 16. Marked with Crepr some field order is preserved. We assume the same field order in the shaders
    [Align(16), CRepr]
    public struct PerFrameConstants
    {
        public Vec4 ViewPos = .Zero;
        public Vec4 DiffuseColor = .(1.0f, 1.0f, 1.0f, 1.0f);
        public f32 DiffuseIntensity = 1.0f;
        public f32 ElevationFactorBias = 0.8f;
        public i32 ShadeMode = 1;
        public f32 Time = 0.0f;
        public Vec2 ViewportDimensions = .Zero;
    }
}