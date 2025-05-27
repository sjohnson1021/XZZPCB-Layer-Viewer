#include <vector>
#include <cmath>
#include <algorithm>
#include <map>
#include <cstdio>

struct Point
{
    double x, y;
    Point(double x = 0, double y = 0) : x(x), y(y) {}
};

struct Pin
{
    int id;
    Point position;
    double width, height;
    int type;        // 1 or 2
    double rotation; // calculated rotation in degrees

    Pin(int id, double x, double y, double w, double h, int type = 1)
        : id(id), position(x, y), width(w), height(h), type(type), rotation(0) {}
};

struct LineSegment
{
    Point start, end;
    double angle; // angle in degrees
    double length;

    LineSegment(Point s, Point e) : start(s), end(e)
    {
        double dx = end.x - start.x;
        double dy = end.y - start.y;
        length = std::sqrt(dx * dx + dy * dy);
        angle = std::atan2(dy, dx) * 180.0 / M_PI;
        // Normalize angle to [0, 180) for easier comparison
        if (angle < 0)
            angle += 180.0;
    }
};

struct ComponentOutline
{
    Point min, max;                 // bounding box
    std::vector<Point> vertices;    // outline vertices
    std::vector<LineSegment> edges; // component edges
    double componentRotation;       // detected component rotation in degrees
    double perpendicularRotation;   // perpendicular angle

    ComponentOutline(const std::vector<Point> &outline_points) : componentRotation(0), perpendicularRotation(90)
    {
        if (outline_points.empty())
            return;

        min = max = outline_points[0];
        for (const auto &pt : outline_points)
        {
            vertices.push_back(pt);
            min.x = std::min(min.x, pt.x);
            min.y = std::min(min.y, pt.y);
            max.x = std::max(max.x, pt.x);
            max.y = std::max(max.y, pt.y);
        }

        // Create edges from consecutive vertices
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            size_t next = (i + 1) % vertices.size();
            edges.emplace_back(vertices[i], vertices[next]);
        }

        detectComponentRotation();
    }

    ComponentOutline(const std::vector<LineSegment> &line_segments) : componentRotation(0), perpendicularRotation(90)
    {
        edges = line_segments;

        // Build bounding box from line segments
        if (!edges.empty())
        {
            min = max = edges[0].start;
            for (const auto &seg : edges)
            {
                updateBounds(seg.start);
                updateBounds(seg.end);
                vertices.push_back(seg.start);
                vertices.push_back(seg.end);
            }
        }

        detectComponentRotation();
    }

private:
    void updateBounds(const Point &pt)
    {
        min.x = std::min(min.x, pt.x);
        min.y = std::min(min.y, pt.y);
        max.x = std::max(max.x, pt.x);
        max.y = std::max(max.y, pt.y);
    }

    void detectComponentRotation()
    {
        if (edges.empty())
            return;

        // Analyze edge angles to determine component orientation
        std::map<int, int> angleFrequency; // angle (rounded to nearest degree) -> count

        for (const auto &edge : edges)
        {
            if (edge.length > 1000)
            { // Only consider significant edges
                int roundedAngle = static_cast<int>(std::round(edge.angle));
                // Group similar angles (within 5 degrees)
                bool foundGroup = false;
                for (auto &[angle, count] : angleFrequency)
                {
                    if (std::abs(angle - roundedAngle) <= 5)
                    {
                        count++;
                        foundGroup = true;
                        break;
                    }
                }
                if (!foundGroup)
                {
                    angleFrequency[roundedAngle] = 1;
                }
            }
        }

        // Find the most common angle (dominant edge direction)
        int maxCount = 0;
        int dominantAngle = 0;
        for (const auto &[angle, count] : angleFrequency)
        {
            if (count > maxCount)
            {
                maxCount = count;
                dominantAngle = angle;
            }
        }

        // Set component rotation to the dominant angle
        componentRotation = dominantAngle;
        perpendicularRotation = componentRotation + 90.0;

        // Normalize angles
        if (perpendicularRotation >= 180.0)
        {
            perpendicularRotation -= 180.0;
        }
    }
};

