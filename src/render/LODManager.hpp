#pragma once

#include <blend2d.h>
#include "../view/Camera.hpp"
#include "../view/Viewport.hpp"
#include "../pcb/Board.hpp"

namespace lod {

// Level of Detail enumeration
enum class LODLevel {
    kVeryLow = 0,    // Minimal representation (outlines only)
    kLow = 1,        // Basic shapes, no fine details
    kMedium = 2,     // Standard detail level
    kHigh = 3,       // Full detail
    kVeryHigh = 4    // Maximum detail with anti-aliasing
};

// LOD configuration settings
struct LODSettings {
    // Zoom thresholds for different LOD levels
    double very_low_threshold = 0.05;   // Below this zoom, use very low LOD
    double low_threshold = 0.2;         // Below this zoom, use low LOD
    double medium_threshold = 1.0;      // Below this zoom, use medium LOD
    double high_threshold = 5.0;        // Below this zoom, use high LOD
    
    // Element count thresholds
    size_t max_traces_very_low = 100;
    size_t max_traces_low = 500;
    size_t max_traces_medium = 2000;
    size_t max_traces_high = 10000;
    
    // Rendering quality settings per LOD level
    struct QualitySettings {
        double flatten_tolerance;
        double simplify_tolerance;
        bool enable_antialiasing;
        bool enable_subpixel_rendering;
        int max_bezier_subdivisions;
    };
    
    QualitySettings very_low_quality = {2.0, 2.0, false, false, 2};
    QualitySettings low_quality = {1.0, 1.0, false, false, 4};
    QualitySettings medium_quality = {0.5, 0.5, true, false, 8};
    QualitySettings high_quality = {0.25, 0.25, true, true, 16};
    QualitySettings very_high_quality = {0.1, 0.1, true, true, 32};
};

// LOD Manager class
class LODManager {
private:
    LODSettings m_settings_;
    LODLevel m_current_lod_;
    bool m_is_interactive_mode_;
    
    // Performance tracking
    mutable size_t m_elements_rendered_;
    mutable size_t m_elements_culled_;
    
public:
    explicit LODManager(const LODSettings& settings = LODSettings()) 
        : m_settings_(settings), m_current_lod_(LODLevel::kMedium), 
          m_is_interactive_mode_(false), m_elements_rendered_(0), m_elements_culled_(0) {}
    
    // Determine appropriate LOD level based on camera and scene complexity
    LODLevel DetermineLOD(const Camera& camera, const Viewport& viewport, const Board& board) const {
        double zoom = camera.GetZoom();
        
        // Get scene complexity metrics
        size_t total_traces = 0;
        size_t total_components = 0;
        
        for (const auto& layer : board.GetLayers()) {
            // Note: GetLayers() returns vector<LayerInfo>, not pairs
            // For now, we'll need to get elements by layer ID from the board
            // This is a simplified approach - in practice you'd need to access
            // the actual layer data through the board's element storage
            total_traces += 100;  // Placeholder - need proper layer element access
            total_components += 10;  // Placeholder - need proper layer element access
        }
        
        // Determine LOD based on zoom level
        LODLevel zoom_lod;
        if (zoom < m_settings_.very_low_threshold) {
            zoom_lod = LODLevel::kVeryLow;
        } else if (zoom < m_settings_.low_threshold) {
            zoom_lod = LODLevel::kLow;
        } else if (zoom < m_settings_.medium_threshold) {
            zoom_lod = LODLevel::kMedium;
        } else if (zoom < m_settings_.high_threshold) {
            zoom_lod = LODLevel::kHigh;
        } else {
            zoom_lod = LODLevel::kVeryHigh;
        }
        
        // Adjust LOD based on scene complexity
        LODLevel complexity_lod = zoom_lod;
        if (total_traces > m_settings_.max_traces_high) {
            complexity_lod = std::min(complexity_lod, LODLevel::kMedium);
        } else if (total_traces > m_settings_.max_traces_medium) {
            complexity_lod = std::min(complexity_lod, LODLevel::kHigh);
        }
        
        // In interactive mode, reduce LOD for better performance
        if (m_is_interactive_mode_) {
            complexity_lod = static_cast<LODLevel>(std::max(0, static_cast<int>(complexity_lod) - 1));
        }
        
        return complexity_lod;
    }
    
