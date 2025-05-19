#include "render/RenderContext.hpp"
#include <SDL3/SDL.h>
#include <stdexcept> // For std::runtime_error or logging errors
#include <iostream>  // For error logging

// Constructor
RenderContext::RenderContext()
    : m_imageWidth(0), m_imageHeight(0) {
    // m_blContext is default constructed
    // m_targetImage is default constructed
    m_clearColor[0] = m_clearColor[1] = m_clearColor[2] = 0.0f;
    m_clearColor[3] = 0.0f; // Transparent by default
}

RenderContext::~RenderContext() {
    Shutdown();
}

bool RenderContext::Initialize(int width, int height) {
    if (width <= 0 || height <= 0) {
        std::cerr << "RenderContext::Initialize Error: Invalid image dimensions (" 
                  << width << "x" << height << ")." << std::endl;
        return false;
    }

    BLResult err = m_targetImage.create(width, height, BL_FORMAT_PRGB32);
    if (err != BL_SUCCESS) {
        std::cerr << "RenderContext::Initialize Error: Failed to create BLImage: " << err << std::endl;
        return false;
    }
    m_imageWidth = width;
    m_imageHeight = height;

    err = m_blContext.begin(m_targetImage);
    if (err != BL_SUCCESS) {
        std::cerr << "RenderContext::Initialize Error: Failed to begin BLContext on BLImage: " << err << std::endl;
        m_targetImage.reset(); // Release the image if context creation failed
        return false;
    }

    // Optional: Set default context properties if needed
    // m_blContext.setCompOp(BL_COMP_OP_SRC_COPY);
    // m_blContext.fillAll(BLRgba32(0, 0, 0, 0)); // Example: clear to transparent black

    std::cout << "RenderContext initialized with Blend2D for a " << width << "x" << height << " image." << std::endl;
    return true;
}

void RenderContext::Shutdown() {
    if (m_blContext.isValid()) { // Check if context is active before ending
        m_blContext.end();
    }
    m_targetImage.reset(); // Release the image data
    m_imageWidth = 0;
    m_imageHeight = 0;
    // std::cout << "RenderContext shutdown." << std::endl;
}

void RenderContext::BeginFrame() {
    if (!m_blContext.isValid() || m_targetImage.empty()) {
        std::cerr << "RenderContext::BeginFrame Error: Context not initialized or image empty." << std::endl;
        return;
    }

    // Only clear if specifically requested
    if (m_clearOnBeginFrame) {
        m_blContext.setCompOp(BL_COMP_OP_SRC_COPY); // Ensure overwrite
        m_blContext.fillAll(BLRgba32(
            static_cast<uint8_t>(m_clearColor[0] * 255.0f),
            static_cast<uint8_t>(m_clearColor[1] * 255.0f),
            static_cast<uint8_t>(m_clearColor[2] * 255.0f),
            static_cast<uint8_t>(m_clearColor[3] * 255.0f)
        ));
    }
}

void RenderContext::EndFrame() {
    // For this model, EndFrame might be a no-op as PcbRenderer will grab the image data.
    // If there were any batched operations or explicit flushing needed for BLContext, it would go here.
    // m_blContext.flush(BL_CONTEXT_FLUSH_SYNC); // Example, if needed
}

BLContext& RenderContext::GetBlend2DContext() {
    return m_blContext;
}

const BLImage& RenderContext::GetTargetImage() const {
    return m_targetImage;
}

BLImage& RenderContext::GetTargetImage() {
    return m_targetImage;
}

bool RenderContext::ResizeImage(int newWidth, int newHeight) {
    if (newWidth <= 0 || newHeight <= 0) {
        std::cerr << "RenderContext::ResizeImage Error: Invalid new dimensions (" 
                  << newWidth << "x" << newHeight << ")." << std::endl;
        return false;
    }

    if (m_blContext.isValid()) {
        m_blContext.end(); // End context before resizing image
    }

    BLImage newImage;
    BLResult err = newImage.create(newWidth, newHeight, BL_FORMAT_PRGB32);
    if (err != BL_SUCCESS) {
        std::cerr << "RenderContext::ResizeImage Error: Failed to create new BLImage: " << err << std::endl;
        // Attempt to restart context on old image if it was valid
        if (!m_targetImage.empty()) {
            m_blContext.begin(m_targetImage);
        }
        return false;
    }

    m_targetImage = newImage; // Assign new image
    m_imageWidth = newWidth;
    m_imageHeight = newHeight;

    err = m_blContext.begin(m_targetImage); // Re-begin context on the new image
    if (err != BL_SUCCESS) {
        std::cerr << "RenderContext::ResizeImage Error: Failed to begin BLContext on new image: " << err << std::endl;
        m_targetImage.reset(); // Failed, so invalidate
        return false;
    }
    std::cout << "RenderContext image resized to " << newWidth << "x" << newHeight << std::endl;
    return true;
}

void RenderContext::OptimizeForStatic() {
    // Use these settings when rendering static content
    m_blContext.setCompOp(BL_COMP_OP_SRC_OVER);
    m_blContext.setFillRule(BL_FILL_RULE_NON_ZERO);

    BLApproximationOptions precisionOptions = blDefaultApproximationOptions;
    precisionOptions.flattenTolerance = 0.1; // Default is 0.3; smaller is more precise
    precisionOptions.simplifyTolerance = 0.0; // Default is 0.05; 0 means no/less simplification
    m_blContext.setApproximationOptions(precisionOptions);
}

void RenderContext::OptimizeForInteractive() {
    // Use these for dynamic/interactive content
    m_blContext.setCompOp(BL_COMP_OP_SRC_OVER);

    BLApproximationOptions speedOptions = blDefaultApproximationOptions;
    speedOptions.flattenTolerance = 0.5; // Larger tolerance for speed
    speedOptions.simplifyTolerance = 0.2; // More simplification for speed
    m_blContext.setApproximationOptions(speedOptions);
} 