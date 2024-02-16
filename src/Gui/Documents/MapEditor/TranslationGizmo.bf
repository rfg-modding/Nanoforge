#pragma warning disable 168
using Common;
using System;
using Nanoforge.Render;
using Nanoforge.Rfg;
using Nanoforge.App;
using System.Diagnostics;
using Common.Math;
using Nanoforge.Misc;

namespace Nanoforge.Gui.Documents.MapEditor;

//Mouse controlled gizmo for changing object positions.
//At the moment its only meant for use with MapEditorDocument and ZoneObject. But there's no reason it couldn't be extended for use with mesh viewers when those are added back in.
public class TranslationGizmo
{
    public bool Hovering => HoveringXAxis || HoveringYAxis || HoveringZAxis;
    public bool Grabbing => GrabbingXAxis || GrabbingYAxis || GrabbingZAxis;

    public bool HoveringXAxis { get; private set; } = false;
    public bool GrabbingXAxis { get; private set; } = false;
    public bool HoveringYAxis { get; private set; } = false;
    public bool GrabbingYAxis { get; private set; } = false;
    public bool HoveringZAxis { get; private set; } = false;
    public bool GrabbingZAxis { get; private set; } = false;

    public bool ControlsEnabled = true;
    public bool DrawColliders = false;
    public f32 AxesColliderBoxWidth = 5.0f;
    public f32 AxesLength = 30.0f;
    public f32 AxesColliderDistanceFromCenter = 5.0f;

    public f32 Sensitivity = 100.0f;

    public Vec3? _lastFrameMouseRay = .Zero;

    //Track how far the mouse has moved along a plane parallel to the axis being translated along.
    //For the x and z axes thats the xz plane. For the y axis its either xy or zy depending on which direction the camera is pointing.
    private Plane? _grabPlane = null;

    public void Update(App app, MapEditorDocument editor, ZoneObject obj)
    {
        Ray? MouseRay = editor.MouseRay;

        Sensitivity = 1.0f;//0.3f;

        //Translation gizmos
        HoveringXAxis = false;
        HoveringYAxis = false;
        HoveringZAxis = false;
        Mat3 orient = obj.Orient.Enabled ? obj.Orient.Value : .Identity;
        Vec3 center = obj.Position;

        BoundingBox xAxisBBox = GetXAxisBBox(obj.Position);
        BoundingBox yAxisBBox = GetYAxisBBox(obj.Position);
        BoundingBox zAxisBBox = GetZAxisBBox(obj.Position);
        if (DrawColliders)
        {
            editor.Scene.DrawBox(xAxisBBox.Min, xAxisBBox.Max, .(1.0f, 1.0f, 0.0f, 1.0f));
            editor.Scene.DrawBox(yAxisBBox.Min, yAxisBBox.Max, .(1.0f, 1.0f, 0.0f, 1.0f));
            editor.Scene.DrawBox(zAxisBBox.Min, zAxisBBox.Max, .(1.0f, 1.0f, 0.0f, 1.0f));
        }

        Vec3 xhit = .Zero;
        Vec3 yhit = .Zero;
        Vec3 zhit = .Zero;
        if (MouseRay.HasValue && xAxisBBox.IntersectsLine(MouseRay.Value.Start, MouseRay.Value.End, ref xhit))
        {
            HoveringXAxis = true;
        }
        if (MouseRay.HasValue && yAxisBBox.IntersectsLine(MouseRay.Value.Start, MouseRay.Value.End, ref yhit))
        {
            HoveringYAxis = true;
        }
        if (MouseRay.HasValue && zAxisBBox.IntersectsLine(MouseRay.Value.Start, MouseRay.Value.End, ref zhit))
        {
            HoveringZAxis = true;
        }

        //TODO: Figure out a way to simplify and generalize this logic across all axes
        Input input = app.GetResource<Input>();
        if (HoveringXAxis && input.MouseButtonPressed(.Left) && !Grabbing)
        {
            GrabbingXAxis = true;
            Vec3 planeNormal = .(0.0f, 1.0f, 0.0f);
            _grabPlane = .(obj.Position, planeNormal);
        }
        else if (!input.MouseButtonDown(.Left))
        {
            GrabbingXAxis = false;
        }

        if (HoveringYAxis && input.MouseButtonPressed(.Left) && !Grabbing)
        {
            GrabbingYAxis = true;
            Vec3 planeNormal = .(1.0f, 0.0f, 0.0f);
            _grabPlane = .(obj.Position, planeNormal);
        }
        else if (!input.MouseButtonDown(.Left))
        {
            GrabbingYAxis = false;
        }

        if (HoveringZAxis && input.MouseButtonPressed(.Left) && !Grabbing)
        {
            GrabbingZAxis = true;
            Vec3 planeNormal = .(0.0f, 1.0f, 0.0f);
            _grabPlane = .(obj.Position, planeNormal);
        }
        else if (!input.MouseButtonDown(.Left))
        {
            GrabbingZAxis = false;
        }

        //TODO: Some other improvements to make:
        //    - Loop the mouse around the edge of the viewport when using gizmos. Just like Blender does.
        //    - Draw lines extending out of the axis being edited. Have a dot indicating the starting position. Draw it as an overlay.
        //    - Flip the gizmo when viewing an axis from the negative side. Ideally indicate its the negative. Maybe with overlay text or color difference.

        //Check if the mouse is intersecting the grab plane so we can calculate the distance the mouse has moved on that plane
        Vec3 lastFramePlaneHit = .Zero;
        Vec3 thisFramePlaneHit = .Zero;
        if (ControlsEnabled && _lastFrameMouseRay.HasValue && MouseRay.HasValue && _grabPlane.HasValue
        	                                                 && _grabPlane.Value.IntersectsLine(editor.Scene.Camera.Position, _lastFrameMouseRay.Value, ref lastFramePlaneHit)
        	                                                 && _grabPlane.Value.IntersectsLine(MouseRay.Value.Start, MouseRay.Value.End, ref thisFramePlaneHit))
        {
            if (GrabbingXAxis)
            {
                Vec3 mouseDelta = (lastFramePlaneHit - thisFramePlaneHit);
                Vec3 projMouseOnX = mouseDelta.Projection(.(1.0f, 0.0f, 0.0f)); //Project mouse movement vector onto the x axis to get how much the mouse has moved in the x axis
                MoveObject(editor, obj, -1.0f * projMouseOnX * Sensitivity);
            }
            if (GrabbingYAxis)
            {
                Vec3 mouseDelta = (lastFramePlaneHit - thisFramePlaneHit);
                Vec3 projMouseOnY = mouseDelta.Projection(.(0.0f, 1.0f, 0.0f));
                MoveObject(editor, obj, -1.0f * projMouseOnY * Sensitivity);
            }
            if (GrabbingZAxis)
            {
                Vec3 mouseDelta = (lastFramePlaneHit - thisFramePlaneHit);
                Vec3 projMouseOnZ = mouseDelta.Projection(.(0.0f, 0.0f, 1.0f));
                MoveObject(editor, obj, -1.0f * projMouseOnZ * Sensitivity);
            }
        }

        //Track last frames mouse ray so we can measure the distance its moved each frame
        if (Grabbing && MouseRay.HasValue)
        {
            _lastFrameMouseRay = (MouseRay.Value.End);
        }
        if (!Grabbing)
        {
            _grabPlane = null;
        }

        f32 notHoveredBrightness = 0.6f;
        f32 hoveredBrightness = 1.0f;
        editor.Scene.DrawOverlayArrow(center, center + (Vec3(1.0f, 0.0f, 0.0f) * AxesLength), color: .((HoveringXAxis || GrabbingXAxis) ? hoveredBrightness : notHoveredBrightness, 0.0f, 0.0f, 1.0f), headLength: 5.0f, lineWidth: 3.0f, headWidth: 2.5f); //X
        editor.Scene.DrawOverlayArrow(center, center + (Vec3(0.0f, 1.0f, 0.0f) * AxesLength), color: .(0.0f, (HoveringYAxis || GrabbingYAxis) ? hoveredBrightness : notHoveredBrightness, 0.0f, 1.0f), headLength: 5.0f, lineWidth: 3.0f, headWidth: 2.5f); //Y
        editor.Scene.DrawOverlayArrow(center, center + (Vec3(0.0f, 0.0f, 1.0f) * AxesLength), color: .(0.0f, 0.0f, (HoveringZAxis || GrabbingZAxis) ? hoveredBrightness : notHoveredBrightness, 1.0f), headLength: 5.0f, lineWidth: 3.0f, headWidth: 2.5f); //Z

        if (!Grabbing)
        {
            _lastFrameMouseRay = null;
        }
    }

