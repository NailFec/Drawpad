// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#pragma once

#include "gesture_tracker.h"

#include <QObject>
#include <QVector>

#include <atomic>
#include <thread>

namespace drawpad {

class TouchpadGrabber : public QObject {
    Q_OBJECT

public:
    explicit TouchpadGrabber(QObject *parent = nullptr);
    ~TouchpadGrabber() override;

    void setGrabbed(bool grabbed);

signals:
    void contactsUpdated(const QVector<drawpad::Contact> &contacts);
    void touchpadsReady(int count, double width, double height);

private:
    void run();

    std::atomic<bool> m_stop {false};
    std::atomic<bool> m_grabbed {false};
    std::thread m_thread;
};

} // namespace drawpad
