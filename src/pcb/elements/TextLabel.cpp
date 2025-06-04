#include "TextLabel.hpp"

#include <cmath>
#include <math.h>
#include <sstream>

#include <blend2d.h>

#include "Component.hpp"  // For parentComponent context

#include "utils/Constants.hpp"  // For kPi

BLRect TextLabel::GetBoundingBox(const Component* parent_component) const
{
    double world_x = coords.x_ax;
    double world_y = coords.y_ax;
    double current_rotation_deg = rotation;  // TextLabel's own rotation

    if (parent_component != nullptr) {
        double const comp_center_x = parent_component->center_x;
        double const comp_center_y = parent_component->center_y;
        double comp_rot_rad = parent_component->rotation * (kPi / 180.0);
        double const cos_comp = std::cos(comp_rot_rad);
        double const sin_comp = std::sin(comp_rot_rad);

        // Rotate local text anchor relative to component origin
        double rotated_local_x = coords.x_ax * cos_comp - coords.y_ax * sin_comp;
        double rotated_local_y = coords.x_ax * sin_comp + coords.y_ax * cos_comp;

        // Translate to world space
        world_x = comp_center_x + rotated_local_x;
        world_y = comp_center_y + rotated_local_y;

        // Add component's rotation to text's own rotation
        current_rotation_deg += parent_component->rotation;
    }

    // Normalize final rotation
    current_rotation_deg = fmod(current_rotation_deg, 360.0);
    if (current_rotation_deg < 0) {
        current_rotation_deg += 360.0;
    }

    // Very rough estimate of text extents based on font size and content length
    // This does NOT account for actual font metrics or character widths.
    double estimated_char_width = font_size * 0.6 * scale;  // Rough aspect ratio assumption
    double text_width = text_content.length() * estimated_char_width;
    double text_height = font_size * scale;

    // Calculate AABB of the OBB defined by (world_x, world_y) as center, text_width, text_height, and current_rotation_deg
    if (text_width == 0 || text_height == 0) {
        return BLRect(world_x, world_y, 0, 0);  // No dimensions
    }

    double w = text_width / 2.0;
    double h = text_height / 2.0;

    double angle_rad = current_rotation_deg * (kPi / 180.0);
    double cos_a = std::cos(angle_rad);
    double sin_a = std::sin(angle_rad);

    // Coordinates of the 4 corners of the unrotated box centered at origin
    Vec2 corners[4] = {{-w, -h}, {w, -h}, {w, h}, {-w, h}};

    double min_x = world_x;
    double max_x = world_x;
    double min_y = world_y;
    double max_y = world_y;
    bool first = true;

    for (int i = 0; i < 4; ++i) {
        // Rotate corner
        double rot_x = (corners[i].x_ax * cos_a) - (corners[i].y_ax * sin_a);
        double rot_y = (corners[i].x_ax * sin_a) + (corners[i].y_ax * cos_a);
        // Translate to world position
        rot_x += world_x;
        rot_y += world_y;
        if (first) {
            min_x = max_x = rot_x;
            min_y = max_y = rot_y;
            first = false;
        } else {
            if (rot_x < min_x) {
                min_x = rot_x;
            }
            if (rot_x > max_x) {
                max_x = rot_x;
            }
            if (rot_y < min_y) {
                min_y = rot_y;
            }
            if (rot_y > max_y) {
                max_y = rot_y;
            }
        }
    }
    return BLRect(min_x, min_y, max_x - min_x, max_y - min_y);
}

bool TextLabel::IsHit(const Vec2& world_mouse_pos, float /*tolerance*/, const Component* parent_component) const
{
    return false;
    // // For text, hit detection against the AABB is often sufficient for UI purposes.
    // // More precise would involve checking against OBB or even glyphs.
    // BLRect aabb = getBoundingBox(parent_component);
    // // Add tolerance to the AABB
    // BLBox hit_box(aabb.x - tolerance, aabb.y - tolerance, aabb.w + 2 * tolerance, aabb.h + 2 * tolerance);
    // return hit_box.contains(world_mouse_pos.x, world_mouse_pos.y);
}

std::string TextLabel::GetInfo(const Component* parent_component) const
{
    std::ostringstream oss;
    oss << "Text Label\n";
    oss << "Content: \"" << text_content << "\"\n";
    oss << "Layer: " << GetLayerId() << "\n";

    double world_x_anchor = NAN;
    world_x_anchor = coords.x_ax;
    double world_y_anchor = NAN;
    world_y_anchor = coords.y_ax;
    double final_rotation_deg = rotation;

    if (parent_component != nullptr) {
        oss << "Parent: " << parent_component->reference_designator << "\n";
        oss << "Local Anchor: (" << coords.x_ax << ", " << coords.y_ax << ")\n";

        double const comp_center_x = parent_component->center_x;
        double const comp_center_y = parent_component->center_y;
        double comp_rot_rad = parent_component->rotation * (kPi / 180.0);
        double const cos_comp = std::cos(comp_rot_rad);
        double const sin_comp = std::sin(comp_rot_rad);

        double rotated_local_x = coords.x_ax * cos_comp - coords.y_ax * sin_comp;
        double rotated_local_y = coords.x_ax * sin_comp + coords.y_ax * cos_comp;

        world_x_anchor = comp_center_x + rotated_local_x;
        world_y_anchor = comp_center_y + rotated_local_y;
        final_rotation_deg += parent_component->rotation;
    }
    final_rotation_deg = fmod(final_rotation_deg, 360.0);
    if (final_rotation_deg < 0) {
        final_rotation_deg += 360.0;
    }

    oss << "World Anchor: (" << world_x_anchor << ", " << world_y_anchor << ")\n";
    oss << "Font Size: " << font_size << ", Scale: " << scale << ", Final Rotation: " << final_rotation_deg << " deg";
    if (GetNetId() != -1) {  // Though unusual for text labels
        oss << "\nNet ID: " << GetNetId();
    }
    return oss.str();
}

void TextLabel::Translate(double dist_x, double dist_y)
{
    // If this TextLabel is standalone (not part of a component), its x, y are world coordinates.
    // If it's part of a component, its x, y are local to the component's center.
    // Component::translate does NOT call translate on its child TextLabels, because the component's
    // center moves, and the labels' local x,y should remain relative to that new center.
    // This translate method is for when Board::NormalizeCoordinatesAndGetCenterOffset directly
    // processes a standalone TextLabel from m_elementsByLayer.
    coords.x_ax += dist_x;
    coords.y_ax += dist_y;
    // Font size, scale, rotation, content are not affected by translation.
}