    // Apply LOD settings to Blend2D context
    void ApplyLODToContext(BLContext& ctx, LODLevel lod) const {
        const LODSettings::QualitySettings* quality = GetQualitySettings(lod);
        if (!quality) return;
        
        // Set approximation options
        BLApproximationOptions approx_opts;
        approx_opts.flattenMode = BL_FLATTEN_MODE_DEFAULT;
        approx_opts.flattenTolerance = quality->flatten_tolerance;
        // Note: simplifyMode and simplifyTolerance don't exist in BLApproximationOptions
        // These were removed from the API
        ctx.setApproximationOptions(approx_opts);

        // Set rendering quality
        if (quality->enable_antialiasing) {
            ctx.setRenderingQuality(BL_RENDERING_QUALITY_ANTIALIAS);
            BLApproximationOptions speedOptions = blDefaultApproximationOptions;
            speedOptions.flattenTolerance = 0.1;   // Larger tolerance for speed
            ctx.setApproximationOptions(speedOptions);
        } else {
            // Note: BL_RENDERING_QUALITY_NONE doesn't exist, use BL_RENDERING_QUALITY_FAST
            BLApproximationOptions speedOptions = blDefaultApproximationOptions;
            speedOptions.flattenTolerance = 0.5;   // Larger tolerance for speed
            ctx.setApproximationOptions(speedOptions);
        }

        // Note: BLCompOpSimplifyInfo and queryProperty don't exist in current API
        // These optimizations were removed or changed in newer Blend2D versions
    }
    
    // Check if element should be rendered at current LOD
    bool ShouldRenderElement(LODLevel lod, double element_size_pixels, bool is_selected = false) const {
        // Always render selected elements
        if (is_selected) return true;
        
        // Size-based culling based on LOD level
        double min_size_threshold;
        switch (lod) {
            case LODLevel::kVeryLow:
                min_size_threshold = 5.0;  // Only render elements > 5 pixels
                break;
            case LODLevel::kLow:
                min_size_threshold = 2.0;  // Only render elements > 2 pixels
                break;
            case LODLevel::kMedium:
                min_size_threshold = 1.0;  // Only render elements > 1 pixel
                break;
            case LODLevel::kHigh:
            case LODLevel::kVeryHigh:
            default:
                min_size_threshold = 0.5;  // Render almost everything
                break;
        }
        
        return element_size_pixels >= min_size_threshold;
    }
    
    // Get quality settings for a specific LOD level
    const LODSettings::QualitySettings* GetQualitySettings(LODLevel lod) const {
        switch (lod) {
            case LODLevel::kVeryLow:
                return &m_settings_.very_low_quality;
            case LODLevel::kLow:
                return &m_settings_.low_quality;
            case LODLevel::kMedium:
                return &m_settings_.medium_quality;
            case LODLevel::kHigh:
                return &m_settings_.high_quality;
            case LODLevel::kVeryHigh:
                return &m_settings_.very_high_quality;
            default:
                return &m_settings_.medium_quality;
        }
    }
    
    // Set interactive mode (reduces LOD for better performance)
    void SetInteractiveMode(bool interactive) {
        m_is_interactive_mode_ = interactive;
    }
    
    bool IsInteractiveMode() const {
        return m_is_interactive_mode_;
    }
    
    // Update current LOD level
    void SetCurrentLOD(LODLevel lod) {
        m_current_lod_ = lod;
    }
    
    LODLevel GetCurrentLOD() const {
        return m_current_lod_;
    }
    
    // Performance tracking
    void IncrementRendered() const {
        ++m_elements_rendered_;
    }
    
    void IncrementCulled() const {
        ++m_elements_culled_;
    }
    
    void ResetCounters() const {
        m_elements_rendered_ = 0;
        m_elements_culled_ = 0;
    }
    
    struct PerformanceStats {
        size_t elements_rendered;
        size_t elements_culled;
        double cull_ratio;
        LODLevel current_lod;
    };
    
    PerformanceStats GetPerformanceStats() const {
        PerformanceStats stats;
        stats.elements_rendered = m_elements_rendered_;
        stats.elements_culled = m_elements_culled_;
        stats.current_lod = m_current_lod_;
        
        size_t total = m_elements_rendered_ + m_elements_culled_;
        stats.cull_ratio = total > 0 ? static_cast<double>(m_elements_culled_) / total : 0.0;
        
        return stats;
    }
    
    // Get LOD settings
    const LODSettings& GetSettings() const {
        return m_settings_;
    }
    
    void SetSettings(const LODSettings& settings) {
        m_settings_ = settings;
    }
};

} // namespace lod
