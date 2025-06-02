#include "pcb/elements/Component.hpp"

#include <algorithm>  // For std::min, std::max
#include <cmath>      // For cos, sin, fmod
#include <sstream>    // For getInfo

#include "utils/Constants.hpp"      // For kPi
#include "utils/GeometryUtils.hpp"  // Might be useful for future, more complex component shapes
#include "utils/Vec2.hpp"           // For Vec2, though indirectly used via BLRect/BLBox

// Other includes for Component methods if necessary

// void Component::translate(double dx, double dy)
// {
//     // Translate the component's own anchor point (center)
//     center_x += dx;
//     center_y += dy;

//     // Pins and TextLabels within a component have coordinates (x_coord, y_coord and x, y respectively)
//     // that are generally understood to be *local offsets* relative to the component's center_x, center_y.
//     // When the component itself is translated, these local offsets should remain unchanged.
//     // Their world positions change because the component's center (their reference point) has changed.
//     // Therefore, we do NOT call pin.translate() or label.translate() here.

//     // for (auto &pin : pins)
//     // {
//     //     // pin.translate(dx, dy); // NO! Pin coordinates are local to component center.
//     // }

//     // for (auto &label : text_labels)
//     // {
//     //     // label.translate(dx, dy); // NO! Label coordinates are local to component center.
//     // }

//     // Translate owned LineSegments (these are structs with absolute-like points, or points that need to move with component)
//     // If LineSegment points are also local offsets, they wouldn't be translated here either.
//     // Assuming LineSegment start/end points are effectively world-offsets that need to move with the component,
//     // or rather, they were defined relative to an original component origin that is now moving.
//     // Let's assume graphical_elements are defined in the same space as pins/labels (relative to component center).
//     // So, their coordinates also don't change relative to the component center.
//     // IF graphical_elements were defined in absolute world coordinates anchored to the component, they would need translation.
//     // For now, assuming they are like pins/labels: local offsets.
//     for (auto &segment : graphical_elements) // If these are local offsets:
//     {
//         // segment.start.x += dx; // No, if local
//         // segment.start.y += dy; // No, if local
//         // segment.end.x += dx;   // No, if local
//         // segment.end.y += dy;   // No, if local
//         // However, if graphical_elements coordinates were meant to be translated *with* the component,
//         // (e.g. they define the visual shape at the component's location), then they *do* need translation.
//         // This depends on how they are defined and used. For board normalization, if these are part
//         // of what defines the component's visual extent that needs to be centered, they should be translated.
//         // Let's assume they ARE part of the visual representation that moves with the component's center.
//         segment.start.x += dx;
//         segment.start.y += dy;
//         segment.end.x += dx;
//         segment.end.y += dy;
//     }

//     // Adjust pre-calculated bounding box coordinates (pin_bbox_...).
//     // These are likely absolute world coordinates or derived in a way that they need to follow the component's movement.
//     pin_bbox_min_x += dx;
//     pin_bbox_max_x += dx;
//     pin_bbox_min_y += dy;
//     pin_bbox_max_y += dy;

//     // Note: Component's width and height are dimensions, not coordinates, so they don't change with translation.
// }

// BLRect Component::getBoundingBox() const
// {
//     // If width or height is zero, component has no area, return a point-like box at its center.
//     if (width == 0 || height == 0)
//     {
//         return BLRect(center_x, center_y, 0, 0);
//     }

//     // Half dimensions for easier calculation of corner coordinates
//     double half_w = width / 2.0;
//     double half_h = height / 2.0;

//     // Convert rotation to radians
//     double angle_rad = rotation * (kPi / 180.0);
//     double cos_rot = std::cos(angle_rad);
//     double sin_rot = std::sin(angle_rad);

//     // Coordinates of the 4 corners of the component's unrotated bounding box, centered at origin (0,0)
//     Vec2 local_corners[4] = {
//         {-half_w, -half_h}, {half_w, -half_h}, {half_w, half_h}, {-half_w, half_h}};

//     double min_x_world = center_x, max_x_world = center_x;
//     double min_y_world = center_y, max_y_world = center_y;
//     bool first = true;

//     for (int i = 0; i < 4; ++i)
//     {
//         // Rotate corner relative to component's local origin (which is (0,0) for these local_corners)
//         double rotated_x = local_corners[i].x * cos_rot - local_corners[i].y * sin_rot;
//         double rotated_y = local_corners[i].x * sin_rot + local_corners[i].y * cos_rot;

//         // Translate rotated corner to the component's world center
//         double corner_world_x = rotated_x + center_x;
//         double corner_world_y = rotated_y + center_y;

//         if (first)
//         {
//             min_x_world = max_x_world = corner_world_x;
//             min_y_world = max_y_world = corner_world_y;
//             first = false;
//         }
//         else
//         {
//             min_x_world = std::min(min_x_world, corner_world_x);
//             max_x_world = std::max(max_x_world, corner_world_x);
//             min_y_world = std::min(min_y_world, corner_world_y);
//             max_y_world = std::max(max_y_world, corner_world_y);
//         }
//     }
//     return BLRect(min_x_world, min_y_world, max_x_world - min_x_world, max_y_world - min_y_world);
// }

// bool Component::isHit(const Vec2 &worldMousePos, float tolerance) const
// {
//     BLRect aabb = getBoundingBox();

//     // Create a BLBox from the BLRect for contains check (BLRect itself doesn't have contains)
//     // BLBox constructor is (x0, y0, x1, y1)
//     BLBox hit_box(aabb.x - tolerance,
//                   aabb.y - tolerance,
//                   aabb.x + aabb.w + tolerance,
//                   aabb.y + aabb.h + tolerance);

//     return hit_box.contains(worldMousePos.x, worldMousePos.y);
// }

// std::string Component::getInfo() const
// {
//     std::ostringstream oss;
//     oss << "Component: " << reference_designator << " (\"" << value << "\")\n";
//     oss << "Footprint: " << footprint_name << "\n";
//     oss << "Center: (" << center_x << ", " << center_y << ")\n";
//     oss << "Rotation: " << rotation << " deg\n";
//     oss << "Dimensions (W,H): (" << width << ", " << height << ")\n";
//     oss << "Layer: " << layer << ", Side: " << (side == MountingSide::Top ? "Top" : "Bottom") << "\n";
//     oss << "Type: ";
//     switch (type)
//     {
//     case ComponentElementType::SMD:
//         oss << "SMD";
//         break;
//     case ComponentElementType::ThroughHole:
//         oss << "ThroughHole";
//         break;
//     case ComponentElementType::Other:
//         oss << "Other";
//         break;
//     }
//     oss << "\nPins: " << pins.size();
//     // Could add more info like pin bounding box etc.
//     return oss.str();
// }

// Implement other Component methods here if any
// e.g., addPin, addTextLabel, addGraphicalElement, recalculateBoundingBox, etc.
