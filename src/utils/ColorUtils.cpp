#include "utils/ColorUtils.hpp"
#include <algorithm> // For std::min, std::max
// cmath is already included via ColorUtils.hpp for std::fmod, std::fabs, std::round

namespace { // Anonymous namespace for internal helpers

struct HsvColor {
    float h;    // Hue: 0-360 degrees
    float s;    // Saturation: 0-1
    float v;    // Value: 0-1
    uint32_t a; // Alpha: 0-255 (preserved from original RGBA)
};

void BLRgba32ToHsvInternal(const BLRgba32& rgba, HsvColor& hsv) {
    float r_ = static_cast<float>(rgba.r()) / 255.0f;
    float g_ = static_cast<float>(rgba.g()) / 255.0f;
    float b_ = static_cast<float>(rgba.b()) / 255.0f;
    hsv.a = rgba.a(); // Preserve alpha

    float cmax = std::max({r_, g_, b_});
    float cmin = std::min({r_, g_, b_});
    float delta = cmax - cmin;

    // Hue calculation
    if (delta == 0.0f) {
        hsv.h = 0.0f; // Achromatic, hue is undefined, conventionally 0
    } else if (cmax == r_) {
        hsv.h = 60.0f * std::fmod(((g_ - b_) / delta), 6.0f);
    } else if (cmax == g_) {
        hsv.h = 60.0f * (((b_ - r_) / delta) + 2.0f);
    } else { // cmax == b_
        hsv.h = 60.0f * (((r_ - g_) / delta) + 4.0f);
    }

    if (hsv.h < 0.0f) {
        hsv.h += 360.0f;
    }
    if (hsv.h >= 360.0f) { // Ensure h is strictly < 360
        hsv.h = std::fmod(hsv.h, 360.0f);
    }


    // Saturation calculation
    hsv.s = (cmax == 0.0f) ? 0.0f : (delta / cmax);

    // Value calculation
    hsv.v = cmax;
}

void HsvToBLRgba32Internal(const HsvColor& hsv_color, BLRgba32& rgba) {
    float r_f, g_f, b_f; // r,g,b in [0,1]

    float h = hsv_color.h; // Expected to be [0, 360)
    float s = hsv_color.s; // 0-1
    float v = hsv_color.v; // 0-1

    if (s == 0.0f) { // Achromatic (grey)
        r_f = g_f = b_f = v;
    } else {
        float h_sector = h / 60.0f;
        // if (h_sector >= 6.0f) h_sector = 0.0f; // Should not happen if h is [0, 360)
        int i = static_cast<int>(h_sector);
        float f = h_sector - i; // Fractional part

        float p = v * (1.0f - s);
        float q = v * (1.0f - (s * f));
        float t = v * (1.0f - (s * (1.0f - f)));

        switch (i) {
            case 0: r_f = v; g_f = t; b_f = p; break;
            case 1: r_f = q; g_f = v; b_f = p; break;
            case 2: r_f = p; g_f = v; b_f = t; break;
            case 3: r_f = p; g_f = q; b_f = v; break;
            case 4: r_f = t; g_f = p; b_f = v; break;
            case 5: default: r_f = v; g_f = p; b_f = q; break; // Handles sector 5 and h_sector close to 6.0
        }
    }

    rgba.reset(
        static_cast<uint32_t>(std::round(r_f * 255.0f)),
        static_cast<uint32_t>(std::round(g_f * 255.0f)),
        static_cast<uint32_t>(std::round(b_f * 255.0f)),
        hsv_color.a // Use preserved alpha
    );
}

} // anonymous namespace

namespace ColorUtils {

BLRgba32 ShiftHue(BLRgba32 baseColor, float hueShiftDegrees) {
    HsvColor hsv;
    BLRgba32ToHsvInternal(baseColor, hsv);

    hsv.h = std::fmod(hsv.h + hueShiftDegrees, 360.0f);
    if (hsv.h < 0.0f) {
        hsv.h += 360.0f;
    }
    // Ensure h is strictly less than 360 for HsvToBLRgba32Internal's h_sector calculation
    if (hsv.h >= 360.0f) {
        hsv.h = 0.0f; 
    }


    BLRgba32 resultColor;
    HsvToBLRgba32Internal(hsv, resultColor);
    return resultColor;
}

BLRgba32 GenerateLayerColor(int layerIndex, int totalLayers, BLRgba32 baseColor, float hueStepDegrees) {
    if (totalLayers <= 0) return baseColor; // Should not happen

    float actualHueStep = hueStepDegrees;
    if (actualHueStep == 0.0f && totalLayers > 0) {
        actualHueStep = 360.0f / static_cast<float>(totalLayers);
    }
    // Ensure hueStep doesn't make colors repeat too quickly if many layers
    // For example, ensure it's at least 15-20 degrees if possible, or cap total shift
    if (totalLayers > 1 && actualHueStep * totalLayers > 360.f) {
         // If the steps would exceed 360 significantly, cap the step or distribute evenly.
         // For simplicity, let's just use the calculated even distribution if hueStepDegrees was 0.
         // If a specific hueStepDegrees was given, we respect it.
    }

    return ShiftHue(baseColor, static_cast<float>(layerIndex) * actualHueStep);
}

} // namespace ColorUtils 