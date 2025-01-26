using Common.Math;
using Common;
using Nanoforge.Misc;
using Nanoforge.App;
using System;
using ImGui;

namespace Nanoforge.Render
{
    public class Camera3D
    {
        public Vec3 Position = .Zero;
        public Vec3 TargetPosition;
        public Mat4 View = .Identity;
        public Mat4 Projection = .Identity;
        public f32 Speed = 3.0f;
		public f32 MinSpeed = 0.1f;
		public f32 MaxSpeed = 100.0f;
        public f32 SprintMultiplier = 2.0f;
        public f32 MovementSmoothing = 0.125f;
        public f32 LookSensitivity = 0.01f;
        public bool InputEnabled = true;

        private f32 _fovRadians = 0.0f;
        private f32 _pitchRadians = 0.0f;
        private f32 _yawRadians = 0.0f;
        private Vec2 _viewportDimensions;
        private f32 _aspectRatio = 0.0f;
        private f32 _nearPlane = 0.0f;
        private f32 _farPlane = 0.0f;

        private Vec3 _camUp = .Zero;
        private Vec3 _camRight = .Zero;
        private Vec3 _camForward = .Zero;

		public f32 FovDegrees
		{
			get
			{
				return Math.RadiansToDegrees(_fovRadians);
			}
			set
			{
				_fovRadians = Math.DegreesToRadians(value);
				UpdateProjectionMatrix();
			}
		}

        public void Init(Vec3 initialPos, f32 initialFovDegrees, Vec2 viewportDimensions, f32 nearPlane, f32 farPlane)
        {
            Position = initialPos;
            TargetPosition = initialPos;
            _fovRadians = Math.ToRadians(initialFovDegrees);
            _viewportDimensions = viewportDimensions;
            _aspectRatio = viewportDimensions.x / viewportDimensions.y;
            _nearPlane = nearPlane;
            _farPlane = farPlane;

            Vec3 forward = (-1.0f * initialPos).Normalized();
            _pitchRadians = Math.Asin(-forward.y);
            _yawRadians = 0.0f;

            UpdateViewMatrix();
            UpdateProjectionMatrix();
        }

        public void Update(App app)
        {
            Input input = app.GetResource<Input>();
            bool ctrlDown = input.ControlDown;
            bool shiftDown = input.ShiftDown;
            if (InputEnabled && !ctrlDown)
            {
                f32 speed = shiftDown ? SprintMultiplier * Speed : Speed;

                if (input.KeyDown(.W))
                    TargetPosition += Forward() * speed; 
                else if (input.KeyDown(.S))
                    TargetPosition += Backward() * speed;

                if (input.KeyDown(.A))
                    TargetPosition += Left() * speed;
                else if (input.KeyDown(.D))
                    TargetPosition += Right() * speed;

                if (input.KeyDown(.Z))
                    TargetPosition += Up() * speed;
                else if (input.KeyDown(.X))
                    TargetPosition += Down() * speed;

				if (input.MouseScrollY != 0.0f)
				{
					f32 scrollDelta = (f32)input.MouseScrollY / 1000.0f;
					System.Diagnostics.Debug.WriteLine(scope $"{scrollDelta}");
					Speed += scrollDelta;
					if (Speed < MinSpeed)
					    Speed = MinSpeed;
					if (Speed > MaxSpeed)
					    Speed = MaxSpeed;
				}
            }

            Position = Math.Lerp(Position, TargetPosition, MovementSmoothing);
            UpdateViewMatrix();
            UpdateRotationFromMouse(input);
        }

        public void Resize(u32 width, u32 height)
        {
            _viewportDimensions = .(width, height);
            UpdateProjectionMatrix();
        }

        public void UpdateViewMatrix()
        {
            Vec3 defaultForward = .(0.0f, 0.0f, 1.0f);
            Vec3 defaultRight = .(1.0f, 0.0f, 0.0f);

            //Form rotation matrix
            Mat4 rotation = Mat4.RotationPitchYawRoll(_pitchRadians, _yawRadians, 0.0f);

            //Recalculate right, forward, and up
            _camRight = DirectXMath.Transform(defaultRight, rotation);
            _camForward = DirectXMath.Transform(defaultForward, rotation);
            _camUp = _camForward.Cross(_camRight);

            //Recalculate view matrix
            Vec3 focus = Position + DirectXMath.Transform(defaultForward, rotation).Normalized();
            View = Mat4.LookAtLH(Position, focus, _camUp);
        }

        private void UpdateProjectionMatrix()
        {
            Projection = Mat4.PerspectiveFovLH(_fovRadians, _viewportDimensions.x / _viewportDimensions.y, _nearPlane, _farPlane);
        }

        private void UpdateRotationFromMouse(Input input)
        {
            if (input.MouseButtonDown(.Right))
            {
                _yawRadians += input.MouseDeltaX * LookSensitivity;
                _pitchRadians += input.MouseDeltaY * LookSensitivity;

                let maxPitch = Math.ToRadians(89.0f);
                let minPitch = Math.ToRadians(-89.0f);

                if (_pitchRadians > maxPitch)
                    _pitchRadians = maxPitch;
                if (_pitchRadians < minPitch)
                    _pitchRadians = minPitch;
            }
        }

        private Vec3 Up()
        {
            return _camUp;
        }

        private Vec3 Down()
        {
            return -1.0f * Up();
        }

        private Vec3 Left()
        {
            return -1.0f * Right();
        }

        private Vec3 Right()
        {
            return _camRight;
        }

        private Vec3 Forward()
        {
            return _camForward;
        }

        private Vec3 Backward()
        {
            return -1.0f * Forward();
        }
    }
}
