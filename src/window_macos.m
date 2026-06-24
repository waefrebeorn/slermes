/*
 * window_macos.m — macOS backend for window.h
 *
 * Implements the cross-platform window.h API using Cocoa + OpenGL.
 * Provides: window creation, event processing, OpenGL rendering context,
 * 2D drawing primitives, clipboard, drag-and-drop, HiDPI support,
 * tray icon, hotkeys, deep linking, terminal hyperlinks.
 */

#ifndef __APPLE__
#error "window_macos.m requires macOS (APPLE)"
#endif

#include "window.h"
#include <Cocoa/Cocoa.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── Internal types ───────────────────────────────────────────────────── */

typedef struct {
    char title[WINDOW_MAX_TITLE];
    int width, height;
    bool resizable;
    bool fullscreen;
    bool borderless;
    bool centered;
    int min_width, min_height;
} macos_config_t;

struct window {
    NSWindow *nswin;
    NSView *nsview;
    NSOpenGLContext *gl_ctx;
    macos_config_t config;
    bool should_close;
    bool focused;
    bool mouse_buttons[3];
    int mouse_x, mouse_y;
    float scale_factor;
    window_cursor_t current_cursor;
    char clipboard_text[WINDOW_MAX_TEXT_INPUT];
    bool dnd_enabled;
    /* Key state */
    bool keys[WINDOW_KEY_COUNT];
    char text_input[WINDOW_MAX_TEXT_INPUT];
    size_t text_len;
    /* Event queue */
    window_event_t event_queue[64];
    int event_head, event_tail;
};

/* ── HermesView: NSView subclass with OpenGL ───────────────────────────── */

@interface HermesView : NSView
@property (nonatomic, assign) window_t *window;
@property (nonatomic, strong) NSOpenGLContext *glContext;
@end

@implementation HermesView

- (instancetype)initWithFrame:(NSRect)frame window:(window_t *)w {
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersionLegacy,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAStencilSize, 8,
        NSOpenGLPFAAccelerated,
        0
    };
    NSOpenGLPixelFormat *fmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    self = [super initWithFrame:frame pixelFormat:fmt];
    if (self) {
        _window = w;
        _glContext = [[NSOpenGLContext alloc] initWithFormat:fmt shareContext:nil];
        [_glContext makeCurrentContext];
        [self setWantsBestResolutionOpenGLSurface:YES];
    }
    return self;
}

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    if (!_window) return;
    window_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_EXPOSE;
    /* Push expose event */
    int next = (_window->event_head + 1) % 64;
    if (next != _window->event_tail) {
        _window->event_queue[_window->event_head] = ev;
        _window->event_head = next;
    }
}

- (BOOL)acceptsFirstResponder { return YES; }

- (void)keyDown:(NSEvent *)event {
    if (!_window) return;
    window_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_KEY_DOWN;
    ev.key = (window_key_t)[event keyCode];
    ev.mods = 0;
    if (event.modifierFlags & NSEventModifierFlagShift)   ev.mods |= MOD_SHIFT;
    if (event.modifierFlags & NSEventModifierFlagControl)  ev.mods |= MOD_CTRL;
    if (event.modifierFlags & NSEventModifierFlagOption)   ev.mods |= MOD_ALT;
    if (event.modifierFlags & NSEventModifierFlagCommand)  ev.mods |= MOD_SUPER;
    _window->keys[ev.key] = true;
    int next = (_window->event_head + 1) % 64;
    if (next != _window->event_tail) {
        _window->event_queue[_window->event_head] = ev;
        _window->event_head = next;
    }
    /* Text input */
    NSString *chars = [event characters];
    if (chars.length > 0) {
        window_event_t tev;
        memset(&tev, 0, sizeof(tev));
        tev.type = EVENT_TEXT_INPUT;
        strncpy(_window->text_input, [chars UTF8String], WINDOW_MAX_TEXT_INPUT - 1);
        tev.text = _window->text_input;
        tev.text_len = strlen(_window->text_input);
        next = (_window->event_head + 1) % 64;
        if (next != _window->event_tail) {
            _window->event_queue[_window->event_head] = tev;
            _window->event_head = next;
        }
    }
}

