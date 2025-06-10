#include "ColorUtils.hpp"

#include <algorithm>  // For std::min, std::max
#include <cmath>
#include <cstdint>
// cmath is already included via ColorUtils.hpp for std::fmod, std::fabs, std::round

namespace color_utils
{

void BLRgba32ToHsv(const BLRgba32& rgba, HsvColor& hsv)
{
    float red = static_cast<float>(rgba.r()) / 255.0F;
    float green = static_cast<float>(rgba.g()) / 255.0F;
    float blue = static_cast<float>(rgba.b()) / 255.0F;
    hsv.alpha = rgba.a();  // Preserve alpha

    float cmax = std::max({red, green, blue});
    float cmin = std::min({red, green, blue});
    float const kDelta = cmax - cmin;

    // Hue calculation
    if (kDelta == 0.0F) {
        hsv.hue = 0.0F;  // Achromatic, hue is undefined, conventionally 0
    } else if (cmax == red) {
        hsv.hue = 60.0F * std::fmod(((green - blue) / kDelta), 6.0F);
    } else if (cmax == green) {
        hsv.hue = 60.0F * (((blue - red) / kDelta) + 2.0F);
    } else {  // cmax == blue
        hsv.hue = 60.0F * (((red - green) / kDelta) + 4.0F);
    }

    if (hsv.hue < 0.0F) {
        hsv.hue += 360.0F;
    }
    if (hsv.hue >= 360.0F) {  // Ensure h is strictly < 360
        hsv.hue = std::fmod(hsv.hue, 360.0F);
    }

    // Saturation calculation
    hsv.sat = (cmax == 0.0F) ? 0.0F : (kDelta / cmax);

    // Value calculation
    hsv.val = cmax;
}

void HsvToBLRgba32(const HsvColor& hsv_color, BLRgba32& rgba)
{
    float red_f = NAN;
    float green_f = NAN;
    float blue_f = NAN;  // RGB components in [0,1] range

    float const kHue = hsv_color.hue;  // Expected to be [0, 360)
    float const kSat = hsv_color.sat;  // 0-1
    float const kVal = hsv_color.val;  // 0-1

    if (kSat == 0.0F) {  // Achromatic (grey)
        red_f = green_f = blue_f = kVal;
    } else {
        float const kHueSector = kHue / 60.0F;
        // if (hue_sector >= 6.0f) hue_sector = 0.0f; // Should not happen if hue is [0, 360)
        int const kSectorIndex = static_cast<int>(kHueSector);
        float const kFraction = kHueSector - kSectorIndex;  // Fractional part

        float const kPrimary = kVal * (1.0F - kSat);
        float const kSecondary = kVal * (1.0F - (kSat * kFraction));
        float const kTertiary = kVal * (1.0F - (kSat * (1.0F - kFraction)));

        switch (kSectorIndex) {
            case 0:
                red_f = kVal;
                green_f = kTertiary;
                blue_f = kPrimary;
                break;
            case 1:
                red_f = kSecondary;
                green_f = kVal;
                blue_f = kPrimary;
                break;
            case 2:
                red_f = kPrimary;
                green_f = kVal;
                blue_f = kTertiary;
                break;
            case 3:
                red_f = kPrimary;
                green_f = kSecondary;
                blue_f = kVal;
                break;
            case 4:
                red_f = kTertiary;
                green_f = kPrimary;
                blue_f = kVal;
                break;
            case 5:
            default:
                red_f = kVal;
                green_f = kPrimary;
                blue_f = kSecondary;
                break;  // Handles sector 5 and hue_sector close to 6.0
        }
    }

    rgba.reset(static_cast<uint32_t>(std::round(red_f * 255.0F)),
               static_cast<uint32_t>(std::round(green_f * 255.0F)),
               static_cast<uint32_t>(std::round(blue_f * 255.0F)),
               hsv_color.alpha  // Use preserved alpha
    );
}

// extern void HSVtoRGB(float hue, float sat, float val, float& red, float& green, float& blue)
// {
//     BLRgba32 rgba;
//     HsvColor hsv;
//     hsv.hue = hue;
//     hsv.sat = sat;
//     hsv.val = val;
//     HsvToBLRgba32(hsv, rgba);
//     red = rgba.r();
//     green = rgba.g();
//     blue = rgba.b();
// }

// extern void RGBtoHSV(float red, float green, float blue, float& hue, float& saturation, float& value)
// {
//     BLRgba32 rgba;
//     rgba.reset(red, green, blue, 255);
//     HsvColor hsv;
//     BLRgba32ToHsv(rgba, hsv);
//     hue = hsv.hue;
//     saturation = hsv.sat;
//     value = hsv.val;
// }
BLRgba32 ShiftHue(BLRgba32 base_color, float hue_shift_degrees)
{
    HsvColor hsv;
    BLRgba32ToHsv(base_color, hsv);

    hsv.hue = std::fmod(hsv.hue + hue_shift_degrees, 360.0F);
    if (hsv.hue < 0.0F) {
        hsv.hue += 360.0F;
    }
    // Ensure h is strictly less than 360 for HsvToBLRgba32Internal's h_sector calculation
    if (hsv.hue >= 360.0F) {
        hsv.hue = 0.0F;
    }

    BLRgba32 result_color;
    HsvToBLRgba32(hsv, result_color);
    return result_color;
}

BLRgba32 GenerateLayerColor(int layer_index, int total_layers, BLRgba32 base_color, float hue_step_degrees)
{
    if (total_layers <= 0) {
        return base_color;  // Should not happen
    }

    float actual_hue_step = hue_step_degrees;
    if (actual_hue_step == 0.0F && total_layers > 0) {
        actual_hue_step = 360.0F / static_cast<float>(total_layers);
    }
    // Ensure hueStep doesn't make colors repeat too quickly if many layers
    // For example, ensure it's at least 15-20 degrees if possible, or cap total shift
    if (total_layers > 1 && actual_hue_step * total_layers > 360.F) {
        // If the steps would exceed 360 significantly, cap the step or distribute evenly.
        // For simplicity, let's just use the calculated even distribution if hueStepDegrees was 0.
        // If a specific hueStepDegrees was given, we respect it.
    }

    return ShiftHue(base_color, static_cast<float>(layer_index) * actual_hue_step);
}

}  // namespace color_utils
