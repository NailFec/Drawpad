// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#include "drawpad_window.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QScreen>
#include <QSurfaceFormat>

#if DRAWPAD_HAS_VULKAN
#include <QVulkanInstance>
#endif

namespace {

QRhi::Implementation defaultApi()
{
#if DRAWPAD_HAS_VULKAN
    return QRhi::Vulkan;
#elif QT_CONFIG(opengl)
    return QRhi::OpenGLES2;
#else
    return QRhi::Null;
#endif
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("NailFec"));
    application.setOrganizationDomain(QStringLiteral("nailfec.com"));
    application.setApplicationName(QStringLiteral("Drawpad"));
    application.setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption vulkanOption(QStringLiteral("vulkan"), QStringLiteral("Render with Vulkan"));
    const QCommandLineOption openGlOption(QStringLiteral("opengl"), QStringLiteral("Render with OpenGL"));
    parser.addOption(vulkanOption);
    parser.addOption(openGlOption);
    parser.process(application);

    QRhi::Implementation graphicsApi = defaultApi();
    if (parser.isSet(openGlOption))
        graphicsApi = QRhi::OpenGLES2;
    if (parser.isSet(vulkanOption))
        graphicsApi = QRhi::Vulkan;

#if QT_CONFIG(opengl)
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);
#endif

#if DRAWPAD_HAS_VULKAN
    QVulkanInstance vulkanInstance;
    if (graphicsApi == QRhi::Vulkan) {
        vulkanInstance.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
        if (!vulkanInstance.create()) {
#if QT_CONFIG(opengl)
            qWarning("Vulkan is not usable, falling back to OpenGL");
            graphicsApi = QRhi::OpenGLES2;
#else
            qWarning("Vulkan is not usable");
            return 1;
#endif
        }
    }
#elif QT_CONFIG(opengl)
    if (graphicsApi == QRhi::Vulkan) {
        qWarning("Vulkan headers are missing, falling back to OpenGL");
        graphicsApi = QRhi::OpenGLES2;
    }
#else
    if (graphicsApi == QRhi::Vulkan) {
        qWarning("Vulkan headers are missing");
        return 1;
    }
#endif

    drawpad::DrawpadWindow window(graphicsApi);
#if DRAWPAD_HAS_VULKAN
    if (graphicsApi == QRhi::Vulkan)
        window.setVulkanInstance(&vulkanInstance);
#endif
    if (QScreen *screen = application.primaryScreen())
        window.setScreen(screen);
    window.resize(1280, 800);
    window.show();
    return application.exec();
}