- (void)keyUp:(NSEvent *)event {
    if (!_window) return;
    window_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_KEY_UP;
    ev.key = (window_key_t)[event keyCode];
    ev.mods = 0;
    if (event.modifierFlags & NSEventModifierFlagShift)   ev.mods |= MOD_SHIFT;
    if (event.modifierFlags & NSEventModifierFlagControl)  ev.mods |= MOD_CTRL;
    if (event.modifierFlags & NSEventModifierFlagOption)   ev.mods |= MOD_ALT;
    if (event.modifierFlags & NSEventModifierFlagCommand)  ev.mods |= MOD_SUPER;
    _window->keys[ev.key] = false;
    int next = (_window->event_head + 1) % 64;
    if (next != _window->event_tail) {
        _window->event_queue[_window->event_head] = ev;
        _window->event_head = next;
    }
}

- (void)mouseDown:(NSEvent *)event {
    if (!_window) return;
    NSPoint pt = [self convertPoint:[event locationInWindow] fromView:nil];
    window_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_MOUSE_DOWN;
    ev.button = MOUSE_LEFT;
    ev.x = (int)pt.x;
    ev.y = (int)(self.frame.size.height - pt.y);
    ev.mods = 0;
    _window->mouse_buttons[0] = true;
    int next = (_window->event_head + 1) % 64;
    if (next != _window->event_tail) {
        _window->event_queue[_window->event_head] = ev;
        _window->event_head = next;
    }
}

- (void)mouseUp:(NSEvent *)event {
    if (!_window) return;
    NSPoint pt = [self convertPoint:[event locationInWindow] fromView:nil];
    window_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_MOUSE_UP;
    ev.button = MOUSE_LEFT;
    ev.x = (int)pt.x;
    ev.y = (int)(self.frame.size.height - pt.y);
    ev.mods = 0;
    _window->mouse_buttons[0] = false;
    int next = (_window->event_head + 1) % 64;
    if (next != _window->event_tail) {
        _window->event_queue[_window->event_head] = ev;
        _window->event_head = next;
    }
}

- (void)rightMouseDown:(NSEvent *)event {
    if (!_window) return;
    NSPoint pt = [self convertPoint:[event locationInWindow] fromView:nil];
    window_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_MOUSE_DOWN;
    ev.button = MOUSE_RIGHT;
    ev.x = (int)pt.x;
    ev.y = (int)(self.frame.size.height - pt.y);
    _window->mouse_buttons[2] = true;
    int next = (_window->event_head + 1) % 64;
    if (next != _window->event_tail) {
        _window->event_queue[_window->event_head] = ev;
        _window->event_head = next;
    }
}

- (void)rightMouseUp:(NSEvent *)event {
    if (!_window) return;
    NSPoint pt = [self convertPoint:[event locationInWindow] fromView:nil];
    window_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_MOUSE_UP;
    ev.button = MOUSE_RIGHT;
    ev.x = (int)pt.x;
    ev.y = (int)(self.frame.size.height - pt.y);
    _window->mouse_buttons[2] = false;
    int next = (_window->event_head + 1) % 64;
    if (next != _window->event_tail) {
        _window->event_queue[_window->event_head] = ev;
        _window->event_head = next;
    }
}

- (void)mouseMoved:(NSEvent *)event {
    if (!_window) return;
    NSPoint pt = [self convertPoint:[event locationInWindow] fromView:nil];
    window_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_MOUSE_MOVE;
    ev.x = (int)pt.x;
    ev.y = (int)(self.frame.size.height - pt.y);
    _window->mouse_x = ev.x;
    _window->mouse_y = ev.y;
    int next = (_window->event_head + 1) % 64;
    if (next != _window->event_tail) {
        _window->event_queue[_window->event_head] = ev;
        _window->event_head = next;
    }
}

- (void)scrollWheel:(NSEvent *)event {
    if (!_window) return;
    window_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_MOUSE_SCROLL;
    ev.scroll_dy = (float)[event scrollingDeltaY];
    ev.scroll_dx = (float)[event scrollingDeltaX];
    NSPoint pt = [self convertPoint:[event locationInWindow] fromView:nil];
    ev.x = (int)pt.x;
    ev.y = (int)(self.frame.size.height - pt.y);
    int next = (_window->event_head + 1) % 64;
    if (next != _window->event_tail) {
        _window->event_queue[_window->event_head] = ev;
        _window->event_head = next;
    }
}

