using Common;
using System;
using Common.Math;
using Direct3D;

namespace Nanoforge.Render.Resources
{
    [Align(16), CRepr]
    public struct PerObjectConstants
    {
        public Mat4 MVP;
        public Mat4 Rotation;
        public Vec4 WorldPosition;
    }

	public class RenderObject
	{
        public Mesh ObjectMesh;
        public Vec3 Scale = .(1.0f, 1.0f, 1.0f);
        public Vec3 Position = .Zero;
        public Mat3 Rotation = .Identity;
        public bool Visible = true;
        public bool UseTextures = false;
        public Texture2D[10] Textures;

        public this()
        {

        }

        public this(Mesh mesh, Vec3 position, Mat3 rotation = .Identity)
        {
            ObjectMesh = mesh;
            Position = position;
            Rotation = rotation;
        }

        public virtual void Draw(ID3D11DeviceContext* context, Buffer perObjectBuffer, Camera3D cam)
        {
            if (!Visible)
                return;

            PerObjectConstants constants = .();

            //Calculate matrices
            Mat4 rotation = .();
            rotation.Vectors[0] = .(Rotation.Vectors[0].x, Rotation.Vectors[0].y, Rotation.Vectors[0].z, 0.0f);
            rotation.Vectors[1] = .(Rotation.Vectors[1].x, Rotation.Vectors[1].y, Rotation.Vectors[1].z, 0.0f);
            rotation.Vectors[2] = .(Rotation.Vectors[2].x, Rotation.Vectors[2].y, Rotation.Vectors[2].z, 0.0f);
            rotation.Vectors[3] = .(0.0f, 0.0f, 0.0f, 1.0f);
            Mat4 translation = Mat4.Translation(Position);
            Mat4 scale = Mat4.Scale(Scale);
            Mat4 model = rotation * translation * scale;

            //Calculate final MVP matrix + set other constants
            constants.MVP = Mat4.Transpose(model * cam.View * cam.Projection);
            constants.Rotation = rotation;
            constants.WorldPosition = .(Position.x, Position.y, Position.z, 1.0f);

            //Set constants
            perObjectBuffer.SetData(context, &constants);

            //Bind textures
            if (UseTextures)
            {
                for (u32 i in 0 ... 9)
                {
                    if (Textures[i] != null)
                    {
                        Textures[i].Bind(context, i);
                    }
                }
            }

            ObjectMesh.Draw(context);
        }

        public virtual RenderObject Clone(RenderObject existingObject = null)
        {
            RenderObject clone = (existingObject == null) ? new RenderObject() : existingObject;

            clone.ObjectMesh = ObjectMesh;
            clone.Scale = Scale;
            clone.Position = Position;
            clone.Rotation = Rotation;
            clone.Visible = Visible;
            clone.UseTextures = UseTextures;
            for (int i in 0..<10)
            {
                clone.Textures[i] = Textures[i];
            }

            return clone;
        }
	}
}