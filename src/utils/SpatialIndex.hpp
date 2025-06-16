#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>

#include "Vec2.hpp"
#include "../pcb/elements/Element.hpp"

namespace spatial_index {

// Simple spatial hash grid for fast spatial queries
class SpatialHashGrid {
public:
    struct GridCell {
        std::vector<const Element*> elements;
        
        void AddElement(const Element* element) {
            elements.push_back(element);
        }
        
        void Clear() {
            elements.clear();
        }
    };

private:
    double m_cell_size_;
    std::unordered_map<uint64_t, GridCell> m_grid_;
    
    // Hash function for grid coordinates
    uint64_t HashCoords(int x, int y) const {
        // Simple hash combining x and y coordinates
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint64_t>(y);
    }
    
    // Convert world coordinates to grid coordinates
    void WorldToGrid(double world_x, double world_y, int& grid_x, int& grid_y) const {
        grid_x = static_cast<int>(std::floor(world_x / m_cell_size_));
        grid_y = static_cast<int>(std::floor(world_y / m_cell_size_));
    }

public:
    explicit SpatialHashGrid(double cell_size = 10.0) : m_cell_size_(cell_size) {}
    
    void SetCellSize(double cell_size) {
        m_cell_size_ = cell_size;
        Clear();
    }
    
    void Clear() {
        m_grid_.clear();
    }
    
    // Add an element to the spatial index
    void AddElement(const Element* element, const Component* parent_component = nullptr) {
        if (!element) return;
        
        // Get bounding box of the element
        auto bbox = element->GetBoundingBox(parent_component);
        
        // Calculate grid range that this element spans
        int min_x, min_y, max_x, max_y;
        WorldToGrid(bbox.x, bbox.y, min_x, min_y);
        WorldToGrid(bbox.x + bbox.w, bbox.y + bbox.h, max_x, max_y);
        
        // Add element to all cells it overlaps
        for (int x = min_x; x <= max_x; ++x) {
            for (int y = min_y; y <= max_y; ++y) {
                uint64_t hash = HashCoords(x, y);
                m_grid_[hash].AddElement(element);
            }
        }
    }
    
    // Query elements near a point
    std::vector<const Element*> QueryPoint(const Vec2& point, double radius = 0.0) const {
        std::vector<const Element*> results;
        
        // Calculate grid range to search
        int min_x, min_y, max_x, max_y;
        WorldToGrid(point.x_ax - radius, point.y_ax - radius, min_x, min_y);
        WorldToGrid(point.x_ax + radius, point.y_ax + radius, max_x, max_y);
        
        // Collect elements from all relevant cells
        std::unordered_set<const Element*> unique_elements;
        for (int x = min_x; x <= max_x; ++x) {
            for (int y = min_y; y <= max_y; ++y) {
                uint64_t hash = HashCoords(x, y);
                auto it = m_grid_.find(hash);
                if (it != m_grid_.end()) {
                    for (const Element* element : it->second.elements) {
                        unique_elements.insert(element);
                    }
                }
            }
        }
        
        // Convert to vector
        results.reserve(unique_elements.size());
        for (const Element* element : unique_elements) {
            results.push_back(element);
        }
        
        return results;
    }
    
    // Query elements in a rectangular region
    std::vector<const Element*> QueryRect(double min_x, double min_y, double max_x, double max_y) const {
        std::vector<const Element*> results;
        
        // Calculate grid range to search
        int grid_min_x, grid_min_y, grid_max_x, grid_max_y;
        WorldToGrid(min_x, min_y, grid_min_x, grid_min_y);
        WorldToGrid(max_x, max_y, grid_max_x, grid_max_y);
        
        // Collect elements from all relevant cells
        std::unordered_set<const Element*> unique_elements;
        for (int x = grid_min_x; x <= grid_max_x; ++x) {
            for (int y = grid_min_y; y <= grid_max_y; ++y) {
                uint64_t hash = HashCoords(x, y);
                auto it = m_grid_.find(hash);
                if (it != m_grid_.end()) {
                    for (const Element* element : it->second.elements) {
                        unique_elements.insert(element);
                    }
                }
            }
        }
        
        // Convert to vector
        results.reserve(unique_elements.size());
        for (const Element* element : unique_elements) {
            results.push_back(element);
        }
        
        return results;
    }
    
    size_t GetElementCount() const {
        size_t total = 0;
        for (const auto& pair : m_grid_) {
            total += pair.second.elements.size();
        }
        return total;
    }
    
    size_t GetCellCount() const {
        return m_grid_.size();
    }
};

// Optimized hit detection using spatial indexing
class OptimizedHitDetector {
private:
    SpatialHashGrid m_spatial_index_;
    bool m_index_dirty_;
    
public:
    OptimizedHitDetector() : m_index_dirty_(true) {}
    
    void SetCellSize(double cell_size) {
        m_spatial_index_.SetCellSize(cell_size);
        m_index_dirty_ = true;
    }
    
    void RebuildIndex(const std::vector<const Element*>& elements, const Component* parent_component = nullptr) {
        m_spatial_index_.Clear();
        
        for (const Element* element : elements) {
            if (element && element->IsVisible()) {
                m_spatial_index_.AddElement(element, parent_component);
            }
        }
        
        m_index_dirty_ = false;
    }
    
    // Fast hit detection using spatial indexing
    const Element* FindHitElement(const Vec2& world_pos, float tolerance, const Component* parent_component = nullptr) {
        if (m_index_dirty_) {
            // Index needs rebuilding - fall back to linear search for now
            return nullptr;
        }
        
        // Query spatial index for nearby elements
        auto candidates = m_spatial_index_.QueryPoint(world_pos, tolerance);
        
        // Test candidates for actual hit
        for (const Element* element : candidates) {
            if (element && element->IsVisible() && element->IsHit(world_pos, tolerance, parent_component)) {
                return element;
            }
        }
        
        return nullptr;
    }
    
    void MarkDirty() {
        m_index_dirty_ = true;
    }
    
    bool IsIndexValid() const {
        return !m_index_dirty_;
    }
};

} // namespace spatial_index