@end

/* ── HermesWindow: NSWindow subclass ───────────────────────────────────── */

@interface HermesWindow : NSWindow
@property (nonatomic, assign) window_t *window;
@end

@implementation HermesWindow

- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }

- (void)windowDidBecomeKey:(NSNotification *)notification {
    (void)notification;
    if (!_window) return;
    _window->focused = true;
    window_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_FOCUS_IN;
    int next = (_window->event_head + 1) % 64;
    if (next != _window->event_tail) {
        _window->event_queue[_window->event_head] = ev;
        _window->event_head = next;
    }
}

- (void)windowDidResignKey:(NSNotification *)notification {
    (void)notification;
    if (!_window) return;
    _window->focused = false;
    window_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_FOCUS_OUT;
    int next = (_window->event_head + 1) % 64;
    if (next != _window->event_tail) {
        _window->event_queue[_window->event_head] = ev;
        _window->event_head = next;
    }
}

- (BOOL)windowShouldClose:(id)sender {
    (void)sender;
    if (!_window) return YES;
    _window->should_close = true;
    window_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_CLOSE;
    int next = (_window->event_head + 1) % 64;
    if (next != _window->event_tail) {
        _window->event_queue[_window->event_head] = ev;
        _window->event_head = next;
    }
    return NO; /* We handle destruction explicitly */
}

@end

/* ── Static state ─────────────────────────────────────────────────────── */

static bool s_app_initialized = false;
static int s_window_count = 0;

/* ── Event queue ───────────────────────────────────────────────────────── */

static void push_event(window_t *w, const window_event_t *ev) {
    int next = (w->event_head + 1) % 64;
    if (next != w->event_tail) {
        w->event_queue[w->event_head] = *ev;
        w->event_head = next;
    }
}

static bool pop_event(window_t *w, window_event_t *ev) {
    if (w->event_head == w->event_tail) return false;
    *ev = w->event_queue[w->event_tail];
    w->event_tail = (w->event_tail + 1) % 64;
    return true;
}

/* ── Window lifecycle ─────────────────────────────────────────────────── */

/* PoP: window_create @ apps/desktop/src/app/window/index.tsx */
window_t *window_create(const window_config_t *config) {
    if (!s_app_initialized) {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        s_app_initialized = true;
    }

    window_t *w = (window_t *)calloc(1, sizeof(window_t));
    if (!w) return NULL;

    int win_w = config->width > 0 ? config->width : 1280;
    int win_h = config->height > 0 ? config->height : 900;
    w->config.width = win_w;
    w->config.height = win_h;
    w->config.resizable = config->resizable;
    w->config.fullscreen = config->fullscreen;
    w->config.borderless = config->borderless;
    w->config.centered = config->centered;
    w->config.min_width = config->min_width > 0 ? config->min_width : 320;
    w->config.min_height = config->min_height > 0 ? config->min_height : 240;
    strncpy(w->config.title, config->title ? config->title : "Hermes", WINDOW_MAX_TITLE - 1);

    /* HiDPI scale factor */
    w->scale_factor = 1.0f;
    NSScreen *screen = [NSScreen mainScreen];
    if (screen) {
        w->scale_factor = (float)[screen backingScaleFactor];
    }

    /* Window style */
    NSWindowStyleMask style = NSWindowStyleMaskTitled
        | NSWindowStyleMaskClosable
        | NSWindowStyleMaskMiniaturizable;
    if (config->resizable) style |= NSWindowStyleMaskResizable;
    if (config->borderless) style = NSWindowStyleMaskBorderless;

    NSRect contentFrame = NSMakeRect(0, 0, win_w, win_h);
    if (config->centered) {
        NSScreen *scr = [NSScreen mainScreen];
        if (scr) {
            NSRect scrFrame = [scr visibleFrame];
            contentFrame.origin.x = scrFrame.origin.x + (scrFrame.size.width - win_w) / 2;
            contentFrame.origin.y = scrFrame.origin.y + (scrFrame.size.height - win_h) / 2;
        }
    }

    HermesWindow *nswin = [[HermesWindow alloc]
        initWithContentRect:contentFrame
        styleMask:style
        backing:NSBackingStoreBuffered
        defer:NO];
    if (!nswin) { free(w); return NULL; }
    nswin.window = w;
    [nswin setTitle:[NSString stringWithUTF8String:w->config.title]];
    [nswin setAcceptsMouseMovedEvents:YES];

    /* Create OpenGL view */
    HermesView *view = [[HermesView alloc] initWithFrame:contentFrame window:w];
    if (!view) { [nswin release]; free(w); return NULL; }
    view.window = w;
    [nswin setContentView:view];
    [nswin makeFirstResponder:view];

    w->nswin = nswin;
    w->nsview = view;
    w->gl_ctx = view.glContext;

    [nswin makeKeyAndOrderFront:nil];
    s_window_count++;
    return w;
}

