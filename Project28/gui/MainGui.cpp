#include "MainGui.h"
#include "common/Typedefs.h"
#include "render/imgui/ImGuiFontManager.h"
#include "rfg/PackfileVFS.h"
#include "render/imgui/imgui_ext.h"
#include "render/camera/Camera.h"
#include <imgui/imgui.h>
#include <im3d.h>
#include <im3d_math.h>

void MainGui::Update(f32 deltaTime)
{
    //Run gui code
#ifdef DEBUG_BUILD
    static bool show_demo_window = true;
    ImGui::ShowDemoWindow(&show_demo_window);
#endif

    DrawMainMenuBar();
    DrawDockspace();
    DrawFileExplorer();
	DrawCameraWindow();
    DrawIm3dPrimitives();
	UpdateCamera(deltaTime);
}

void MainGui::HandleResize()
{
	RECT rect;
	RECT usableRect;

	if (GetClientRect(hwnd_, &usableRect))
	{
		windowWidth_ = usableRect.right - usableRect.left;
		windowHeight_ = usableRect.bottom - usableRect.top;
	}
}

void MainGui::DrawMainMenuBar()
{
    ImGuiMainMenuBar
    (
        ImGuiMenu("File",
            ImGuiMenuItemShort("Open file", )
            ImGuiMenuItemShort("Save file", )
            ImGuiMenuItemShort("Exit", )
        )
        ImGuiMenu("Help",
            ImGuiMenuItemShort("Welcome", )
            ImGuiMenuItemShort("Metrics", )
            ImGuiMenuItemShort("About", )
        )
    )
}

void MainGui::DrawDockspace()
{
    //Dockspace flags
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

    //Parent window flags
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetWorkPos());
    ImGui::SetNextWindowSize(viewport->GetWorkSize());
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace parent window", &Visible, window_flags);
    ImGui::PopStyleVar(3);

    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiID dockspace_id = ImGui::GetID("Editor dockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }

    ImGui::End();
}

void MainGui::DrawFileExplorer()
{
    ImGui::Begin("File explorer");

    fontManager_->FontL.Push();
    ImGui::Text(ICON_FA_ARCHIVE " Packfiles");
    fontManager_->FontL.Pop();
    ImGui::Separator();
    for (auto& packfile : packfileVFS_->packfiles_)
    {
        string packfileNodeLabel = packfile.Name() + " [" + std::to_string(packfile.Header.NumberOfSubfiles) + " subfiles";
        if (packfile.Compressed)
            packfileNodeLabel += ", Compressed";
        if (packfile.Condensed)
            packfileNodeLabel += ", Condensed";

        packfileNodeLabel += "]";

        if (ImGui::TreeNode(packfileNodeLabel.c_str()))
        {
            for (u32 i = 0; i < packfile.Entries.size(); i++)
            {
                ImGui::BulletText("%s - %d bytes", packfile.EntryNames[i], packfile.Entries[i].DataSize);
            }
            ImGui::TreePop();
        }
    }

    ImGui::Separator();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}

void MainGui::DrawCameraWindow()
{
	if (!ImGui::Begin("Camera", &Visible))
	{
		ImGui::End();
		return;
	}

	fontManager_->FontL.Push();
	ImGui::Text(ICON_FA_CAMERA " Camera");
	fontManager_->FontL.Pop();
	ImGui::Separator();

	f32 fov = camera_->GetFov();
	f32 nearPlane = camera_->GetNearPlane();
	f32 farPlane = camera_->GetFarPlane();
	f32 lookSensitivity = camera_->GetLookSensitivity();

	ImGui::InputFloat("Speed", &camera_->Speed);
	ImGui::InputFloat("Sprint speed", &camera_->SprintSpeed);
	//ImGui::InputFloat("Camera smoothing", &camera_->CameraSmoothing);
	ImGui::Separator();

	if (ImGui::InputFloat("Fov", &fov))
		camera_->SetFov(fov);
	if (ImGui::InputFloat("Near plane", &nearPlane))
		camera_->SetNearPlane(nearPlane);
	if (ImGui::InputFloat("Far plane", &farPlane))
		camera_->SetFarPlane(farPlane);
	if (ImGui::InputFloat("Look sensitivity", &lookSensitivity))
		camera_->SetLookSensitivity(lookSensitivity);

	//ImGui::Checkbox("Follow mouse", &camera_->FollowMouse);
	ImGui::Separator();

	if (ImGui::InputFloat3("Position", (float*)&camera_->camPosition, 3))
	{
		camera_->UpdateViewMatrix();
	}
	//gui::LabelAndValue("Position: ", util::to_string(camera_->GetPos()));
	//gui::LabelAndValue("Target position: ", util::to_string(camera_->GetTargetPos()));
	gui::LabelAndValue("Aspect ratio: ", std::to_string(camera_->GetAspectRatio()));
	gui::LabelAndValue("Pitch: ", std::to_string(camera_->GetPitch()));
	gui::LabelAndValue("Yaw: ", std::to_string(camera_->GetYaw()));

	ImGui::End();
}

