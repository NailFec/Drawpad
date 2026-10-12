// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#pragma once

#include <QWindow>
#include <rhi/qrhi.h>

#include <memory>

// Qt enables Vulkan in QT_CONFIG when it was built with it, but QVulkanInstance
// and QRhiVulkanInitParams stay incomplete unless vulkan/vulkan.h is present.
#if QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
#define DRAWPAD_HAS_VULKAN 1
#else
#define DRAWPAD_HAS_VULKAN 0
#endif

class QCanvasPainter;
class QCanvasPainterFactory;
class QOffscreenSurface;

namespace drawpad {

class PainterWindow : public QWindow {
public:
    explicit PainterWindow(QRhi::Implementation api);
    ~PainterWindow() override;

protected:
    void exposeEvent(QExposeEvent *event) override;
    bool event(QEvent *event) override;
    virtual void paint(QCanvasPainter *painter) = 0;

private:
    void init();
    void render();
    void resizeSwapChain();
    void releaseSwapChain();

    QRhi::Implementation m_api;
    std::unique_ptr<QRhi> m_rhi;
    std::unique_ptr<QRhiSwapChain> m_swapchain;
    std::unique_ptr<QRhiRenderBuffer> m_depthStencil;
    std::unique_ptr<QRhiRenderPassDescriptor> m_renderPass;
    std::unique_ptr<QCanvasPainterFactory> m_factory;
#if QT_CONFIG(opengl)
    std::unique_ptr<QOffscreenSurface> m_fallbackSurface;
#endif
    bool m_initialized = false;
    bool m_hasSwapChain = false;
    bool m_notExposed = false;
    bool m_newlyExposed = false;
};

} // namespace drawpad