/* PoP: window_destroy @ apps/desktop/src/app/window/index.tsx */
void window_destroy(window_t *w) {
    if (!w) return;
    if (w->nswin) {
        [w->nswin close];
        w->nswin = nil;
    }
    w->nsview = nil;
    w->gl_ctx = nil;
    free(w);
    s_window_count--;
}

/* PoP: window_set_title @ apps/desktop/src/app/window/index.tsx */
void window_set_title(window_t *w, const char *title) {
    if (!w || !title || !w->nswin) return;
    strncpy(w->config.title, title, WINDOW_MAX_TITLE - 1);
    [w->nswin setTitle:[NSString stringWithUTF8String:title]];
}

void window_set_min_size(window_t *w, int min_w, int min_h) {
    if (!w) return;
    w->config.min_width = min_w;
    w->config.min_height = min_h;
    if (w->nswin) {
        NSSize minSize = NSMakeSize(min_w, min_h);
        [w->nswin setContentMinSize:minSize];
    }
}

void window_show(window_t *w) {
    if (w && w->nswin) [w->nswin makeKeyAndOrderFront:nil];
}

void window_hide(window_t *w) {
    if (w && w->nswin) [w->nswin orderOut:nil];
}

void window_focus(window_t *w) {
    if (w && w->nswin) {
        [w->nswin makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    }
}

void window_request_close(window_t *w) {
    if (w) w->should_close = true;
}

/* ── Event processing ─────────────────────────────────────────────────── */

/* PoP: window_poll_event @ apps/desktop/src/app/window/index.tsx */
bool window_poll_event(window_t *w, window_event_t *ev) {
    if (!w || !ev) return false;
    @autoreleasepool {
        NSDate *until = [NSDate dateWithTimeIntervalSinceNow:0.001];
        NSEvent *event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
            untilDate:until inMode:NSDefaultRunLoopMode dequeue:YES])) {
            [NSApp sendEvent:event];
        }
    }
    return pop_event(w, ev);
}

/* PoP: window_wait_event @ apps/desktop/src/app/window/index.tsx */
bool window_wait_event(window_t *w, window_event_t *ev) {
    if (!w || !ev) return false;
    if (pop_event(w, ev)) return true;
    @autoreleasepool {
        NSEvent *event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
            untilDate:[NSDate distantFuture] inMode:NSDefaultRunLoopMode dequeue:YES])) {
            [NSApp sendEvent:event];
            window_event_t tmp;
            if (pop_event(w, &tmp)) {
                *ev = tmp;
                return true;
            }
        }
    }
    return false;
}

/* PoP: window_wait_event_timeout @ apps/desktop/src/app/window/index.tsx */
bool window_wait_event_timeout(window_t *w, window_event_t *ev, int timeout_ms) {
    if (!w || !ev) return false;
    if (pop_event(w, ev)) return true;
    @autoreleasepool {
        NSDate *until = [NSDate dateWithTimeIntervalSinceNow:timeout_ms / 1000.0];
        NSEvent *event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
            untilDate:until inMode:NSDefaultRunLoopMode dequeue:YES])) {
            [NSApp sendEvent:event];
            window_event_t tmp;
            if (pop_event(w, &tmp)) {
                *ev = tmp;
                return true;
            }
        }
    }
    return false;
}

/* ── Size / position ──────────────────────────────────────────────────── */