    //TODO: Move this logic into ZoneObject.Position or maybe make a new function ZoneObject.SetPosition() so extra args like 'bool autoMoveChildren' can be passed along (or put AutoMoveChildren in a global CVar)
    private void MoveObject(MapEditorDocument editor, ZoneObject obj, Vec3 delta)
    {
        obj.Position += delta;
        obj.BBox += delta;

        //Update mesh position
        if (obj.RenderObject != null)
        {
            obj.RenderObject.Position = obj.Position;
        }

        if (editor.AutoMoveChildren)
        {
            MoveObjectChildrenRecursive(obj, delta);
        }

        editor.UnsavedChanges = true;
    }

    private void MoveObjectChildrenRecursive(ZoneObject obj, Vec3 delta)
    {
        for (ZoneObject child in obj.Children)
        {
            MoveObjectChildrenRecursive(child, delta);
            child.Position += delta;
            child.BBox += delta;
        }    
    }

    private BoundingBox GetXAxisBBox(Vec3 center)
    {
        f32 axesBoxHalfWidth = AxesColliderBoxWidth / 2.0f;
        Vec3 min = .Zero;
        min.x = center.x + AxesColliderDistanceFromCenter;
        min.y = center.y - axesBoxHalfWidth;
        min.z = center.z - axesBoxHalfWidth;
        Vec3 max = .Zero;
        max.x = center.x + AxesLength;
        max.y = center.y + axesBoxHalfWidth;
        max.z = center.z + axesBoxHalfWidth;

        return .(min, max);
    }

    private BoundingBox GetYAxisBBox(Vec3 center)
    {
        f32 axesBoxHalfWidth = AxesColliderBoxWidth / 2.0f;
        Vec3 min = .Zero;
        min.x = center.x - axesBoxHalfWidth;
        min.y = center.y + AxesColliderDistanceFromCenter;
        min.z = center.z - axesBoxHalfWidth;
        Vec3 max = .Zero;
        max.x = center.x + axesBoxHalfWidth;
        max.z = center.z + axesBoxHalfWidth;
        max.y = center.y + AxesLength;

        return .(min, max);
    }

    private BoundingBox GetZAxisBBox(Vec3 center)
    {
        f32 axesBoxHalfWidth = AxesColliderBoxWidth / 2.0f;
        Vec3 min = .Zero;
        min.x = center.x - axesBoxHalfWidth;
        min.y = center.y - axesBoxHalfWidth;
        min.z = center.z + AxesColliderDistanceFromCenter;
        Vec3 max = .Zero;
        max.x = center.x + axesBoxHalfWidth;
        max.y = center.y + axesBoxHalfWidth;
        max.z = center.z + AxesLength;

        return .(min, max);
    }
}