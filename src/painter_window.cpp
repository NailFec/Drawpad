// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#include "painter_window.h"

#include <QtCanvasPainter/qcanvaspainter.h>
#include <QtCanvasPainter/qcanvaspainterfactory.h>
#include <QtCanvasPainter/qcanvasrhipaintdriver.h>

#include <QColor>
#include <QExposeEvent>
#include <QPlatformSurfaceEvent>

#if QT_CONFIG(opengl)
#include <QOffscreenSurface>
#endif

namespace drawpad {

PainterWindow::PainterWindow(QRhi::Implementation api)
    : m_api(api)
{
    switch (api) {
    case QRhi::OpenGLES2:
        setSurfaceType(OpenGLSurface);
        break;
    case QRhi::Vulkan:
        setSurfaceType(VulkanSurface);
        break;
    default:
        break;
    }
}

PainterWindow::~PainterWindow()
{
    releaseSwapChain();
}

void PainterWindow::exposeEvent(QExposeEvent *)
{
    if (isExposed()) {
        m_notExposed = false;
        m_newlyExposed = true;
        if (!m_initialized)
            init();
        requestUpdate();
    } else {
        m_notExposed = true;
    }
}

bool PainterWindow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::UpdateRequest:
        render();
        return true;
    case QEvent::PlatformSurface:
        if (static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType()
            == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
            releaseSwapChain();
        break;
    default:
        break;
    }
    return QWindow::event(event);
}

void PainterWindow::init()
{
    if (m_api == QRhi::Vulkan) {
#if QT_CONFIG(vulkan)
        QRhiVulkanInitParams params;
        params.inst = vulkanInstance();
        params.window = this;
        m_rhi.reset(QRhi::create(QRhi::Vulkan, &params));
#endif
    } else if (m_api == QRhi::OpenGLES2) {
#if QT_CONFIG(opengl)
        m_fallbackSurface.reset(QRhiGles2InitParams::newFallbackSurface());
        QRhiGles2InitParams params;
        params.fallbackSurface = m_fallbackSurface.get();
        params.window = this;
        m_rhi.reset(QRhi::create(QRhi::OpenGLES2, &params));
#endif
    }
    if (!m_rhi)
        return;

    m_swapchain.reset(m_rhi->newSwapChain());
    m_depthStencil.reset(m_rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, QSize(), 1,
                                                QRhiRenderBuffer::UsedWithSwapChainOnly));
    m_swapchain->setWindow(this);
    m_swapchain->setDepthStencil(m_depthStencil.get());
    m_renderPass.reset(m_swapchain->newCompatibleRenderPassDescriptor());
    m_swapchain->setRenderPassDescriptor(m_renderPass.get());

    m_factory = std::make_unique<QCanvasPainterFactory>();
    m_factory->create(m_rhi.get());
    m_initialized = true;
    resizeSwapChain();
}

void PainterWindow::resizeSwapChain()
{
    m_hasSwapChain = m_swapchain && m_swapchain->createOrResize();
}

void PainterWindow::releaseSwapChain()
{
    m_hasSwapChain = false;
    m_initialized = false;
    if (m_factory)
        m_factory->destroy();
    m_factory.reset();
    m_renderPass.reset();
    m_depthStencil.reset();
    m_swapchain.reset();
    m_rhi.reset();
#if QT_CONFIG(opengl)
    m_fallbackSurface.reset();
#endif
}

void PainterWindow::render()
{
    if (!m_initialized || !m_factory || !m_factory->isValid() || !m_rhi || !m_swapchain)
        return;
    if (!m_hasSwapChain || m_notExposed || !isExposed())
        return;

    if (m_swapchain->currentPixelSize() != m_swapchain->surfacePixelSize() || m_newlyExposed) {
        resizeSwapChain();
        if (!m_hasSwapChain)
            return;
        m_newlyExposed = false;
    }

    QRhi::FrameOpResult result = m_rhi->beginFrame(m_swapchain.get());
    if (result == QRhi::FrameOpSwapChainOutOfDate) {
        resizeSwapChain();
        if (!m_hasSwapChain)
            return;
        result = m_rhi->beginFrame(m_swapchain.get());
    }
    if (result != QRhi::FrameOpSuccess) {
        requestUpdate();
        return;
    }

    QCanvasRhiPaintDriver *driver = m_factory->paintDriver();
    QCanvasPainter *painter = m_factory->painter();
    driver->resetForNewFrame();
    driver->beginPaint(m_swapchain->currentFrameCommandBuffer(), m_swapchain->currentFrameRenderTarget(),
                       QColor(255, 255, 255), size(), devicePixelRatio());
    paint(painter);
    driver->endPaint();
    m_rhi->endFrame(m_swapchain.get());
}

} // namespace drawpad
