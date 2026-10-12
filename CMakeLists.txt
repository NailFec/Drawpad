cmake_minimum_required(VERSION 3.16)

project(Drawpad VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)

if(NOT EXISTS "/usr/lib/cmake/Qt6CanvasPainter/Qt6CanvasPainterConfig.cmake"
    AND EXISTS "$ENV{HOME}/.cache/drawpad/qt6-canvaspainter/usr/lib/cmake/Qt6CanvasPainter/Qt6CanvasPainterConfig.cmake")
    set(Qt6CanvasPainter_DIR
        "$ENV{HOME}/.cache/drawpad/qt6-canvaspainter/usr/lib/cmake/Qt6CanvasPainter"
        CACHE PATH "Qt Canvas Painter package location")
endif()

find_package(Qt6 6.12 REQUIRED COMPONENTS Gui GuiPrivate Widgets Test CanvasPainter)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED IMPORTED_TARGET libevdev)
pkg_check_modules(LIBUDEV REQUIRED IMPORTED_TARGET libudev)

get_target_property(DRAWPAD_CANVAS_LIB Qt6::CanvasPainter IMPORTED_LOCATION_RELWITHDEBINFO)
if(NOT DRAWPAD_CANVAS_LIB)
    get_target_property(DRAWPAD_CANVAS_LIB Qt6::CanvasPainter IMPORTED_LOCATION)
endif()
if(DRAWPAD_CANVAS_LIB)
    get_filename_component(DRAWPAD_CANVAS_LIB_DIR "${DRAWPAD_CANVAS_LIB}" DIRECTORY)
endif()

add_library(drawpad_core STATIC
    src/geometry.cpp
    src/camera.cpp
    src/mapping.cpp
    src/gesture_tracker.cpp
    src/stroke_document.cpp
    src/document_file.cpp
)
target_include_directories(drawpad_core PUBLIC src)
target_link_libraries(drawpad_core PUBLIC Qt6::Gui)

qt_add_executable(appDrawpad
    src/main.cpp
    src/painter_window.cpp
    src/drawpad_window.cpp
    src/touchpad_grabber.cpp
)
target_link_libraries(appDrawpad PRIVATE
    drawpad_core
    Qt6::GuiPrivate
    Qt6::CanvasPainter
    Qt6::Widgets
    PkgConfig::LIBEVDEV
    PkgConfig::LIBUDEV
)
if(DRAWPAD_CANVAS_LIB_DIR)
    set_target_properties(appDrawpad PROPERTIES
        BUILD_RPATH "${DRAWPAD_CANVAS_LIB_DIR}"
        INSTALL_RPATH "${DRAWPAD_CANVAS_LIB_DIR}"
    )
endif()

enable_testing()
qt_add_executable(drawpad_tests tests/tst_drawpad.cpp)
target_link_libraries(drawpad_tests PRIVATE drawpad_core Qt6::Test)
add_test(NAME drawpad COMMAND drawpad_tests)

install(TARGETS appDrawpad
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