enum class EdgeSide
{
    LEFT,
    RIGHT,
    TOP,
    BOTTOM,
    INTERIOR
};

class PinRotationCalculator
{
private:
    static constexpr double EDGE_TOLERANCE = 0.1; // tolerance for edge detection

    // Determine which edge of the component a pin is closest to
    EdgeSide determineEdgeSide(const Pin &pin, const ComponentOutline &outline)
    {
        Point center = pin.position;

        // Find the closest actual edge (line segment) to the pin
        double minDistance = std::numeric_limits<double>::max();
        const LineSegment *closestEdge = nullptr;

        for (const auto &edge : outline.edges)
        {
            double dist = distancePointToLineSegment(center, edge);
            if (dist < minDistance)
            {
                minDistance = dist;
                closestEdge = &edge;
            }
        }

        if (!closestEdge)
        {
            return EdgeSide::INTERIOR;
        }

        // Determine edge orientation based on the detected component rotation
        double edgeAngle = closestEdge->angle;
        double componentAngle = outline.componentRotation;
        double perpAngle = outline.perpendicularRotation;

        // Check if edge is parallel to component's primary orientation
        if (std::abs(angleDifference(edgeAngle, componentAngle)) < 15.0)
        {
            // This is a horizontal edge relative to component orientation
            // Determine if it's top or bottom based on pin position
            Point edgeCenter = {
                (closestEdge->start.x + closestEdge->end.x) / 2.0,
                (closestEdge->start.y + closestEdge->end.y) / 2.0};

            // Project the difference onto the perpendicular direction
            double dx = center.x - edgeCenter.x;
            double dy = center.y - edgeCenter.y;
            double perpAngleRad = perpAngle * M_PI / 180.0;
            double projectedDist = dx * std::sin(perpAngleRad) + dy * std::cos(perpAngleRad);

            return (projectedDist > 0) ? EdgeSide::TOP : EdgeSide::BOTTOM;
        }
        else if (std::abs(angleDifference(edgeAngle, perpAngle)) < 15.0)
        {
            // This is a vertical edge relative to component orientation
            Point edgeCenter = {
                (closestEdge->start.x + closestEdge->end.x) / 2.0,
                (closestEdge->start.y + closestEdge->end.y) / 2.0};

            // Project the difference onto the primary direction
            double dx = center.x - edgeCenter.x;
            double dy = center.y - edgeCenter.y;
            double compAngleRad = componentAngle * M_PI / 180.0;
            double projectedDist = dx * std::cos(compAngleRad) + dy * std::sin(compAngleRad);

            return (projectedDist > 0) ? EdgeSide::RIGHT : EdgeSide::LEFT;
        }

        // If pin is far from edges, it's interior
        if (minDistance > 50000)
        { // adjust threshold as needed
            return EdgeSide::INTERIOR;
        }

        // Default fallback
        return EdgeSide::INTERIOR;
    }

    // Helper function to calculate distance from point to line segment
    double distancePointToLineSegment(const Point &p, const LineSegment &seg)
    {
        double A = p.x - seg.start.x;
        double B = p.y - seg.start.y;
        double C = seg.end.x - seg.start.x;
        double D = seg.end.y - seg.start.y;

        double dot = A * C + B * D;
        double len_sq = C * C + D * D;

        if (len_sq == 0)
        {
            // Line segment is actually a point
            return std::sqrt(A * A + B * B);
        }

        double param = dot / len_sq;

        double xx, yy;
        if (param < 0)
        {
            xx = seg.start.x;
            yy = seg.start.y;
        }
        else if (param > 1)
        {
            xx = seg.end.x;
            yy = seg.end.y;
        }
        else
        {
            xx = seg.start.x + param * C;
            yy = seg.start.y + param * D;
        }

        double dx = p.x - xx;
        double dy = p.y - yy;
        return std::sqrt(dx * dx + dy * dy);
    }

