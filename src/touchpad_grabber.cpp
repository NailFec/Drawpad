// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#include "touchpad_grabber.h"

#include <QCoreApplication>

#include <libevdev/libevdev.h>
#include <libudev.h>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <vector>

namespace drawpad {
namespace {

constexpr int kSlots = 16;

double axisLength(const input_absinfo *info)
{
    if (!info || info->maximum <= info->minimum)
        return 1;
    const double span = static_cast<double>(info->maximum - info->minimum);
    if (info->resolution > 0)
        return span / static_cast<double>(info->resolution);
    return span;
}

double normalize(int value, int minimum, int maximum)
{
    if (maximum <= minimum)
        return 0;
    return std::clamp((value - minimum) / static_cast<double>(maximum - minimum), 0.0, 1.0);
}

struct Slot {
    int id = -1;
    int x = 0;
    int y = 0;
};

struct Device {
    int fd = -1;
    libevdev *dev = nullptr;
    int minX = 0;
    int maxX = 1;
    int minY = 0;
    int maxY = 1;
    double width = 1;
    double height = 1;
    bool multitouch = false;
    int slot = 0;
    bool fingerDown = false;
    int absX = 0;
    int absY = 0;
    std::array<Slot, kSlots> slotState {};

    ~Device()
    {
        if (dev) {
            libevdev_grab(dev, LIBEVDEV_UNGRAB);
            libevdev_free(dev);
        }
        if (fd >= 0)
            close(fd);
    }

