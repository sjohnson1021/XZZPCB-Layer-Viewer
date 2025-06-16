#include "RenderContext.hpp"

#include <iostream>
#include <thread>

// Constructor
RenderContext::RenderContext() : m_image_width_(0), m_image_height_(0)
{
    // m_blContext is default constructed
    // m_targetImage is default constructed
    m_clear_color_[0] = m_clear_color_[1] = m_clear_color_[2] = 0.0f;
    m_clear_color_[3] = 0.0f;  // Transparent by default
}

RenderContext::~RenderContext()
{
    Shutdown();
}

// Adaptive thread count selection based on workload size
int GetOptimalThreadCount(int width, int height) {
    int pixels = width * height;
    int hw_threads = std::thread::hardware_concurrency();
    
    if (pixels < 250000) return 1;        // < 500x500
    if (pixels < 4000000) return 2;       // < 2000x2000
    return std::min(4, hw_threads);       // Large viewports
}

bool RenderContext::Initialize(int width, int height, int thread_count) {
    // Create a new BLImage with the specified dimensions
    m_target_image_ = BLImage(width, height, BL_FORMAT_PRGB32);
    
    // Check if image creation was successful
    if (m_target_image_.empty()) {
        std::cerr << "RenderContext: Failed to create BLImage" << std::endl;
        return false;
    }
    
    m_image_width_ = width;
    m_image_height_ = height;

    // Use adaptive thread count based on viewport size
    int optimal_threads = GetOptimalThreadCount(width, height);
    thread_count = (thread_count <= 0) ? optimal_threads : thread_count;

    // Configure Blend2D multithreading if requested
    if (thread_count > 1) {
        BLContextCreateInfo createInfo = {};
        createInfo.threadCount = static_cast<uint32_t>(thread_count);
        
        // Add gradient hints for better work distribution
        createInfo.flags = BL_CONTEXT_CREATE_FLAG_FALLBACK_TO_SYNC;

        BLResult err = m_bl_context_.begin(m_target_image_, createInfo);
        m_thread_count_ = thread_count;

        std::cout << "RenderContext: Initialized with " << thread_count 
                  << " threads for Blend2D async rendering" << std::endl;
    } else {
        // Single-threaded (synchronous) rendering
        BLResult err = m_bl_context_.begin(m_target_image_);
        m_thread_count_ = 1;

        std::cout << "RenderContext: Initialized with single-threaded (synchronous) rendering" << std::endl;
    }
    
    return true;
}

void RenderContext::Shutdown()
{
    if (m_bl_context_.isValid()) {  // Check if context is active before ending
        m_bl_context_.end();
    }
    m_target_image_.reset();  // Release the image data
    m_image_width_ = 0;
    m_image_height_ = 0;
    // std::cout << "RenderContext shutdown." << std::endl;
}

void RenderContext::BeginFrame()
{
    if (!m_bl_context_.isValid() || m_target_image_.empty()) {
        std::cerr << "RenderContext::BeginFrame Error: Context not initialized or image empty." << std::endl;
        return;
    }

    // Only clear if specifically requested
    if (m_clear_on_begin_frame_) {
        m_bl_context_.setCompOp(BL_COMP_OP_SRC_COPY);  // Ensure overwrite
        m_bl_context_.fillAll(BLRgba32(static_cast<uint8_t>(m_clear_color_[0] * 255.0f),
                                       static_cast<uint8_t>(m_clear_color_[1] * 255.0f),
                                       static_cast<uint8_t>(m_clear_color_[2] * 255.0f),
                                       static_cast<uint8_t>(m_clear_color_[3] * 255.0f)));
        m_bl_context_.setCompOp(BL_COMP_OP_SRC_OVER);  // Restore default
    }
}

void RenderContext::EndFrame()
{
    // For multithreaded contexts, we need to ensure all rendering is complete
    // before the frame is considered done
    if (IsMultithreaded()) {
        // Force synchronization to ensure all async operations complete
        FlushSync();
    } else {
        // For single-threaded contexts, this is typically a no-op
        // as rendering is synchronous
    }
}

