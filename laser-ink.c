/*
 * laser-ink —— 白板风格的激光笔迹（X11 / ARGB 覆盖层）
 *
 * 跟随鼠标绘制带光晕的轨迹，按时间淡出消失；窗口点击穿透，不抓键。
 *
 * 用法:
 *   laser-ink [--trail 1500] [--radius 16] [--color '#ff2b2b']
 *             [--opacity 1.0] [--demo]
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#define MAXPTS 16384
#define CIRCLE_SIZE 128
#define ALPHA_STEPS 96

typedef struct { double x, y, t; } Pt;

static volatile sig_atomic_t running = 1;
static void on_sig(int s) { (void)s; running = 0; }

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void usage(void) {
    fprintf(stderr,
        "用法: laser-ink [--trail ms] [--radius px] [--color '#rrggbb'] "
        "[--opacity 0..1] [--demo]\n");
}

int main(int argc, char **argv) {
    int trail_ms = 600, radius = 4, demo = 0;
    double opacity = 1.0;
    unsigned int cr = 0xff, cg = 0x2b, cb = 0x2b;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--trail") && i + 1 < argc) trail_ms = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--radius") && i + 1 < argc) radius = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--opacity") && i + 1 < argc) opacity = atof(argv[++i]);
        else if (!strcmp(argv[i], "--demo")) demo = 1;
        else if (!strcmp(argv[i], "--color") && i + 1 < argc) {
            const char *c = argv[++i];
            if (c[0] == '#') c++;
            if (strlen(c) >= 6) {
                unsigned int v = (unsigned int)strtoul(c, NULL, 16);
                cr = (v >> 16) & 0xff; cg = (v >> 8) & 0xff; cb = v & 0xff;
            }
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(); return 0;
        } else { usage(); return 1; }
    }
    if (trail_ms < 100) trail_ms = 100;
    if (radius < 2) radius = 2;
    if (opacity < 0) opacity = 0;
    if (opacity > 1) opacity = 1;

    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);
    signal(SIGHUP, on_sig);

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "laser-ink: 无法连接 X server\n"); return 1; }

    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);
    int W = DisplayWidth(dpy, scr), H = DisplayHeight(dpy, scr);

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(dpy, scr, 32, TrueColor, &vinfo)) {
        fprintf(stderr, "laser-ink: 没有 32 位 ARGB visual，无法做淡出效果\n");
        XCloseDisplay(dpy);
        return 1;
    }

    XSetWindowAttributes attrs;
    attrs.colormap = XCreateColormap(dpy, root, vinfo.visual, AllocNone);
    attrs.background_pixel = 0;
    attrs.border_pixel = 0;
    attrs.background_pixmap = None;
    attrs.override_redirect = True;
    attrs.event_mask = ExposureMask;

    Window win = XCreateWindow(dpy, root, 0, 0, W, H, 0, vinfo.depth,
                               InputOutput, vinfo.visual,
                               CWColormap | CWBackPixel | CWBackPixmap |
                               CWBorderPixel | CWOverrideRedirect, &attrs);

    XStoreName(dpy, win, "laser-ink");
    XSelectInput(dpy, win, ExposureMask | StructureNotifyMask);

    /* 输入区域清空 => 点击穿透，不干扰任何操作 */
    XShapeCombineRectangles(dpy, win, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted);
    XMapRaised(dpy, win);

    Picture dst = XRenderCreatePicture(
        dpy, win, XRenderFindStandardFormat(dpy, PictStandardARGB32), 0, NULL);

    /* 径向渐变光斑（白热核心 -> 主色 -> 透明） */
    Pixmap cpx = XCreatePixmap(dpy, win, CIRCLE_SIZE, CIRCLE_SIZE, 32);
    Picture circle = XRenderCreatePicture(
        dpy, cpx, XRenderFindStandardFormat(dpy, PictStandardARGB32), 0, NULL);

    XRenderColor stops[4];
    stops[0].red = 0xffff; stops[0].green = 0xffff; stops[0].blue = 0xffff;
    stops[0].alpha = 0xffff;
    stops[1].red = cr * 257; stops[1].green = cg * 257; stops[1].blue = cb * 257;
    stops[1].alpha = 0xe800;
    stops[2].red = cr * 257; stops[2].green = cg * 257; stops[2].blue = cb * 257;
    stops[2].alpha = 0x2800;
    stops[3].red = cr * 257; stops[3].green = cg * 257; stops[3].blue = cb * 257;
    stops[3].alpha = 0x0000;

    XFixed half = (XFixed)((CIRCLE_SIZE / 2) << 16);
    XRadialGradient rg;
    rg.inner.x = half; rg.inner.y = half; rg.inner.radius = 0;
    rg.outer.x = half; rg.outer.y = half; rg.outer.radius = half;
    XFixed stop_pos[4] = {
        0, (XFixed)(0.40 * 65536), (XFixed)(0.72 * 65536), (XFixed)65536
    };
    Picture grad = XRenderCreateRadialGradient(dpy, &rg, stop_pos, stops, 4);
    XRenderComposite(dpy, PictOpSrc, grad, None, circle,
                     0, 0, 0, 0, 0, 0, CIRCLE_SIZE, CIRCLE_SIZE);
    XRenderFreePicture(dpy, grad);

    XRenderSetPictureFilter(dpy, circle, FilterBilinear, NULL, 0);

    /* 预生成不同透明度的蒙版，避免每帧反复创建 */
    Picture masks[ALPHA_STEPS];
    for (int i = 0; i < ALPHA_STEPS; i++) {
        XRenderColor mc = {0, 0, 0,
            (unsigned short)(65535.0 * (i + 1) / ALPHA_STEPS)};
        masks[i] = XRenderCreateSolidFill(dpy, &mc);
    }

    Pt pts[MAXPTS];
    int n = 0;
    double last_x = -1, last_y = -1, last_t = 0;
    double t0 = now_s();

    while (running) {
        double t = now_s();

        if (demo) {
            double ph = fmod((t - t0) * 2.2, 6.283185307);
            double dx = W * 0.30 + cos(ph) * W * 0.18;
            double dy = H * 0.45 + sin(ph) * H * 0.18;
            if (n >= MAXPTS - 64) n = 0;
            pts[n].x = dx; pts[n].y = dy; pts[n].t = t; n++;
        } else {
            Window r1, c1; int rx, ry, wx, wy; unsigned int mask;
            if (XQueryPointer(dpy, root, &r1, &c1, &rx, &ry, &wx, &wy, &mask)) {
                if (last_x < 0) {
                    last_x = rx; last_y = ry; last_t = t;
                }
                double dist = hypot(rx - last_x, ry - last_y);
                if (dist >= 1.0) {
                    /* 沿路径插值加密：每 ~2px 一个采样点，轨迹才平滑 */
                    double step = (radius > 10) ? 2.0 : 1.0;
                    int steps = (int)(dist / step) + 1;
                    for (int s = 1; s <= steps && n < MAXPTS; s++) {
                        double f = (double)s / steps;
                        pts[n].x = last_x + (rx - last_x) * f;
                        pts[n].y = last_y + (ry - last_y) * f;
                        pts[n].t = last_t + (t - last_t) * f;
                        n++;
                    }
                    last_x = rx; last_y = ry; last_t = t;
                }
            }
        }

        /* 丢弃过期点 */
        int keep = 0;
        double life = trail_ms / 1000.0;
        for (int i = 0; i < n; i++)
            if (t - pts[i].t <= life) pts[keep++] = pts[i];
        n = keep;

        /* 清屏 */
        XRenderColor clear = {0, 0, 0, 0};
        XRenderFillRectangle(dpy, PictOpSrc, dst, &clear, 0, 0, W, H);

        /* 画轨迹：越老越小越淡 */
        for (int i = 0; i < n; i++) {
            double age = t - pts[i].t;
            double k = 1.0 - age / life;
            if (k <= 0) continue;
            /* smoothstep：头尾都平滑，不突兀 */
            double a = (k * k * (3.0 - 2.0 * k)) * opacity;
            int idx = (int)(a * (ALPHA_STEPS - 1) + 0.5);
            if (idx < 0) idx = 0;
            if (idx >= ALPHA_STEPS) idx = ALPHA_STEPS - 1;
            /* 等宽：不再从粗到细，只在最后 15% 轻微收细 */
            int r = radius;
            if (k < 0.15) r = (int)(radius * (0.6 + 0.4 * (k / 0.15)));
            if (r < 1) r = 1;

            /* 把 128px 的渐变图缩放到 2r 大小 */
            double sc = (double)CIRCLE_SIZE / (double)(2 * r);
            XTransform tr = {{{XDoubleToFixed(sc), 0, 0},
                              {0, XDoubleToFixed(sc), 0},
                              {0, 0, XDoubleToFixed(1)}}};
            XRenderSetPictureTransform(dpy, circle, &tr);

            XRenderComposite(dpy, PictOpOver, circle, masks[idx], dst,
                             0, 0, 0, 0,
                             (int)pts[i].x - r, (int)pts[i].y - r,
                             2 * r, 2 * r);
        }

        XFlush(dpy);
        usleep(1000 * 11); /* ~90fps */
    }

    for (int i = 0; i < ALPHA_STEPS; i++) XRenderFreePicture(dpy, masks[i]);
    XRenderFreePicture(dpy, circle);
    XRenderFreePicture(dpy, dst);
    XFreePixmap(dpy, cpx);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
