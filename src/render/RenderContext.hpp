#pragma once

#include <blend2d.h>  // For BLImage and BLContext

#include "core/BoardDataManager.hpp"

// Forward declarations (if any become necessary)
// struct SDL_Window; // No longer needed directly by RenderContext

class RenderContext
{
public:
    RenderContext();
    ~RenderContext();

    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;
    RenderContext(RenderContext&&) = delete;
    RenderContext& operator=(RenderContext&&) = delete;

    // Initialize with a default size, possibly configurable later
    bool Initialize(int width, int height);
    void Shutdown();

    // Frame operations for the Blend2D context
    void BeginFrame();  // Clears the BLImage
    void EndFrame();    // Finalizes BLContext operations if any (might be a no-op)

    // Accessors for Blend2D resources
    BLContext& GetBlend2DContext();
    [[nodiscard]] const BLImage& GetTargetImage() const;  // Read-only access to the image
    BLImage& GetTargetImage();                            // Writable access if needed, e.g., for direct manipulation or resizing

    // Add public getters for image dimensions
    [[nodiscard]] int GetImageWidth() const { return m_image_width_; }
    [[nodiscard]] int GetImageHeight() const { return m_image_height_; }

    // Potentially a method to resize the off-screen image
    bool ResizeImage(int new_width, int new_height);

    // Set the clear color for BeginFrame
    void SetClearColor(float r, float g, float b, float a = 1.0F)
    {
        m_clear_color_[0] = r;
        m_clear_color_[1] = g;
        m_clear_color_[2] = b;
        m_clear_color_[3] = a;
    }

    // Control whether BeginFrame does a full clear
    void SetClearOnBeginFrame(bool should_clear) { m_clear_on_begin_frame_ = should_clear; }

    // Performance optimization methods
    void OptimizeForStatic();
    void OptimizeForInteractive();

    // BoardDataManager access
    void SetBoardDataManager(std::shared_ptr<BoardDataManager> board_data_manager);
    [[nodiscard]] std::shared_ptr<BoardDataManager> GetBoardDataManager() const;

private:
    // Blend2D resources
    BLImage m_target_image_;  // The off-screen image for PCB rendering
    BLContext m_bl_context_;  // Blend2D rendering context targeting m_targetImage
    // No longer managing SDL_Window* or SDL_Renderer*
    // SDL_Window* m_window = nullptr;
    // SDL_Renderer* m_renderer = nullptr;

    int m_image_width_ = 0;
    int m_image_height_ = 0;
    float m_clear_color_[4] = {0.0F, 0.0F, 0.0F, 0.0F};  // Default clear color (transparent black)
    bool m_clear_on_begin_frame_ = true;                 // Whether to clear on BeginFrame
    std::shared_ptr<BoardDataManager> m_board_data_manager_;
};