BLContext& RenderContext::GetBlend2DContext()
{
    return m_bl_context_;
}

const BLImage& RenderContext::GetTargetImage() const
{
    return m_target_image_;
}

BLImage& RenderContext::GetTargetImage()
{
    return m_target_image_;
}

bool RenderContext::ResizeImage(int newWidth, int newHeight)
{
    if (newWidth <= 0 || newHeight <= 0) {
        std::cerr << "RenderContext::ResizeImage Error: Invalid new dimensions (" << newWidth << "x" << newHeight << ")." << std::endl;
        return false;
    }

    if (m_bl_context_.isValid()) {
        m_bl_context_.end();  // End context before resizing image
    }

    BLImage newImage;
    BLResult err = newImage.create(newWidth, newHeight, BL_FORMAT_PRGB32);
    if (err != BL_SUCCESS) {
        std::cerr << "RenderContext::ResizeImage Error: Failed to create new BLImage: " << err << std::endl;
        // Attempt to restart context on old image if it was valid
        if (!m_target_image_.empty()) {
            m_bl_context_.begin(m_target_image_);
        }
        return false;
    }

    m_target_image_ = newImage;  // Assign new image
    m_image_width_ = newWidth;
    m_image_height_ = newHeight;

    // Re-begin context on the new image with same threading configuration
    if (m_thread_count_ > 1) {
        BLContextCreateInfo createInfo = {};
        createInfo.threadCount = static_cast<uint32_t>(m_thread_count_);
        err = m_bl_context_.begin(m_target_image_, createInfo);
    } else {
        err = m_bl_context_.begin(m_target_image_);
    }

    if (err != BL_SUCCESS) {
        std::cerr << "RenderContext::ResizeImage Error: Failed to begin BLContext on new image: " << err << std::endl;
        m_target_image_.reset();  // Failed, so invalidate
        return false;
    }
    std::cout << "RenderContext image resized to " << newWidth << "x" << newHeight << std::endl;
    return true;
}

void RenderContext::OptimizeForStatic()
{
    // Use these settings when rendering static content
    m_bl_context_.setCompOp(BL_COMP_OP_SRC_OVER);
    m_bl_context_.setFillRule(BL_FILL_RULE_NON_ZERO);

    BLApproximationOptions precisionOptions = blDefaultApproximationOptions;
    precisionOptions.flattenTolerance = 0.1;   // Default is 0.3; smaller is more precise
    // Note: simplifyTolerance was removed from BLApproximationOptions in newer Blend2D versions
    m_bl_context_.setApproximationOptions(precisionOptions);
}

void RenderContext::OptimizeForInteractive()
{
    // Use these for dynamic/interactive content
    m_bl_context_.setCompOp(BL_COMP_OP_SRC_OVER);

    BLApproximationOptions speedOptions = blDefaultApproximationOptions;
    speedOptions.flattenTolerance = 0.5;   // Larger tolerance for speed
    // Note: simplifyTolerance was removed from BLApproximationOptions in newer Blend2D versions
    m_bl_context_.setApproximationOptions(speedOptions);
}

void RenderContext::SetBoardDataManager(std::shared_ptr<BoardDataManager> board_data_manager)
{
    m_board_data_manager_ = board_data_manager;
}

std::shared_ptr<BoardDataManager> RenderContext::GetBoardDataManager() const
{
    return m_board_data_manager_;
}

void RenderContext::FlushAsync()
{
    // Flush without forcing synchronization - let Blend2D handle async rendering
    if (m_bl_context_.isValid()) {
        m_bl_context_.flush(BL_CONTEXT_FLUSH_NO_FLAGS);
    }
}

void RenderContext::FlushSync()
{
    // Flush and wait for completion - forces synchronization
    if (m_bl_context_.isValid()) {
        m_bl_context_.flush(BL_CONTEXT_FLUSH_SYNC);
    }
}