void RandSeed(int _seed)
{
    srand(_seed);
}
int RandInt(int _min, int _max)
{
    return _min + (int)rand() % (_max - _min);
}
float RandFloat(float _min, float _max)
{
    return _min + (float)rand() / (float)RAND_MAX * (_max - _min);
}
constexpr float Pi = 3.14159265359f;

Im3d::Vec3 RandVec3(float _min, float _max)
{
    return Im3d::Vec3(
        RandFloat(_min, _max),
        RandFloat(_min, _max),
        RandFloat(_min, _max)
    );
}
Im3d::Mat3 RandRotation()
{
    return Im3d::Rotation(Im3d::Normalize(RandVec3(-1.0f, 1.0f)), RandFloat(-Pi, Pi));
}
Im3d::Color RandColor(float _min, float _max)
{
    Im3d::Vec3 v = RandVec3(_min, _max);
    return Im3d::Color(v.x, v.y, v.z);
}

void MainGui::DrawIm3dPrimitives()
{
    ImGui::Begin("Im3d tester");

	Im3d::Context& ctx = Im3d::GetContext();
	Im3d::AppData& ad = Im3d::GetAppData();

	ImGui::InputFloat3("Im3d ray origin", (float*)&ad.m_cursorRayOrigin);
	ImGui::InputFloat3("Im3d ray direction", (float*)&ad.m_cursorRayDirection);

	//Draw grid
	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Grid"))
	{
		static int gridSize = 100;
		ImGui::SliderInt("Grid Size", &gridSize, 1, 1000);
		const float gridHalf = (float)gridSize * 0.5f;
		Im3d::SetAlpha(1.0f);
		Im3d::SetSize(1.0f);
		Im3d::BeginLines();
		for (int x = 0; x <= gridSize; ++x)
		{
			Im3d::Vertex(-gridHalf, 0.0f, (float)x - gridHalf, Im3d::Color(0.0f, 0.0f, 0.0f));
			Im3d::Vertex(gridHalf, 0.0f, (float)x - gridHalf, Im3d::Color(1.0f, 1.0f, 1.0f));
		}
		for (int z = 0; z <= gridSize; ++z)
		{
			Im3d::Vertex((float)z - gridHalf, 0.0f, -gridHalf, Im3d::Color(0.0f, 0.0f, 0.0f));
			Im3d::Vertex((float)z - gridHalf, 0.0f, gridHalf, Im3d::Color(1.0f, 1.0f, 1.0f));
		}
		Im3d::End();

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Cursor Ray Intersection"))
	{
		// Context exposes the 'hot depth' along the cursor ray which intersects with the current hot gizmo - this is useful
		// when drawing the cursor ray.
		float depth = FLT_MAX;
		depth = Im3d::Min(depth, Im3d::GetContext().m_hotDepth);
		float size = Im3d::Clamp(32.0f / depth, 4.0f, 32.0f);

		if (depth != FLT_MAX)
		{
			ImGui::Text("Depth: %f", depth);
			Im3d::PushEnableSorting(true);
			Im3d::BeginPoints();
			Im3d::Vertex(ad.m_cursorRayOrigin + ad.m_cursorRayDirection * depth * 0.99f, size, Im3d::Color_Magenta);
			Im3d::End();
			Im3d::PopEnableSorting();
		}
		else
		{
			ImGui::Text("Depth: FLT_MAX");
		}

		ImGui::TreePop();
	}

	//ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("High Order Shapes"))
	{
		// Im3d provides functions to easily draw high order shapes - these don't strictly require a matrix to be pushed on
		// the stack (although this is supported, as below).
		static Im3d::Mat4 transform(1.0f);
		Im3d::Gizmo("ShapeGizmo", transform);

		enum Shape {
			Shape_Quad,
			Shape_QuadFilled,
			Shape_Circle,
			Shape_CircleFilled,
			Shape_Sphere,
			Shape_SphereFilled,
			Shape_AlignedBox,
			Shape_AlignedBoxFilled,
			Shape_Cylinder,
			Shape_Capsule,
			Shape_Prism,
			Shape_Arrow
		};
		static const char* shapeList =
			"Quad\0"
			"Quad Filled\0"
			"Circle\0"
			"Circle Filled\0"
			"Sphere\0"
			"Sphere Filled\0"
			"AlignedBox\0"
			"AlignedBoxFilled\0"
			"Cylinder\0"
			"Capsule\0"
			"Prism\0"
			"Arrow\0"
			;
		static int currentShape = Shape_Capsule;
		ImGui::Combo("Shape", &currentShape, shapeList);
		static Im3d::Vec4 color = Im3d::Vec4(1.0f, 0.0f, 0.6f, 1.0f);
		ImGui::ColorEdit4("Color", color);
		static float thickness = 4.0f;
		ImGui::SliderFloat("Thickness", &thickness, 0.0f, 16.0f);
		static int detail = -1;

		Im3d::PushMatrix(transform);
		Im3d::PushDrawState();
		Im3d::SetSize(thickness);
		Im3d::SetColor(Im3d::Color(color.x, color.y, color.z, color.w));

		switch ((Shape)currentShape)
		{
		case Shape_Quad:
		case Shape_QuadFilled:
		{
			static Im3d::Vec2 quadSize(1.0f);
			ImGui::SliderFloat2("Size", quadSize, 0.0f, 10.0f);
			if (currentShape == Shape_Quad)
			{
				Im3d::DrawQuad(Im3d::Vec3(0.0f), Im3d::Vec3(0.0f, 0.0f, 1.0f), quadSize);
			}
			else
			{
				Im3d::DrawQuadFilled(Im3d::Vec3(0.0f), Im3d::Vec3(0.0f, 0.0f, 1.0f), quadSize);
			}
			break;
		}
		case Shape_Circle:
		case Shape_CircleFilled:
		{
			static float circleRadius = 1.0f;
			ImGui::SliderFloat("Radius", &circleRadius, 0.0f, 10.0f);
			ImGui::SliderInt("Detail", &detail, -1, 128);
			if (currentShape == Shape_Circle)
			{
				Im3d::DrawCircle(Im3d::Vec3(0.0f), Im3d::Vec3(0.0f, 0.0f, 1.0f), circleRadius, detail);
			}
			else if (currentShape = Shape_CircleFilled)
			{
				Im3d::DrawCircleFilled(Im3d::Vec3(0.0f), Im3d::Vec3(0.0f, 0.0f, 1.0f), circleRadius, detail);
			}
			break;
		}
		case Shape_Sphere:
		case Shape_SphereFilled:
		{
			static float sphereRadius = 1.0f;
			ImGui::SliderFloat("Radius", &sphereRadius, 0.0f, 10.0f);
			ImGui::SliderInt("Detail", &detail, -1, 128);
			if (currentShape == Shape_Sphere)
			{
				Im3d::DrawSphere(Im3d::Vec3(0.0f), sphereRadius, detail);
			}
			else
			{
				Im3d::DrawSphereFilled(Im3d::Vec3(0.0f), sphereRadius, detail);
			}
			break;
		}
		case Shape_AlignedBox:
		case Shape_AlignedBoxFilled:
		{
			static Im3d::Vec3 boxSize(1.0f);
			ImGui::SliderFloat3("Size", boxSize, 0.0f, 10.0f);
			if (currentShape == Shape_AlignedBox)
			{
				Im3d::DrawAlignedBox(-boxSize, boxSize);
			}
			else
			{
				Im3d::DrawAlignedBoxFilled(-boxSize, boxSize);
			}
			break;
		}
		case Shape_Cylinder:
		{
			static float cylinderRadius = 1.0f;
			static float cylinderLength = 1.0f;
			ImGui::SliderFloat("Radius", &cylinderRadius, 0.0f, 10.0f);
			ImGui::SliderFloat("Length", &cylinderLength, 0.0f, 10.0f);
			ImGui::SliderInt("Detail", &detail, -1, 128);
			Im3d::DrawCylinder(Im3d::Vec3(0.0f, -cylinderLength, 0.0f), Im3d::Vec3(0.0f, cylinderLength, 0.0f), cylinderRadius, detail);
			break;
		}
		case Shape_Capsule:
		{
			static float capsuleRadius = 0.5f;
			static float capsuleLength = 1.0f;
			ImGui::SliderFloat("Radius", &capsuleRadius, 0.0f, 10.0f);
			ImGui::SliderFloat("Length", &capsuleLength, 0.0f, 10.0f);
			ImGui::SliderInt("Detail", &detail, -1, 128);
			Im3d::DrawCapsule(Im3d::Vec3(0.0f, -capsuleLength, 0.0f), Im3d::Vec3(0.0f, capsuleLength, 0.0f), capsuleRadius, detail);
			break;
		}
		case Shape_Prism:
		{
			static float prismRadius = 1.0f;
			static float prismLength = 1.0f;
			static int   prismSides = 3;
			ImGui::SliderFloat("Radius", &prismRadius, 0.0f, 10.0f);
			ImGui::SliderFloat("Length", &prismLength, 0.0f, 10.0f);
			ImGui::SliderInt("Sides", &prismSides, 3, 16);
			Im3d::DrawPrism(Im3d::Vec3(0.0f, -prismLength, 0.0f), Im3d::Vec3(0.0f, prismLength, 0.0f), prismRadius, prismSides);
			break;
		}
		case Shape_Arrow:
		{
			static float arrowLength = 1.0f;
			static float headLength = -1.0f;
			static float headThickness = -1.0f;
			ImGui::SliderFloat("Length", &arrowLength, 0.0f, 10.0f);
			ImGui::SliderFloat("Head Length", &headLength, 0.0f, 1.0f);
			ImGui::SliderFloat("Head Thickness", &headThickness, 0.0f, 1.0f);
			Im3d::DrawArrow(Im3d::Vec3(0.0f), Im3d::Vec3(0.0f, arrowLength, 0.0f), headLength, headThickness);
			break;
		}
		default:
			break;
		};

		Im3d::PopDrawState();
		Im3d::PopMatrix();

		ImGui::TreePop();
	}
    if (ImGui::TreeNode("Basic Perf"))
    {
        // Simple perf test: draw a large number of points, enable/disable sorting and the use of the matrix stack.
        static bool enableSorting = false;
        static bool useMatrix = false; // if the matrix stack size == 1 Im3d assumes it's the identity matrix and skips the matrix mul as an optimisation
        static int  primCount = 50000;
        ImGui::Checkbox("Enable sorting", &enableSorting);
        ImGui::Checkbox("Use matrix stack", &useMatrix);
        ImGui::SliderInt("Prim Count", &primCount, 2, 100000);

        Im3d::PushEnableSorting(enableSorting);
        Im3d::BeginPoints();
        if (useMatrix)
        {
            Im3d::PushMatrix();
            for (int i = 0; i < primCount; ++i)
            {
                Im3d::Mat4 wm(1.0f);
                wm.setTranslation(RandVec3(-10.0f, 10.0f));
                Im3d::SetMatrix(wm);
                Im3d::Vertex(Im3d::Vec3(0.0f), RandFloat(2.0f, 16.0f), RandColor(0.0f, 1.0f));
            }
            Im3d::PopMatrix();
        }
        else
        {
            for (int i = 0; i < primCount; ++i)
            {
                Im3d::Vec3 t = RandVec3(-10.0f, 10.0f);
                Im3d::Vertex(t, RandFloat(2.0f, 16.0f), RandColor(0.0f, 1.0f));
            }
        }
        Im3d::End();
        Im3d::PopEnableSorting();

        ImGui::TreePop();
    }

    ImGui::End();
}

//TODO: Rewrite this manually using directx math stuff only. Avoid doing all these messy type casts
void MainGui::UpdateCamera(f32 deltaTime)
{
	////Simple way of handling this for the moment. Should move into input callbacks and the Camera class later on to keep things clean & organized

	//float kCamSpeed = 2.0f;
	//float kCamSpeedMul = 10.0f;
	//float kCamRotationMul = 10.0f;

	////Todo: Use proper conversion operators here or just eliminate im3d stuff altogether and us D3dMath stuff
	//Im3d::Vec4& m_camPos = (Im3d::Vec4&)camera_->camPosition;
	//Im3d::Vec4& m_camDir = (Im3d::Vec4&)camera_->camTarget;
	////m_camWorld = Im3d::LookAt(*(Im3d::Vec3*) & camera_->camPosition, m_camPos - m_camDir);
	//Im3d::Mat4 m_camWorld = Im3d::LookAt(m_camPos, (m_camPos) - (m_camDir));
	//camera_->UpdateViewMatrix();
	//Im3d::Mat4& m_camView = (Im3d::Mat4&)camera_->camView;
	////m_camView = Inverse(m_camWorld);
	////Im3d::Mat4 camProj = *(Im3d::Mat4*)(&camera_->camProjection); //Todo: Make conversion function for this. Fill out conversion operators in ext/im3d_config.h
	////Im3d::Mat4 camWorld = Inverse(*(Im3d::Mat4*)(&camera_->camView));


	//Im3d::Vec2 cursorPos = GetWindowRelativeCursor();
	//if (SystemWindowFocused())
	//{
	//	if (!ImGui::GetIO().WantCaptureKeyboard)
	//	{
	//		if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
	//		{
	//			kCamSpeed *= 10.0f;
	//		}
	//		if ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) == 0) // ctrl not pressed
	//		{
	//			if (GetAsyncKeyState(0x57) & 0x8000) // W (forward)
	//			{
	//				m_camPos = m_camPos - m_camWorld.getCol(2) * (deltaTime * kCamSpeed);
	//			}
	//			if (GetAsyncKeyState(0x41) & 0x8000) // A (left)
	//			{
	//				m_camPos = m_camPos - m_camWorld.getCol(0) * (deltaTime * kCamSpeed);
	//			}
	//			if (GetAsyncKeyState(0x53) & 0x8000) // S (backward)
	//			{
	//				m_camPos = m_camPos + m_camWorld.getCol(2) * (deltaTime * kCamSpeed);
	//			}
	//			if (GetAsyncKeyState(0x44) & 0x8000) // D (right)
	//			{
	//				m_camPos = m_camPos + m_camWorld.getCol(0) * (deltaTime * kCamSpeed);
	//			}
	//			if (GetAsyncKeyState(0x51) & 0x8000) // Q (down)
	//			{
	//				m_camPos = m_camPos - m_camWorld.getCol(1) * (deltaTime * kCamSpeed);
	//			}
	//			if (GetAsyncKeyState(0x45) & 0x8000) // D (up)
	//			{
	//				m_camPos = m_camPos + m_camWorld.getCol(1) * (deltaTime * kCamSpeed);
	//			}
	//		}
	//	}
	//	if (!ImGui::GetIO().WantCaptureMouse)
	//	{
	//		if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
	//		{
	//			Im3d::Vec2 cursorDelta = ((cursorPos - m_prevCursorPos) / Im3d::Vec2((f32)windowWidth_, (f32)windowHeight_)) * kCamRotationMul;
	//			m_camDir = Im3d::Rotation(Im3d::Vec3(0.0f, 1.0f, 0.0f), -cursorDelta.x) * m_camDir;
	//			m_camDir = Im3d::Rotation(m_camWorld.getCol(0), -cursorDelta.y) * m_camDir;
	//		}
	//	}
	//}
	////m_camView = Inverse(m_camWorld);
	//m_prevCursorPos = cursorPos;
}

Im3d::Vec2 MainGui::GetWindowRelativeCursor() const
{
	POINT p = {};
	GetCursorPos(&p);
	ScreenToClient(hwnd_, &p);
	return Im3d::Vec2((f32)p.x, (f32)p.y);
}

bool MainGui::SystemWindowFocused()
{
	return hwnd_ = GetFocus();
}
