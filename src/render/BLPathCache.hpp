#pragma once

#include <blend2d.h>
#include <unordered_map>
#include <string>
#include <memory>
#include <chrono>

namespace path_cache {

// Cache key for path operations
struct PathCacheKey {
    std::string element_id;
    double thickness;
    BLStrokeCap start_cap;
    BLStrokeCap end_cap;
    uint32_t transform_hash;  // Hash of transformation matrix
    
    bool operator==(const PathCacheKey& other) const {
        return element_id == other.element_id &&
               std::abs(thickness - other.thickness) < 0.001 &&
               start_cap == other.start_cap &&
               end_cap == other.end_cap &&
               transform_hash == other.transform_hash;
    }
};

// Hash function for PathCacheKey
struct PathCacheKeyHash {
    std::size_t operator()(const PathCacheKey& key) const {
        std::size_t h1 = std::hash<std::string>{}(key.element_id);
        std::size_t h2 = std::hash<double>{}(key.thickness);
        std::size_t h3 = std::hash<int>{}(static_cast<int>(key.start_cap));
        std::size_t h4 = std::hash<int>{}(static_cast<int>(key.end_cap));
        std::size_t h5 = std::hash<uint32_t>{}(key.transform_hash);
        
        // Combine hashes
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
    }
};

// Cached path entry
struct CachedPath {
    BLPath original_path;
    BLPath stroked_path;
    BLStrokeOptions stroke_options;
    std::chrono::steady_clock::time_point last_used;
    bool is_valid;
    
    CachedPath() : is_valid(false) {
        last_used = std::chrono::steady_clock::now();
    }
    
    void UpdateLastUsed() {
        last_used = std::chrono::steady_clock::now();
    }
    
    bool IsExpired(std::chrono::milliseconds max_age) const {
        auto now = std::chrono::steady_clock::now();
        return (now - last_used) > max_age;
    }
};

// High-performance path cache for Blend2D operations
class BLPathCache {
private:
    std::unordered_map<PathCacheKey, CachedPath, PathCacheKeyHash> m_cache_;
    std::chrono::milliseconds m_max_age_;
    size_t m_max_entries_;
    mutable size_t m_cache_hits_;
    mutable size_t m_cache_misses_;
    
    // Clean up expired entries
    void CleanupExpired() {
        auto now = std::chrono::steady_clock::now();
        auto it = m_cache_.begin();
        while (it != m_cache_.end()) {
            if (it->second.IsExpired(m_max_age_)) {
                it = m_cache_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Remove least recently used entries if cache is full
    void EvictLRU() {
        if (m_cache_.size() <= m_max_entries_) {
            return;
        }
        
        // Find oldest entry
        auto oldest_it = m_cache_.begin();
        for (auto it = m_cache_.begin(); it != m_cache_.end(); ++it) {
            if (it->second.last_used < oldest_it->second.last_used) {
                oldest_it = it;
            }
        }
        
        if (oldest_it != m_cache_.end()) {
            m_cache_.erase(oldest_it);
        }
    }

public:
    explicit BLPathCache(size_t max_entries = 1000, std::chrono::milliseconds max_age = std::chrono::minutes(5))
        : m_max_age_(max_age), m_max_entries_(max_entries), m_cache_hits_(0), m_cache_misses_(0) {}
    
    // Get or create a stroked path
    const BLPath& GetStrokedPath(const PathCacheKey& key, const BLPath& original_path,
                                const BLStrokeOptions& stroke_options) {
        // Check if path is in cache
        auto it = m_cache_.find(key);
        if (it != m_cache_.end() && it->second.is_valid && !it->second.IsExpired(m_max_age_)) {
            it->second.UpdateLastUsed();
            ++m_cache_hits_;
            return it->second.stroked_path;
        }

        ++m_cache_misses_;

        // Create new cached entry
        CachedPath& cached = m_cache_[key];
        cached.original_path = original_path;
        cached.stroke_options = stroke_options;

        // Generate stroked path using correct Blend2D API
        cached.stroked_path.clear();

        // Use BLPath::addStrokedPath method (correct API)
        BLApproximationOptions approx_opts;
        approx_opts.flattenMode = BL_FLATTEN_MODE_DEFAULT;
        approx_opts.flattenTolerance = 0.2;
        BLResult result = cached.stroked_path.addStrokedPath(cached.original_path, stroke_options, approx_opts);

        if (result == BL_SUCCESS) {
            cached.is_valid = true;
            cached.UpdateLastUsed();
        } else {
            cached.is_valid = false;
            // Return original path as fallback
            cached.stroked_path = original_path;
        }

        // Cleanup if needed
        EvictLRU();

        return cached.stroked_path;
    }
    
    // Invalidate a specific cache entry
    void Invalidate(const PathCacheKey& key) {
        auto it = m_cache_.find(key);
        if (it != m_cache_.end()) {
            it->second.is_valid = false;
        }
    }
    
    // Clear all cache entries
    void Clear() {
        m_cache_.clear();
        m_cache_hits_ = 0;
        m_cache_misses_ = 0;
    }
    
    // Perform maintenance (cleanup expired entries)
    void Maintenance() {
        CleanupExpired();
        EvictLRU();
    }
    
    // Get cache statistics
    struct CacheStats {
        size_t total_entries;
        size_t cache_hits;
        size_t cache_misses;
        double hit_ratio;
    };
    
    CacheStats GetStats() const {
        CacheStats stats;
        stats.total_entries = m_cache_.size();
        stats.cache_hits = m_cache_hits_;
        stats.cache_misses = m_cache_misses_;
        
        size_t total_requests = m_cache_hits_ + m_cache_misses_;
        stats.hit_ratio = total_requests > 0 ? static_cast<double>(m_cache_hits_) / total_requests : 0.0;
        
        return stats;
    }
    
    // Generate cache key for trace elements
    static PathCacheKey CreateTraceKey(const std::string& trace_id, double thickness, 
                                      BLStrokeCap start_cap, BLStrokeCap end_cap) {
        PathCacheKey key;
        key.element_id = "trace_" + trace_id;
        key.thickness = thickness;
        key.start_cap = start_cap;
        key.end_cap = end_cap;
        key.transform_hash = 0;  // No transform for basic traces
        return key;
    }
    
    // Generate cache key for component elements
    static PathCacheKey CreateComponentKey(const std::string& component_id, const std::string& element_id,
                                          double thickness, uint32_t transform_hash = 0) {
        PathCacheKey key;
        key.element_id = "comp_" + component_id + "_" + element_id;
        key.thickness = thickness;
        key.start_cap = BL_STROKE_CAP_ROUND;
        key.end_cap = BL_STROKE_CAP_ROUND;
        key.transform_hash = transform_hash;
        return key;
    }
};

// Global path cache instance
extern BLPathCache g_path_cache;

} // namespace path_cache