/* PoP: window_get_size @ apps/desktop/src/app/window/index.tsx */
void window_get_size(window_t *w, int *width, int *height) {
    if (!w) return;
    if (w->nswin) {
        NSSize sz = [[w->nswin contentView] frame].size;
        if (width) *width = (int)sz.width;
        if (height) *height = (int)sz.height;
    } else {
        if (width) *width = w->config.width;
        if (height) *height = w->config.height;
    }
}

/* PoP: window_set_size @ apps/desktop/src/app/window/index.tsx */
void window_set_size(window_t *w, int width, int height) {
    if (!w || !w->nswin) return;
    NSRect frame = [w->nswin frame];
    frame.size.width = width;
    frame.size.height = height;
    [w->nswin setFrame:frame display:YES];
    w->config.width = width;
    w->config.height = height;
}

void window_get_position(window_t *w, int *x, int *y) {
    if (!w || !w->nswin) return;
    NSRect frame = [w->nswin frame];
    if (x) *x = (int)frame.origin.x;
    if (y) *y = (int)([[NSScreen mainScreen] frame].size.height - frame.origin.y - frame.size.height);
}

void window_set_position(window_t *w, int x, int y) {
    if (!w || !w->nswin) return;
    NSRect frame = [w->nswin frame];
    CGFloat screenH = [[NSScreen mainScreen] frame].size.height;
    frame.origin.x = x;
    frame.origin.y = screenH - y - frame.size.height;
    [w->nswin setFrame:frame display:YES];
}

float window_get_scale(window_t *w) {
    return w ? w->scale_factor : 1.0f;
}

/* PoP: window_set_fullscreen @ apps/desktop/src/app/window/index.tsx */
void window_set_fullscreen(window_t *w, bool fullscreen) {
    if (!w || !w->nswin) return;
    if (fullscreen) {
        [w->nswin toggleFullScreen:nil];
    }
    w->config.fullscreen = fullscreen;
}

bool window_is_fullscreen(window_t *w) {
    if (!w || !w->nswin) return false;
    return ([w->nswin styleMask] & NSWindowStyleMaskFullScreen) != 0;
}

/* ── Window state management ───────────────────────────────────────────── */

/* PoP: window_minimize @ apps/desktop/src/app/window/index.tsx */
void window_minimize(window_t *w) {
    if (!w || !w->nswin) return;
    [w->nswin miniaturize:nil];
}

/* PoP: window_maximize @ apps/desktop/src/app/window/index.tsx */
void window_maximize(window_t *w) {
    if (!w || !w->nswin) return;
    [w->nswin zoom:nil];
}

/* PoP: window_restore @ apps/desktop/src/app/window/index.tsx */
void window_restore(window_t *w) {
    if (!w || !w->nswin) return;
    if ([w->nswin isMiniaturized]) [w->nswin deminiaturize:nil];
}

/* ── Transparency / Always-on-Top ──────────────────────────────────────── */

/* PoP: window_transparency @ apps/desktop/src/app/window/index.tsx */
void window_set_opacity(window_t *w, float opacity) {
    if (!w || !w->nswin) return;
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 1.0f) opacity = 1.0f;
    [w->nswin setAlphaValue:opacity];
}

float window_get_opacity(window_t *w) {
    if (!w || !w->nswin) return 1.0f;
    return (float)[w->nswin alphaValue];
}

/* PoP: window_always_on_top @ apps/desktop/src/app/window/index.tsx */
void window_set_always_on_top(window_t *w, bool enabled) {
    if (!w || !w->nswin) return;
    if (enabled) {
        [w->nswin setLevel:NSFloatingWindowLevel];
    } else {
        [w->nswin setLevel:NSNormalWindowLevel];
    }
}

bool window_is_always_on_top(window_t *w) {
    if (!w || !w->nswin) return false;
    return [w->nswin level] != NSNormalWindowLevel;
}

/* ── Rendering ─────────────────────────────────────────────────────────── */

window_renderer_t *window_get_renderer(window_t *w) {
    return (window_renderer_t *)w;
}

/* PoP: window_swap_buffers @ apps/desktop/src/app/window/index.tsx */
void window_swap_buffers(window_t *w) {
    if (!w || !w->gl_ctx) return;
    [[w->gl_ctx openGLContext] flushBuffer];
}

