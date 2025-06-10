#pragma once

#include "../Board.hpp" // Assuming Board aggregates Components
#include "../elements/Component.hpp"
#include "../elements/Pin.hpp"
#include <vector>
#include <string>
#include <limits>    // Required for std::numeric_limits
#include <cmath>     // Required for std::abs, std::min, std::max, std::sqrt, std::pow, std::fmod, std::atan2, std::acos
#include <algorithm> // Required for std::find, std::min, std::max

// Forward declaration if XZZPCBLoader is used directly, or pass necessary data
// class XZZPCBLoader;

namespace PCBProcessing
{

    class OrientationProcessor
    {
    public:
        // Option 1: Process a single component
        // static void processComponent(Component& component);

        // Option 2: Process all components on a board
        static void processBoard(Board &board);

    private:
        // Helper methods adapted from the ConvertPCBtoLineSegments logic
        static void calculateInitialPinDimensions(Pin &pin);
        static void calculatePinBoundingBox(Component &component, double tolerance);
        static void firstPass_AnalyzeComponent(Component &component, double component_rotation_radians);
        static void secondPass_CheckOverlapsAndBoundaries(Component &component, double component_rotation_radians);
        static void thirdPass_FinalBoundaryCheck(Component &component, double component_rotation_radians);

        // Helper to determine component's body rotation
        static double calculateComponentRotation(const Component &component);
    };

} // namespace PCBProcessing