    Device() = default;
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;
    Device(Device &&other) noexcept
        : fd(other.fd)
        , dev(other.dev)
        , minX(other.minX)
        , maxX(other.maxX)
        , minY(other.minY)
        , maxY(other.maxY)
        , width(other.width)
        , height(other.height)
        , multitouch(other.multitouch)
        , slot(other.slot)
        , fingerDown(other.fingerDown)
        , absX(other.absX)
        , absY(other.absY)
        , slotState(other.slotState)
    {
        other.fd = -1;
        other.dev = nullptr;
    }
    Device &operator=(Device &&) = delete;
};

void handleEvent(Device &device, const input_event &event)
{
    if (event.type == EV_ABS && device.multitouch) {
        if (event.code == ABS_MT_SLOT)
            device.slot = std::clamp(event.value, 0, kSlots - 1);
        else if (event.code == ABS_MT_TRACKING_ID)
            device.slotState[static_cast<size_t>(device.slot)].id = event.value;
        else if (event.code == ABS_MT_POSITION_X)
            device.slotState[static_cast<size_t>(device.slot)].x = event.value;
        else if (event.code == ABS_MT_POSITION_Y)
            device.slotState[static_cast<size_t>(device.slot)].y = event.value;
        return;
    }
    if (event.type == EV_ABS && !device.multitouch) {
        if (event.code == ABS_X)
            device.absX = event.value;
        else if (event.code == ABS_Y)
            device.absY = event.value;
        return;
    }
    if (event.type == EV_KEY && (event.code == BTN_TOUCH || event.code == BTN_TOOL_FINGER))
        device.fingerDown = event.value != 0;
}

bool drain(Device &device)
{
    bool reported = false;
    input_event event {};
    int status = LIBEVDEV_READ_STATUS_SUCCESS;
    while ((status = libevdev_next_event(device.dev, LIBEVDEV_READ_FLAG_NORMAL, &event))
           == LIBEVDEV_READ_STATUS_SUCCESS
           || status == LIBEVDEV_READ_STATUS_SYNC) {
        handleEvent(device, event);
        if (event.type == EV_SYN && event.code == SYN_REPORT)
            reported = true;
        if (status == LIBEVDEV_READ_STATUS_SYNC) {
            while ((status = libevdev_next_event(device.dev, LIBEVDEV_READ_FLAG_SYNC, &event))
                   == LIBEVDEV_READ_STATUS_SYNC) {
                handleEvent(device, event);
                if (event.type == EV_SYN && event.code == SYN_REPORT)
                    reported = true;
            }
            if (status == LIBEVDEV_READ_STATUS_SUCCESS) {
                handleEvent(device, event);
                if (event.type == EV_SYN && event.code == SYN_REPORT)
                    reported = true;
            }
        }
        if (status < 0 && status != -EAGAIN)
            break;
    }
    return reported;
}

QVector<Contact> contactsFrom(const std::vector<Device> &devices)
{
    QVector<Contact> contacts;
    for (size_t deviceIndex = 0; deviceIndex < devices.size(); ++deviceIndex) {
        const Device &device = devices[deviceIndex];
        if (device.multitouch) {
            for (int slot = 0; slot < kSlots; ++slot) {
                const Slot &state = device.slotState[static_cast<size_t>(slot)];
                if (state.id < 0)
                    continue;
                contacts.push_back(Contact {static_cast<int>(deviceIndex) * kSlots + slot,
                                            normalize(state.x, device.minX, device.maxX),
                                            normalize(state.y, device.minY, device.maxY)});
            }
        } else if (device.fingerDown) {
            contacts.push_back(Contact {static_cast<int>(deviceIndex) * kSlots,
                                        normalize(device.absX, device.minX, device.maxX),
                                        normalize(device.absY, device.minY, device.maxY)});
        }
    }
    return contacts;
}

std::vector<Device> openTouchpads(double &width, double &height)
{
    std::vector<Device> devices;
    udev *context = udev_new();
    if (!context)
        return devices;
    udev_enumerate *enumerate = udev_enumerate_new(context);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_add_match_property(enumerate, "ID_INPUT_TOUCHPAD", "1");
    udev_enumerate_scan_devices(enumerate);

    double largestArea = 0;
    width = 0;
    height = 0;
    udev_list_entry *entries = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry *entry = nullptr;
    udev_list_entry_foreach(entry, entries) {
        udev_device *udevDevice = udev_device_new_from_syspath(context, udev_list_entry_get_name(entry));
        const char *node = udevDevice ? udev_device_get_devnode(udevDevice) : nullptr;
        if (!node) {
            if (udevDevice)
                udev_device_unref(udevDevice);
            continue;
        }
        const int fd = open(node, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            udev_device_unref(udevDevice);
            continue;
        }
        libevdev *evdev = nullptr;
        if (libevdev_new_from_fd(fd, &evdev) < 0) {
            close(fd);
            udev_device_unref(udevDevice);
            continue;
        }
        const input_absinfo *axisX = libevdev_get_abs_info(evdev, ABS_MT_POSITION_X);
        const input_absinfo *axisY = libevdev_get_abs_info(evdev, ABS_MT_POSITION_Y);
        const bool multitouch = axisX && axisY;
        if (!multitouch) {
            axisX = libevdev_get_abs_info(evdev, ABS_X);
            axisY = libevdev_get_abs_info(evdev, ABS_Y);
        }
        if (!axisX || !axisY) {
            libevdev_free(evdev);
            close(fd);
            udev_device_unref(udevDevice);
            continue;
        }

        Device device;
        device.fd = fd;
        device.dev = evdev;
        device.multitouch = multitouch;
        device.minX = axisX->minimum;
        device.maxX = axisX->maximum;
        device.minY = axisY->minimum;
        device.maxY = axisY->maximum;
        device.width = axisLength(axisX);
        device.height = axisLength(axisY);
        const double area = device.width * device.height;
        if (area > largestArea) {
            largestArea = area;
            width = device.width;
            height = device.height;
        }
        devices.push_back(std::move(device));
        udev_device_unref(udevDevice);
    }
    udev_enumerate_unref(enumerate);
    udev_unref(context);
    return devices;
}

} // namespace

TouchpadGrabber::TouchpadGrabber(QObject *parent)
    : QObject(parent)
{
    m_thread = std::thread([this] { run(); });
}

TouchpadGrabber::~TouchpadGrabber()
{
    m_stop.store(true);
    if (m_thread.joinable())
        m_thread.join();
    QCoreApplication::removePostedEvents(this);
}

void TouchpadGrabber::setGrabbed(bool grabbed)
{
    m_grabbed.store(grabbed);
}

void TouchpadGrabber::run()
{
    double padWidth = 0;
    double padHeight = 0;
    std::vector<Device> devices = openTouchpads(padWidth, padHeight);
    const int count = static_cast<int>(devices.size());
    QMetaObject::invokeMethod(this, [this, count, padWidth, padHeight] {
        emit touchpadsReady(count, padWidth, padHeight);
    }, Qt::QueuedConnection);

    bool applied = false;
    while (!m_stop.load()) {
        const bool wantGrab = m_grabbed.load();
        if (wantGrab != applied) {
            for (Device &device : devices)
                libevdev_grab(device.dev, wantGrab ? LIBEVDEV_GRAB : LIBEVDEV_UNGRAB);
            applied = wantGrab;
        }
        if (devices.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        std::vector<pollfd> polls(devices.size());
        for (size_t index = 0; index < devices.size(); ++index) {
            polls[index].fd = devices[index].fd;
            polls[index].events = POLLIN;
            polls[index].revents = 0;
        }
        poll(polls.data(), static_cast<nfds_t>(polls.size()), 50);

        bool reported = false;
        for (size_t index = 0; index < devices.size(); ++index) {
            if (polls[index].revents & POLLIN)
                reported = drain(devices[index]) || reported;
        }
        if (!reported)
            continue;

        const QVector<Contact> contacts = contactsFrom(devices);
        QMetaObject::invokeMethod(this, [this, contacts] {
            emit contactsUpdated(contacts);
        }, Qt::QueuedConnection);
    }
}

} // namespace drawpad