void window_render_begin(window_t *w, float r, float g, float b, float a) {
    if (w && w->gl_ctx) {
        [[w->gl_ctx openGLContext] makeCurrentContext];
    }
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void window_render_end(window_t *w) {
    (void)w;
    glFlush();
}

void window_clear(window_t *w, float r, float g, float b, float a) {
    (void)w;
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void window_draw_rect(window_t *w, float x, float y, float width, float height,
                     float r, float g, float b, float a) {
    (void)w;
    glColor4f(r, g, b, a);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void window_fill_rect(window_t *w, float x, float y, float width, float height,
                     float r, float g, float b, float a) {
    (void)w;
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void window_fill_rect_rounded(window_t *w, float x, float y, float width, float height,
                             float radius, float r, float g, float b, float a) {
    (void)w;
    window_fill_rect(w, x + radius, y, width - 2*radius, height, r, g, b, a);
    window_fill_rect(w, x, y + radius, width, height - 2*radius, r, g, b, a);
}

void window_fill_circle(window_t *w, float cx, float cy, float radius,
                       float r, float g, float b, float a) {
    (void)w;
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 32; i++) {
        float angle = 2.0f * 3.14159f * i / 32;
        glVertex2f(cx + radius * cosf(angle), cy + radius * sinf(angle));
    }
    glEnd();
}

void window_draw_text(window_t *w, const char *text, float x, float y,
                     float size, float r, float g, float b, float a) {
    (void)w; (void)text; (void)x; (void)y; (void)size; (void)r; (void)g; (void)b; (void)a;
    /* TODO: implement with CoreText */
}

float window_text_width(window_t *w, const char *text, float size) {
    (void)w; (void)text; (void)size;
    return 0.0f;
}

void window_draw_line(window_t *w, float x0, float y0, float x1, float y1,
                     float line_width, float r, float g, float b, float a) {
    (void)w;
    glColor4f(r, g, b, a);
    glLineWidth(line_width);
    glBegin(GL_LINES);
    glVertex2f(x0, y0);
    glVertex2f(x1, y1);
    glEnd();
}

void window_draw_image(window_t *w, const uint8_t *pixels,
                      int img_w, int img_h,
                      float x, float y, float width, float height) {
    (void)w;
    glEnable(GL_TEXTURE_2D);
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_w, img_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(1, 0); glVertex2f(x + width, y);
    glTexCoord2f(1, 1); glVertex2f(x + width, y + height);
    glTexCoord2f(0, 1); glVertex2f(x, y + height);
    glEnd();
    glDeleteTextures(1, &tex);
    glDisable(GL_TEXTURE_2D);
}

/* ── Mouse / cursor ────────────────────────────────────────────────────── */

/* PoP: window_set_cursor @ apps/desktop/src/app/window/index.tsx */
void window_set_cursor(window_t *w, window_cursor_t cursor) {
    if (!w) return;
    w->current_cursor = cursor;
    switch (cursor) {
    case CURSOR_IBEAM:     [[NSCursor IBeamCursor] set]; break;
    case CURSOR_HAND:      [[NSCursor pointingHandCursor] set]; break;
    case CURSOR_HRESIZE:   [[NSCursor resizeLeftRightCursor] set]; break;
    case CURSOR_VRESIZE:   [[NSCursor resizeUpDownCursor] set]; break;
    case CURSOR_CROSSHAIR: [[NSCursor crosshairCursor] set]; break;
    default:               [[NSCursor arrowCursor] set]; break;
    }
}

void window_get_mouse_pos(window_t *w, int *x, int *y) {
    if (!w || !w->nswin) return;
    NSPoint pt = [w->nswin mouseLocationOutsideOfEventStream];
    NSRect frame = [[w->nswin contentView] frame];
    pt = [[w->nswin contentView] convertPoint:pt fromView:nil];
    if (x) *x = (int)pt.x;
    if (y) *y = (int)(frame.size.height - pt.y);
}

bool window_get_mouse_button(window_t *w, mouse_button_t button) {
    if (!w) return false;
    switch (button) {
    case MOUSE_LEFT:   return (CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonLeft) != 0);
    case MOUSE_RIGHT:  return (CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonRight) != 0);
    case MOUSE_MIDDLE: return (CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonCenter) != 0);
    default: return false;
    }
}

