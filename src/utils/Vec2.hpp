#pragma once

#include <cmath>  // For std::sqrt

// For now, let's assume simple types or define them if needed.
struct Vec2 {
    double x_ax, y_ax;

    Vec2(double x = 0.0, double y = 0.0) : x_ax(x), y_ax(y) {}

    Vec2 operator-(const Vec2& other) const { return {x_ax - other.x_ax, y_ax - other.y_ax}; }
    Vec2 operator+(const Vec2& other) const { return {x_ax + other.x_ax, y_ax + other.y_ax}; }
    Vec2 operator*(double scalar) const { return {x_ax * scalar, y_ax * scalar}; }
    Vec2 operator/(double scalar) const { return {x_ax / scalar, y_ax / scalar}; }
    Vec2& operator+=(const Vec2& other)
    {
        x_ax += other.x_ax;
        y_ax += other.y_ax;
        return *this;
    }
    Vec2& operator-=(const Vec2& other)
    {
        x_ax -= other.x_ax;
        y_ax -= other.y_ax;
        return *this;
    }
    Vec2& operator*=(double scalar)
    {
        x_ax *= scalar;
        y_ax *= scalar;
        return *this;
    }
    Vec2& operator/=(double scalar)
    {
        x_ax /= scalar;
        y_ax /= scalar;
        return *this;
    }
    Vec2 operator-() const { return {-x_ax, -y_ax}; }  // Unary minus
    [[nodiscard]] double Dot(const Vec2& other) const { return (x_ax * other.x_ax) + (y_ax * other.y_ax); }
    [[nodiscard]] double LengthSquared() const { return (x_ax * x_ax) + (y_ax * y_ax); }
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
        return {x_ax / l, y_ax / l};
    }
};
