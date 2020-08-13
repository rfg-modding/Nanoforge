#pragma once
#include "gui/GuiState.h"
#include <im3d.h>

f32 ScaleTextSizeToDistance(f32 minSize, f32 maxSize, const Vec3& textPos, Camera* camera);

void ZoneRender_Update(GuiState* state)
{
    Im3d::PushDrawState();
    Im3d::SetSize(state->BoundingBoxThickness);
    Im3d::SetColor(Im3d::Color(state->BoundingBoxColor.x, state->BoundingBoxColor.y, state->BoundingBoxColor.z, state->BoundingBoxColor.w));

    //Draw bounding boxes
    for (const auto& zone : state->ZoneManager->ZoneFiles)
    {
        if (!zone.RenderBoundingBoxes)
            continue;

        for (const auto& object : zone.Zone.Objects)
        {
            auto objectClass = state->ZoneManager->GetObjectClass(object.ClassnameHash);
            if (!objectClass.Show)
                continue;
            //if (objectClass.Hash == 1794022917)
            if (objectClass.ShowLabel)
            {
                //Todo: Fix Vec3 operator- and use it here
                Vec3 position = Vec3{ object.Bmin.x + (object.Bmax.x - object.Bmin.x) / 2.0f, object.Bmin.y + (object.Bmax.y - object.Bmin.y) / 2.0f, object.Bmin.z + (object.Bmax.z - object.Bmin.z) / 2.0f };
                //Todo: Make conversion operators to simplify this
                f32 size = ScaleTextSizeToDistance(0.0f, state->LabelTextSize, position, state->Camera);
                Im3d::Text(Im3d::Vec3(position.x, position.y, position.z), size,
                    Im3d::Color(state->LabelTextColor.x, state->LabelTextColor.y, state->LabelTextColor.z, state->LabelTextColor.w),
                    Im3d::TextFlags_Default, (string(objectClass.LabelIcon) + objectClass.Name.c_str()).c_str());
            }

            //Todo: Make conversion operators to simplify this
            Im3d::SetColor(Im3d::Color(objectClass.Color.x, objectClass.Color.y, objectClass.Color.z, objectClass.Color.w));
            Im3d::DrawAlignedBox(Im3d::Vec3(object.Bmin.x, object.Bmin.y, object.Bmin.z), Im3d::Vec3(object.Bmax.x, object.Bmax.y, object.Bmax.z));

            //Todo: Could speed this up by giving each zone object a ptr to their hierarchical node, or just using the nodes for everything
            //Todo: Optionally have a flat node list separate from the actual zone data so RfgTools++ doesn't have things only used by this tool
            //Draw object connection lines
            if (state->DrawParentConnections)
            {
                if (object.Parent != InvalidZoneIndex) //Todo: Make invalid object handle constant
                {
                    //Todo: See if necessary to also search for parents in other zones
                    for (const auto& object2 : zone.Zone.Objects)
                    {
                        if (object2.Handle == object.Parent)
                        {
                            Im3d::Color parentArrowColor(0.355f, 0.0f, 1.0f, 1.0f);
                            Im3d::SetColor(parentArrowColor);
                            Vec3 position1 = Vec3{ object.Bmin.x + (object.Bmax.x - object.Bmin.x) / 2.0f, object.Bmin.y + (object.Bmax.y - object.Bmin.y) / 2.0f, object.Bmin.z + (object.Bmax.z - object.Bmin.z) / 2.0f };
                            Vec3 position2 = Vec3{ object2.Bmin.x + (object2.Bmax.x - object2.Bmin.x) / 2.0f, object2.Bmin.y + (object2.Bmax.y - object2.Bmin.y) / 2.0f, object2.Bmin.z + (object2.Bmax.z - object2.Bmin.z) / 2.0f };
                            Im3d::DrawArrow(Im3d::Vec3(position1.x, position1.y, position1.z), Im3d::Vec3(position2.x, position2.y, position2.z), 1.0f, 10.0f);
                            //Im3d::DrawLine(Im3d::Vec3(position1.x, position1.y, position1.z), Im3d::Vec3(position2.x, position2.y, position2.z), 1.0f, color)
                        }
                    }
                }
            }
        }
    }

    //Draw 0.0 height grid
    Im3d::SetAlpha(1.0f);
    Im3d::SetSize(1.0f);
    Im3d::BeginLines();

    //Update grid origin if it should follow the camera. Don't change y, just x & z coords
    if (state->GridFollowCamera)
        state->GridOrigin = Im3d::Vec3(state->Camera->Position().m128_f32[0], state->GridOrigin.y, state->Camera->Position().m128_f32[2]);

    if (state->DrawGrid)
    {
        const float gridHalf = (float)state->GridSize * 0.5f;
        for (int x = 0; x <= state->GridSize; x += state->GridSpacing)
        {
            Im3d::Vertex(state->GridOrigin.x + -gridHalf, state->GridOrigin.y + 0.0f, state->GridOrigin.z + (float)x - gridHalf, Im3d::Color(0.0f, 0.0f, 0.0f));
            Im3d::Vertex(state->GridOrigin.x + gridHalf, state->GridOrigin.y + 0.0f, state->GridOrigin.z + (float)x - gridHalf, Im3d::Color(1.0f, 1.0f, 1.0f));
        }
        for (int z = 0; z <= state->GridSize; z += state->GridSpacing)
        {
            Im3d::Vertex(state->GridOrigin.x + (float)z - gridHalf, state->GridOrigin.y + 0.0f, state->GridOrigin.z + -gridHalf, Im3d::Color(0.0f, 0.0f, 0.0f));
            Im3d::Vertex(state->GridOrigin.x + (float)z - gridHalf, state->GridOrigin.y + 0.0f, state->GridOrigin.z + gridHalf, Im3d::Color(1.0f, 1.0f, 1.0f));
        }
    }
    Im3d::End();

    Im3d::PopDrawState();
}

f32 ScaleTextSizeToDistance(f32 minSize, f32 maxSize, const Vec3& textPos, Camera* camera)
{
    const f32 minSizeDistance = 1000.0f;
    const f32 maxSizeDistance = 10.0f;
    f32 distance = textPos.Distance({ camera->Position().m128_f32[0], camera->Position().m128_f32[1], camera->Position().m128_f32[2] });
    if (distance < maxSizeDistance)
        return maxSize;

    //Lerp result from distance
    f32 lerp = ((minSize * (maxSizeDistance - distance)) + (maxSize * (distance - minSizeDistance))) / (maxSizeDistance - minSizeDistance);
    return lerp < 0.0f ? 0.0f : lerp;
}