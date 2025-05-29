#pragma once

#include <cmath> // For std::sqrt

// For now, let's assume simple types or define them if needed.
struct Vec2
{
    double x, y;

    Vec2(double x_ = 0.0, double y_ = 0.0) : x(x_), y(y_) {}

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
    double dot(const Vec2 &other) const { return x * other.x + y * other.y; }
    double lengthSquared() const { return x * x + y * y; }
    double length() const
    {
        if (lengthSquared() == 0.0)
            return 0.0; // Avoid sqrt(0) issues if any, though 0.0 is fine.
        return std::sqrt(lengthSquared());
    }
    Vec2 normalized() const
    {
        double l = length();
        if (l == 0)
            return {0, 0};
        return {x / l, y / l};
    }
};