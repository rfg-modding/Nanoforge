using System;
using System.Collections.Generic;
using System.IO;
using System.Numerics;
using Avalonia;
using Avalonia.OpenGL;
using Nanoforge.Render.Resources;
using RFGM.Formats.Meshes;
using RFGM.Formats.Meshes.Shared;
using Silk.NET.Maths;
using Silk.NET.OpenGL;
using Silk.NET.Windowing;
using Shader = Nanoforge.Render.Resources.Shader;
using Texture = Nanoforge.Render.Resources.Texture;

namespace Nanoforge.Render;

public class Renderer
{
    private GL _gl;

    public Mesh Mesh;
    
    public static Vector3 CameraPosition = new Vector3(0.0f, 25.0f, 50.0f);
    public static Vector3 CameraFront = new Vector3(0.0f, 0.0f, -1.0f);
    public static Vector3 CameraUp = Vector3.UnitY;
    public static Vector3 CameraDirection = Vector3.Zero;
    public static float CameraYaw = -90f;
    public static float CameraPitch = 0f;
    public static float CameraZoom = 45f;

    public Renderer(GlInterface glInterface)
    {
        _gl = GL.GetApi(glInterface.GetProcAddress);
        Materials.Init(_gl);
        
        MeshInstanceData mesh = LoadStaticMesh("/mnt/StorageC/_RFGR_Unpack/GOG_Terraform/Unpack/items/Unpack/artillery_gun/tharsis_gun_weapon.csmesh_pc");
        Material material = Materials.GetMaterial(mesh.Config.VertexFormat.ToString());
        Mesh = new Mesh(_gl, mesh, material);
    }

    private MeshInstanceData LoadStaticMesh(string cpuFilePath)
    {
        try
        {
            string gpuFilePath = $"{Path.GetDirectoryName(cpuFilePath)}/{Path.GetFileNameWithoutExtension(cpuFilePath)}.gsmesh_pc";
            FileStream cpuFile = new(cpuFilePath, FileMode.Open);
            FileStream gpuFile = new(gpuFilePath, FileMode.Open);
        
            StaticMesh staticMesh = new();
            staticMesh.ReadHeader(cpuFile);
            MeshInstanceData meshData = staticMesh.ReadData(gpuFile);
            //TODO: Pass NumLods to mesh
            
            if (meshData.Config.Topology != PrimitiveTopology.TriangleStrip)
            {
                //Adding this warning
                throw new Exception("Encountered static mesh with primitive topology other than triangle strip.");
            }

            return meshData;
            
            var a = 2;
        }
        catch (Exception e)
        {
            Console.WriteLine(e);
            throw;
        }
    }

    public void RenderFrame(int fb, Rect bounds, float deltaTime, float totalSeconds)
    {
        _gl.ClearColor(0, 0, 0, 255);
        _gl.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit);
        _gl.Enable(EnableCap.DepthTest);
        _gl.Viewport(0, 0, (uint)bounds.Width, (uint)bounds.Height);

        float angle = totalSeconds * 50.0f;//(float)DateTime.Now.Millisecond * (360.0f / 60.0f / 1000.0f);

        var model = Matrix4x4.CreateRotationY(MathHelper.DegreesToRadians(angle));
        var view = Matrix4x4.CreateLookAt(CameraPosition, new Vector3(0.0f, 0.0f, 0.0f), CameraUp);
        var projection = Matrix4x4.CreatePerspectiveFieldOfView(MathHelper.DegreesToRadians(CameraZoom), (float)(bounds.Width / bounds.Height), 0.1f, 100.0f);

        Mesh.Bind();
        Material material = Mesh.Material;
        material.Shader.Use();
        material.Shader.SetUniform("uModel", model);
        material.Shader.SetUniform("uView", view);
        material.Shader.SetUniform("uProjection", projection); 
        Mesh.Draw();
    }

    public void FramebufferResize(Vector2D<int> newSize)
    {
        _gl.Viewport(newSize);
    }

    public void Shutdown()
    {
        Mesh.Dispose();
        Materials.Dispose();
    }
}