    // Helper function to calculate the smallest angle difference between two angles
    double angleDifference(double angle1, double angle2)
    {
        double diff = std::abs(angle1 - angle2);
        if (diff > 90.0)
        {
            diff = 180.0 - diff;
        }
        return diff;
    }

    // Group pins by their edge association
    std::map<EdgeSide, std::vector<Pin *>> groupPinsByEdge(
        std::vector<Pin> &pins, const ComponentOutline &outline)
    {

        std::map<EdgeSide, std::vector<Pin *>> edgeGroups;

        for (auto &pin : pins)
        {
            EdgeSide edge = determineEdgeSide(pin, outline);
            edgeGroups[edge].push_back(&pin);
        }

        return edgeGroups;
    }

    // Calculate rotation for pins on a specific edge
    double calculateEdgeRotation(EdgeSide edge, const Pin &pin,
                                 const std::vector<Pin *> &edgePins,
                                 const ComponentOutline &outline)
    {

        // Use the component's detected rotation angles
        double componentAngle = outline.componentRotation;
        double perpendicularAngle = outline.perpendicularRotation;

        switch (edge)
        {
        case EdgeSide::LEFT:
        case EdgeSide::RIGHT:
            // Pins on left/right edges should align with the component's perpendicular direction
            // Orient the pin so its shortest dimension is parallel to the edge
            if (pin.width > pin.height)
            {
                // Pin is wider than tall, rotate to make it taller
                return perpendicularAngle;
            }
            else
            {
                // Pin is already oriented correctly relative to component
                return componentAngle;
            }

        case EdgeSide::TOP:
        case EdgeSide::BOTTOM:
            // Pins on top/bottom edges should align with the component's primary direction
            // Orient the pin so its shortest dimension is parallel to the edge
            if (pin.height > pin.width)
            {
                // Pin is taller than wide, rotate to make it wider
                return componentAngle;
            }
            else
            {
                // Pin is already oriented correctly relative to component
                return perpendicularAngle;
            }

        case EdgeSide::INTERIOR:
            // Interior pins typically align with the component's primary orientation
            return componentAngle;

        default:
            return componentAngle;
        }
    }

    // Advanced heuristic for detecting pin rows and columns
    bool detectPinAlignment(const std::vector<Pin *> &pins, bool checkHorizontal = true)
    {
        if (pins.size() < 2)
            return false;

        const double ALIGNMENT_TOLERANCE = 1000.0; // adjust based on your coordinate system

        if (checkHorizontal)
        {
            // Check if pins are horizontally aligned (same Y coordinate)
            double refY = pins[0]->position.y;
            for (const auto &pin : pins)
            {
                if (std::abs(pin->position.y - refY) > ALIGNMENT_TOLERANCE)
                {
                    return false;
                }
            }
        }
        else
        {
            // Check if pins are vertically aligned (same X coordinate)
            double refX = pins[0]->position.x;
            for (const auto &pin : pins)
            {
                if (std::abs(pin->position.x - refX) > ALIGNMENT_TOLERANCE)
                {
                    return false;
                }
            }
        }

        return true;
    }

public:
    // Main function to calculate rotations for all pins
    void calculatePinRotations(std::vector<Pin> &pins, const ComponentOutline &outline)
    {
        // Group pins by their edge association
        auto edgeGroups = groupPinsByEdge(pins, outline);

        // Process each edge group
        for (auto &[edge, edgePins] : edgeGroups)
        {

            // For edge pins, apply rotation based on edge orientation
            if (edge != EdgeSide::INTERIOR)
            {

                // Additional check: verify pins are actually aligned in rows/columns
                bool isHorizontalRow = detectPinAlignment(edgePins, true);
                bool isVerticalColumn = detectPinAlignment(edgePins, false);

                for (auto &pin : edgePins)
                {
                    double rotation = calculateEdgeRotation(edge, *pin, edgePins, outline);

                    // Refine rotation based on detected alignment and component orientation
                    if (edge == EdgeSide::LEFT || edge == EdgeSide::RIGHT)
                    {
                        if (isVerticalColumn && pin->width > pin->height)
                        {
                            rotation = outline.perpendicularRotation;
                        }
                    }
                    else if (edge == EdgeSide::TOP || edge == EdgeSide::BOTTOM)
                    {
                        if (isHorizontalRow && pin->height > pin->width)
                        {
                            rotation = outline.componentRotation;
                        }
                    }

                    pin->rotation = rotation;
                }
            }
            else
            {
                // Interior pins - align with component orientation
                for (auto &pin : edgePins)
                {
                    pin->rotation = outline.componentRotation;
                }
            }
        }
    }

