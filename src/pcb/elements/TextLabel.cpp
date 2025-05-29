#include "pcb/elements/TextLabel.hpp"
#include "pcb/elements/Component.hpp" // For parentComponent context
#include <blend2d.h>
#include <sstream>
#include <cmath> // For M_PI, cos, sin

// Define M_PI if not already available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

BLRect TextLabel::getBoundingBox(const Component *parentComponent) const
{
    double world_x = x;
    double world_y = y;
    double current_rotation_deg = rotation; // TextLabel's own rotation

    if (parentComponent)
    {
        double comp_center_x = parentComponent->center_x;
        double comp_center_y = parentComponent->center_y;
        double comp_rot_rad = parentComponent->rotation * (M_PI / 180.0);
        double cos_comp = std::cos(comp_rot_rad);
        double sin_comp = std::sin(comp_rot_rad);

        // Rotate local text anchor relative to component origin
        double rotated_local_x = x * cos_comp - y * sin_comp;
        double rotated_local_y = x * sin_comp + y * cos_comp;

        // Translate to world space
        world_x = comp_center_x + rotated_local_x;
        world_y = comp_center_y + rotated_local_y;

        // Add component's rotation to text's own rotation
        current_rotation_deg += parentComponent->rotation;
    }

    // Normalize final rotation
    current_rotation_deg = fmod(current_rotation_deg, 360.0);
    if (current_rotation_deg < 0)
        current_rotation_deg += 360.0;

    // Very rough estimate of text extents based on font size and content length
    // This does NOT account for actual font metrics or character widths.
    double estimated_char_width = font_size * 0.6 * scale; // Rough aspect ratio assumption
    double text_width = text_content.length() * estimated_char_width;
    double text_height = font_size * scale;

    // Calculate AABB of the OBB defined by (world_x, world_y) as center, text_width, text_height, and current_rotation_deg
    if (text_width == 0 || text_height == 0)
    {
        return BLRect(world_x, world_y, 0, 0); // No dimensions
    }

    double w = text_width / 2.0;
    double h = text_height / 2.0;

    double angle_rad = current_rotation_deg * (M_PI / 180.0);
    double cos_a = std::cos(angle_rad);
    double sin_a = std::sin(angle_rad);

    // Coordinates of the 4 corners of the unrotated box centered at origin
    Vec2 corners[4] = {
        {-w, -h},
        {w, -h},
        {w, h},
        {-w, h}};

    double min_x = world_x, max_x = world_x, min_y = world_y, max_y = world_y;
    bool first = true;

    for (int i = 0; i < 4; ++i)
    {
        // Rotate corner
        double rx = corners[i].x * cos_a - corners[i].y * sin_a;
        double ry = corners[i].x * sin_a + corners[i].y * cos_a;
        // Translate to world position
        rx += world_x;
        ry += world_y;
        if (first)
        {
            min_x = max_x = rx;
            min_y = max_y = ry;
            first = false;
        }
        else
        {
            if (rx < min_x)
                min_x = rx;
            if (rx > max_x)
                max_x = rx;
            if (ry < min_y)
                min_y = ry;
            if (ry > max_y)
                max_y = ry;
        }
    }
    return BLRect(min_x, min_y, max_x - min_x, max_y - min_y);
}

bool TextLabel::isHit(const Vec2 &worldMousePos, float tolerance, const Component *parentComponent) const
{
    return false;
    // // For text, hit detection against the AABB is often sufficient for UI purposes.
    // // More precise would involve checking against OBB or even glyphs.
    // BLRect aabb = getBoundingBox(parentComponent);
    // // Add tolerance to the AABB
    // BLBox hit_box(aabb.x - tolerance, aabb.y - tolerance, aabb.w + 2 * tolerance, aabb.h + 2 * tolerance);
    // return hit_box.contains(worldMousePos.x, worldMousePos.y);
}

std::string TextLabel::getInfo(const Component *parentComponent) const
{
    std::ostringstream oss;
    oss << "Text Label\n";
    oss << "Content: \"" << text_content << "\"\n";
    oss << "Layer: " << m_layerId << "\n";

    double world_x_anchor = x;
    double world_y_anchor = y;
    double final_rotation_deg = rotation;

    if (parentComponent)
    {
        oss << "Parent: " << parentComponent->reference_designator << "\n";
        oss << "Local Anchor: (" << x << ", " << y << ")\n";

        double comp_center_x = parentComponent->center_x;
        double comp_center_y = parentComponent->center_y;
        double comp_rot_rad = parentComponent->rotation * (M_PI / 180.0);
        double cos_comp = std::cos(comp_rot_rad);
        double sin_comp = std::sin(comp_rot_rad);

        double rotated_local_x = x * cos_comp - y * sin_comp;
        double rotated_local_y = x * sin_comp + y * cos_comp;

        world_x_anchor = comp_center_x + rotated_local_x;
        world_y_anchor = comp_center_y + rotated_local_y;
        final_rotation_deg += parentComponent->rotation;
    }
    final_rotation_deg = fmod(final_rotation_deg, 360.0);
    if (final_rotation_deg < 0)
        final_rotation_deg += 360.0;

    oss << "World Anchor: (" << world_x_anchor << ", " << world_y_anchor << ")\n";
    oss << "Font Size: " << font_size << ", Scale: " << scale << ", Final Rotation: " << final_rotation_deg << " deg";
    if (m_netId != -1)
    { // Though unusual for text labels
        oss << "\nNet ID: " << m_netId;
    }
    return oss.str();
}

void TextLabel::translate(double dx, double dy)
{
    // If this TextLabel is standalone (not part of a component), its x, y are world coordinates.
    // If it's part of a component, its x, y are local to the component's center.
    // Component::translate does NOT call translate on its child TextLabels, because the component's
    // center moves, and the labels' local x,y should remain relative to that new center.
    // This translate method is for when Board::NormalizeCoordinatesAndGetCenterOffset directly
    // processes a standalone TextLabel from m_elementsByLayer.
    x += dx;
    y += dy;
    // Font size, scale, rotation, content are not affected by translation.
}