void window_set_mouse_pos(window_t *w, int x, int y) {
    if (!w) return;
    NSPoint pt = NSMakePoint(x, y);
    if (w->nswin) {
        NSRect frame = [[w->nswin contentView] frame];
        pt.y = frame.size.height - pt.y;
        NSPoint windowPt = [[w->nswin contentView] convertPoint:pt toView:nil];
        NSPoint screenPt = [w->nswin convertPointToScreen:windowPt];
        CGWarpMouseCursorPosition(screenPt);
    }
}

/* ── Clipboard ─────────────────────────────────────────────────────────── */

/* PoP: window_clipboard_get @ apps/desktop/src/app/window/index.tsx */
const char *window_clipboard_get(window_t *w) {
    if (!w) return "";
    @autoreleasepool {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        NSString *str = [pb stringForType:NSPasteboardTypeString];
        if (str) {
            strncpy(w->clipboard_text, [str UTF8String], WINDOW_MAX_TEXT_INPUT - 1);
        }
    }
    return w->clipboard_text;
}

/* PoP: window_clipboard_set @ apps/desktop/src/app/window/index.tsx */
void window_clipboard_set(window_t *w, const char *text) {
    if (!w || !text) return;
    strncpy(w->clipboard_text, text, WINDOW_MAX_TEXT_INPUT - 1);
    @autoreleasepool {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        [pb setString:[NSString stringWithUTF8String:text] forType:NSPasteboardTypeString];
    }
}

/* ── Drag and drop ─────────────────────────────────────────────────────── */