    // Utility function to validate pin placement (detect overlaps)
    bool validatePinPlacement(const std::vector<Pin> &pins)
    {
        const double OVERLAP_TOLERANCE = 1000.0; // minimum spacing between pins

        for (size_t i = 0; i < pins.size(); ++i)
        {
            for (size_t j = i + 1; j < pins.size(); ++j)
            {
                const Pin &pin1 = pins[i];
                const Pin &pin2 = pins[j];

                double dx = pin1.position.x - pin2.position.x;
                double dy = pin1.position.y - pin2.position.y;
                double distance = std::sqrt(dx * dx + dy * dy);

                // Calculate effective dimensions considering rotation
                double pin1_w = (pin1.rotation == 90.0) ? pin1.height : pin1.width;
                double pin1_h = (pin1.rotation == 90.0) ? pin1.width : pin1.height;
                double pin2_w = (pin2.rotation == 90.0) ? pin2.height : pin2.width;
                double pin2_h = (pin2.rotation == 90.0) ? pin2.width : pin2.height;

                double minRequiredDistance = std::max(
                                                 (pin1_w + pin2_w) / 2.0,
                                                 (pin1_h + pin2_h) / 2.0) +
                                             OVERLAP_TOLERANCE;

                if (distance < minRequiredDistance)
                {
                    return false; // Overlap detected
                }
            }
        }
        return true;
    }
};

// Example usage function
void processComponentPins()
{
    // Example: N1155 IC component from your data
    // Create line segments from your file data
    std::vector<LineSegment> lineSegments = {
        {{529733110, 513820830}, {532020270, 513820830}},
        {{529733110, 511533660}, {529733110, 513820830}},
        {{529733110, 511533660}, {532020270, 511533660}},
        {{532020270, 511533660}, {532020270, 513820830}}};

    ComponentOutline outline(lineSegments);

    std::vector<Pin> pins;

    // Add example pins from your data
    pins.emplace_back(1, 529913110, 513381540, 330000, 70000);
    pins.emplace_back(20, 531840270, 513381540, 330000, 70000);
    pins.emplace_back(21, 531595000, 511713660, 330000, 70000);
    pins.emplace_back(31, 531595000, 513640830, 330000, 70000);
    pins.emplace_back(41, 530873190, 512666730, 1140000, 1130000); // central pad

    PinRotationCalculator calculator;
    calculator.calculatePinRotations(pins, outline);

    // Validate the result
    bool isValid = calculator.validatePinPlacement(pins);

    // Output results (for debugging)
    printf("Component rotation detected: %.1f degrees\n", outline.componentRotation);
    printf("Perpendicular angle: %.1f degrees\n", outline.perpendicularRotation);

    for (const auto &pin : pins)
    {
        printf("Pin %d: Rotation = %.1f degrees\n", pin.id, pin.rotation);
    }

    printf("Pin placement valid: %s\n", isValid ? "Yes" : "No");
}

// Utility function to create ComponentOutline from your file's line segment data
ComponentOutline createComponentFromLineSegments(const std::vector<std::pair<std::pair<int, int>, std::pair<int, int>>> &segments)
{
    std::vector<LineSegment> lineSegs;

    for (const auto &seg : segments)
    {
        Point start(seg.first.first, seg.first.second);
        Point end(seg.second.first, seg.second.second);
        lineSegs.emplace_back(start, end);
    }

    return ComponentOutline(lineSegs);
}