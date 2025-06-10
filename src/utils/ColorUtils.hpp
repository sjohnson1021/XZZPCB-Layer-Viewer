#pragma once

#include <blend2d.h>

namespace color_utils
{
struct HsvColor {
    float hue;       // Hue: 0-360 degrees
    float sat;       // Saturation: 0-1
    float val;       // Value: 0-1
    uint32_t alpha;  // Alpha: 0-255 (preserved from original RGBA)
};

/**
 * @brief Applies a hue shift to a given BLRgba32 color.
 *
 * @param baseColor The original color.
 * @param hueShiftDegrees The amount to shift the hue, in degrees (0-360).
 *                        Positive values shift hue clockwise, negative counter-clockwise.
 * @return BLRgba32 The new color with the hue shifted.
 */
extern BLRgba32 ShiftHue(BLRgba32 base_color, float hue_shift_degrees);

/**
 * @brief Generates a distinct color for a layer based on its index and a base color using hue rotation.
 *
 * @param layer_index The index of the layer (0-based).
 * @param total_layers The total number of layers (used to determine hue step if not provided explicitly).
 * @param base_color The starting color from which hues will be shifted.
 * @param hue_step_degrees The fixed degrees to shift hue for each subsequent layer.
 *                       If 0, it will attempt to distribute hues across 360 degrees based on totalLayers.
 * @return BLRgba32 The calculated color for the layer.
 */
extern BLRgba32 GenerateLayerColor(int layer_index, int total_layers, BLRgba32 base_color, float hue_step_degrees = 30.0F);

// HSV TO BLRgba32
extern void HsvToBLRgba32(const HsvColor& hsv_color, BLRgba32& rgba);
// BLRgba32 TO HSV
extern void BLRgba32ToHsv(const BLRgba32& rgba, HsvColor& hsv);

// // HSV TO RGB
// extern void HSVtoRGB(float h, float s, float v, float& r, float& g, float& b);
// // RGB TO HSV
// extern void RGBtoHSV(float r, float g, float b, float& h, float& s, float& v);


}  // namespace color_utils
