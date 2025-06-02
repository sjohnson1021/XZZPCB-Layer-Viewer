#pragma once

#include <cmath> // For std::sqrt

// For now, let's assume simple types or define them if needed.
struct Vec2
{
    double x, y;

    Vec2(double x = 0.0, double y = 0.0) : x(x), y(y) {}

    Vec2 operator-(const Vec2 &other) const { return {x - other.x, y - other.y}; }
    Vec2 operator+(const Vec2 &other) const { return {x + other.x, y + other.y}; }
    Vec2 operator*(double scalar) const { return {x * scalar, y * scalar}; }
    Vec2 operator/(double scalar) const { return {x / scalar, y / scalar}; }
    Vec2 &operator+=(const Vec2 &other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }
    Vec2 &operator-=(const Vec2 &other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    Vec2 &operator*=(double scalar)
    {
        x *= scalar;
        y *= scalar;
        return *this;
    }
    Vec2 &operator/=(double scalar)
    {
        x /= scalar;
        y /= scalar;
        return *this;
    }
    Vec2 operator-() const { return {-x, -y}; } // Unary minus
    [[nodiscard]] double Dot(const Vec2& other) const { return (x * other.x) + (y * other.y); }
    [[nodiscard]] double LengthSquared() const { return (x * x) + (y * y); }
    [[nodiscard]] double Length() const
    {
        if (LengthSquared() == 0.0) {
            return 0.0;  // Avoid sqrt(0) issues if any, though 0.0 is fine.
        }
        return std::sqrt(LengthSquared());
    }
    [[nodiscard]] Vec2 Normalized() const
    {
        double const l = Length();
        if (l == 0) {
            return {0, 0};
        }
        return {x / l, y / l};
    }
};