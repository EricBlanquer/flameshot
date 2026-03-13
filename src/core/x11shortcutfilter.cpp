// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "x11shortcutfilter.h"
#include <QImage>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

X11ShortcutWorker::X11ShortcutWorker(QObject* parent)
  : QObject(parent)
  , m_running(false)
{}

void X11ShortcutWorker::stop()
{
    m_running = false;
}

void X11ShortcutWorker::run()
{
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        return;
    }
    int printKeycode = XKeysymToKeycode(dpy, XK_Print);
    if (printKeycode == 0) {
        XCloseDisplay(dpy);
        return;
    }
    Window root = DefaultRootWindow(dpy);
    XGrabKey(dpy,
             printKeycode,
             AnyModifier,
             root,
             False,
             GrabModeAsync,
             GrabModeAsync);
    XFlush(dpy);
    m_running = true;
    while (m_running) {
        while (XPending(dpy) > 0) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            if (xev.type == KeyPress && xev.xkey.keycode == printKeycode) {
                XWindowAttributes attrs;
                XGetWindowAttributes(dpy, root, &attrs);
                int w = attrs.width;
                int h = attrs.height;
                XImage* ximg =
                  XGetImage(dpy, root, 0, 0, w, h, AllPlanes, ZPixmap);
                if (ximg) {
                    QImage img(w, h, QImage::Format_RGB32);
                    for (int y = 0; y < h; ++y) {
                        for (int x = 0; x < w; ++x) {
                            unsigned long pixel = XGetPixel(ximg, x, y);
                            img.setPixel(
                              x, y, (0xFF << 24) | (pixel & 0xFFFFFF));
                        }
                    }
                    XDestroyImage(ximg);
                    emit screenshotReady(img);
                }
            }
        }
        QThread::msleep(5);
    }
    XUngrabKey(dpy, printKeycode, AnyModifier, root);
    XCloseDisplay(dpy);
}

X11ShortcutFilter::X11ShortcutFilter(QObject* parent)
  : QObject(parent)
  , m_worker(new X11ShortcutWorker())
{
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::started, m_worker, &X11ShortcutWorker::run);
    connect(m_worker,
            &X11ShortcutWorker::screenshotReady,
            this,
            &X11ShortcutFilter::screenshotReady,
            Qt::QueuedConnection);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.start();
}

X11ShortcutFilter::~X11ShortcutFilter()
{
    m_worker->stop();
    m_thread.quit();
    m_thread.wait(2000);
}