void window_enable_dnd(window_t *w, bool enable) {
    if (!w) return;
    w->dnd_enabled = enable;
    if (w->nswin) {
        [w->nswin registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
    }
}

/* ── Tray icon (NSStatusItem) ──────────────────────────────────────────── */

/* PoP: window_tray @ apps/desktop/src/app/window/index.tsx */
static NSStatusItem *s_status_item = nil;

bool window_set_tray_icon(window_t *w, const window_tray_config_t *config) {
    (void)w;
    if (!config) return false;
    @autoreleasepool {
        if (s_status_item) {
            [[NSStatusBar systemStatusBar] removeStatusItem:s_status_item];
            s_status_item = nil;
        }
        if (!config->visible) return true;
        s_status_item = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
        if (!s_status_item) return false;
        if (config->icon_path && config->icon_path[0]) {
            NSString *path = [NSString stringWithUTF8String:config->icon_path];
            NSImage *icon = [[NSImage alloc] initWithContentsOfFile:path];
            if (icon) {
                [s_status_item setImage:icon];
            }
        }
        if (config->tooltip) {
            [s_status_item setToolTip:[NSString stringWithUTF8String:config->tooltip]];
        }
        [s_status_item setHighlightMode:YES];
    }
    return true;
}

bool window_remove_tray(window_t *w) {
    (void)w;
    @autoreleasepool {
        if (s_status_item) {
            [[NSStatusBar systemStatusBar] removeStatusItem:s_status_item];
            s_status_item = nil;
        }
    }
    return true;
}

/* ── Global shortcuts / hotkeys ────────────────────────────────────────── */

/* PoP: window_hotkey @ apps/desktop/src/app/window/index.tsx */
#define MACOS_MAX_HOTKEYS 64

typedef struct {
    char id[64];
    uint32_t key;
    uint32_t mods;
    EventHotKeyRef ref;
    int carbon_id;
} macos_hotkey_entry_t;

static macos_hotkey_entry_t s_hotkeys[MACOS_MAX_HOTKEYS];
static int s_hotkey_count = 0;
static int s_next_hotkey_id = 1;

static uint32_t map_mods_to_carbon(uint32_t mods) {
    uint32_t carbon = 0;
    if (mods & MOD_SHIFT)  carbon |= shiftKey;
    if (mods & MOD_CTRL)   carbon |= controlKey;
    if (mods & MOD_ALT)    carbon |= optionKey;
    if (mods & MOD_SUPER)  carbon |= cmdKey;
    return carbon;
}

bool window_register_hotkey(window_t *w, const window_hotkey_t *hotkey) {
    (void)w;
    if (!hotkey || !hotkey->id) return false;
    if (s_hotkey_count >= MACOS_MAX_HOTKEYS) return false;
    for (int i = 0; i < s_hotkey_count; i++) {
        if (strcmp(s_hotkeys[i].id, hotkey->id) == 0) return false;
    }
    UInt32 keyCode = (UInt32)hotkey->key;
    UInt32 modifiers = map_mods_to_carbon(hotkey->mods);
    EventHotKeyID hotKeyID;
    hotKeyID.signature = 'herm';
    hotKeyID.id = s_next_hotkey_id;
    EventHotKeyRef ref = NULL;
    OSStatus err = RegisterEventHotKey(keyCode, modifiers, hotKeyID,
        GetApplicationEventTarget(), 0, &ref);
    if (err != noErr) return false;
    macos_hotkey_entry_t *e = &s_hotkeys[s_hotkey_count++];
    strncpy(e->id, hotkey->id, sizeof(e->id) - 1);
    e->key = hotkey->key;
    e->mods = hotkey->mods;
    e->ref = ref;
    e->carbon_id = s_next_hotkey_id++;
    return true;
}

bool window_unregister_hotkey(window_t *w, const char *id) {
    (void)w;
    if (!id) return false;
    for (int i = 0; i < s_hotkey_count; i++) {
        if (strcmp(s_hotkeys[i].id, id) == 0) {
            if (s_hotkeys[i].ref) UnregisterEventHotKey(s_hotkeys[i].ref);
            for (int j = i; j < s_hotkey_count - 1; j++) {
                s_hotkeys[j] = s_hotkeys[j + 1];
            }
            s_hotkey_count--;
            return true;
        }
    }
    return false;
}

int window_list_hotkeys(window_t *w, window_hotkey_t *out, int max_count) {
    (void)w;
    if (!out || max_count <= 0) return 0;
    int n = s_hotkey_count < max_count ? s_hotkey_count : max_count;
    for (int i = 0; i < n; i++) {
        strncpy((char *)out[i].id, s_hotkeys[i].id, sizeof(out[i].id) - 1);
        out[i].key = s_hotkeys[i].key;
        out[i].mods = s_hotkeys[i].mods;
        out[i].description = "";
    }
    return n;
}

/* ── Deep linking ───────────────────────────────────────────────────────── */

/* PoP: window_deep_link @ apps/desktop/src/app/window/index.tsx */
static deep_link_cb g_deep_link_cb = NULL;

void window_set_deep_link_callback(window_t *w, deep_link_cb cb) {
    (void)w;
    g_deep_link_cb = cb;
}

bool window_handle_deep_link(window_t *w, const char *url) {
    (void)w;
    if (!url || strncmp(url, "hermes://", 9) != 0) return false;
    const char *action = url + 9;
    const char *params = strchr(action, '?');
    char action_buf[256];
    if (params) {
        size_t len = (size_t)(params - action);
        if (len >= sizeof(action_buf)) len = sizeof(action_buf) - 1;
        strncpy(action_buf, action, len);
        action_buf[len] = '\0';
        params++;
    } else {
        strncpy(action_buf, action, sizeof(action_buf) - 1);
        params = "";
    }
    if (g_deep_link_cb) {
        g_deep_link_cb(url, action_buf, params);
    }
    return true;
}

/* ── Terminal hyperlinks ────────────────────────────────────────────────── */

/* PoP: terminal_web_links @ apps/desktop/src/app/terminal/index.tsx */
void window_terminal_enable_hyperlinks(window_t *w, bool enable) {
    (void)w;
    /* macOS Terminal.app supports OSC 8 hyperlinks natively.
     * For iTerm2, this is also supported. No special mode needed. */
    /* This is a no-op on macOS — hyperlinks work via ANSI escape sequences */
}

bool window_terminal_has_hyperlinks(window_t *w) {
    (void)w;
    /* macOS terminals (Terminal.app, iTerm2) support OSC 8 hyperlinks */
    return true;
}

/* ── Platform info ─────────────────────────────────────────────────────── */

const char *window_platform_name(void) { return "macos"; }
bool window_platform_has_gpu(void) { return true; }
