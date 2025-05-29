#pragma once

#include <blend2d.h>
#include <cmath> // For fmod

namespace ColorUtils
{

    /**
     * @brief Applies a hue shift to a given BLRgba32 color.
     *
     * @param baseColor The original color.
     * @param hueShiftDegrees The amount to shift the hue, in degrees (0-360).
     *                        Positive values shift hue clockwise, negative counter-clockwise.
     * @return BLRgba32 The new color with the hue shifted.
     */
    BLRgba32 ShiftHue(BLRgba32 baseColor, float hueShiftDegrees);

    /**
     * @brief Generates a distinct color for a layer based on its index and a base color using hue rotation.
     *
     * @param layerIndex The index of the layer (0-based).
     * @param totalLayers The total number of layers (used to determine hue step if not provided explicitly).
     * @param baseColor The starting color from which hues will be shifted.
     * @param hueStepDegrees The fixed degrees to shift hue for each subsequent layer.
     *                       If 0, it will attempt to distribute hues across 360 degrees based on totalLayers.
     * @return BLRgba32 The calculated color for the layer.
     */
    BLRgba32 GenerateLayerColor(int layerIndex, int totalLayers, BLRgba32 baseColor, float hueStepDegrees = 30.0f);

    // HSV TO RGB
    void HSVtoRGB(float h, float s, float v, float &r, float &g, float &b);
    // RGB TO HSV
    void RGBtoHSV(float r, float g, float b, float &h, float &s, float &v);

} // namespace ColorUtils