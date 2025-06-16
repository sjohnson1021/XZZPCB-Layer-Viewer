#pragma once

#include <chrono>
#include <vector>
#include <iostream>
#include <string>

#include "GeometryUtils.hpp"
#include "Vec2.hpp"

namespace performance_test {

// Simple performance testing utilities
class PerformanceTimer {
private:
    std::chrono::high_resolution_clock::time_point m_start_time_;
    std::string m_test_name_;

public:
    explicit PerformanceTimer(const std::string& test_name) 
        : m_test_name_(test_name), m_start_time_(std::chrono::high_resolution_clock::now()) {}
    
    ~PerformanceTimer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - m_start_time_);
        std::cout << m_test_name_ << " took: " << duration.count() << " microseconds" << std::endl;
    }
    
    double GetElapsedMs() const {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(current_time - m_start_time_);
        return duration.count() / 1000.0;
    }
};

// Test vectorized math performance
void TestVectorizedMath() {
    std::cout << "\n=== Testing Vectorized Math Performance ===" << std::endl;
    
    // Create test data
    const size_t num_points = 10000;
    std::vector<Vec2> points;
    points.reserve(num_points);
    
    for (size_t i = 0; i < num_points; ++i) {
        points.emplace_back(static_cast<float>(i * 0.1), static_cast<float>(i * 0.2));
    }
    
    Vec2 reference_point(500.0f, 1000.0f);
    std::vector<float> results;
    
    // Test vectorized batch distance calculation
    {
        PerformanceTimer timer("Vectorized batch distance calculation");
        geometry_utils::BatchDistanceSquared(points, reference_point, results);
    }
    
    // Test traditional loop for comparison
    std::vector<float> traditional_results;
    traditional_results.resize(num_points);
    {
        PerformanceTimer timer("Traditional loop distance calculation");
        for (size_t i = 0; i < num_points; ++i) {
            float dx = static_cast<float>(points[i].x_ax - reference_point.x_ax);
            float dy = static_cast<float>(points[i].y_ax - reference_point.y_ax);
            traditional_results[i] = dx*dx + dy*dy;
        }
    }
    
    // Verify results are similar
    bool results_match = true;
    for (size_t i = 0; i < std::min(results.size(), traditional_results.size()); ++i) {
        if (std::abs(results[i] - traditional_results[i]) > 0.001f) {
            results_match = false;
            break;
        }
    }
    
    std::cout << "Results match: " << (results_match ? "YES" : "NO") << std::endl;
    
    // Test fast distance approximations
    {
        PerformanceTimer timer("Fast distance approximation");
        for (size_t i = 0; i < num_points; ++i) {
            geometry_utils::FastDistanceApprox(points[i], reference_point);
        }
    }
    
    {
        PerformanceTimer timer("Manhattan distance");
        for (size_t i = 0; i < num_points; ++i) {
            geometry_utils::FastDistance(points[i], reference_point);
        }
    }
    
    {
        PerformanceTimer timer("Exact distance (with sqrt)");
        for (size_t i = 0; i < num_points; ++i) {
            float dx = static_cast<float>(points[i].x_ax - reference_point.x_ax);
            float dy = static_cast<float>(points[i].y_ax - reference_point.y_ax);
            std::sqrt(dx*dx + dy*dy);
        }
    }
}

// Test spatial indexing performance
void TestSpatialIndexing() {
    std::cout << "\n=== Testing Spatial Indexing Performance ===" << std::endl;
    
    // This would require actual Element objects, so we'll just print a placeholder
    std::cout << "Spatial indexing test requires actual board data - implement in integration test" << std::endl;
}

// Run all performance tests
void RunAllTests() {
    std::cout << "=== PCB Renderer Performance Tests ===" << std::endl;
    TestVectorizedMath();
    TestSpatialIndexing();
    std::cout << "=== Performance Tests Complete ===" << std::endl;
}

} // namespace performance_test
