# Next Session Prompt -- v468: Desktop Parity Blitz (Win32 Backend + macOS Backend + TUI Integration)

## Context
- 8,688/8,688 PORTED (100%), Build: CLEAN, Scanner: 100%
- Desktop Parity: ~55 features implemented (up from ~25)
- v467 created 5,419 new lines across 8 files: slash commands, attachments, syntax
  highlighting, tool rendering, window features, 30+ P2 desktop APIs
- Current files: chat_composer.c (806), chat_render.c (912), window_stubs.c (208),
  desktop_app_common.c (2,093), desktop_app.h (544), window.h (418)

## Mission: Platform Backends + TUI Integration

### Batch 1: Win32 Window Backend
Create src/window_win32.c (replacing the stub):
- window_create() — RegisterClassEx, CreateWindowEx, message loop
- window_destroy() — DestroyWindow, UnregisterClass
- window_poll_event() / window_wait_event() — GetMessage/PeekMessage + TranslateMessage + DispatchMessage
- window_set_title() — SetWindowText
- window_set_size() / window_get_size() — SetWindowPos, GetClientRect
- window_set_position() / window_get_position() — SetWindowPos, GetWindowRect
- window_show() / window_hide() — ShowWindow
- window_focus() — SetForegroundWindow
- window_minimize() / window_maximize() / window_restore() — ShowWindow with SW_MINIMIZE/SW_MAXIMIZE/SW_RESTORE
- window_set_fullscreen() — Remove/set WS_OVERLAPPEDWINDOW style
- window_set_opacity() — SetLayeredWindowAttributes with LWA_ALPHA
- window_set_always_on_top() — SetWindowPos with HWND_TOPMOST
- window_set_cursor() — LoadCursor/SetCursor per window_cursor_t
- window_get_mouse_pos() / window_set_mouse_pos() — GetCursorPos/SetCursorPos
- window_clipboard_get() / window_clipboard_set() — OpenClipboard/GetClipboardData/SetClipboardData
- window_swap_buffers() — SwapBuffers (WGL)
- window_render_begin() / window_render_end() — glClear, wglSwapBuffers
- window_clear() — glClearColor + glClear
- window_draw_rect() / window_fill_rect() — OpenGL immediate mode or GDI
- window_draw_text() — wglUseFontBitmaps or GDI TextOut
- window_set_tray_icon() — Shell_NotifyIcon with NIM_ADD
- window_remove_tray() — Shell_NotifyIcon with NIM_DELETE
- window_register_hotkey() — RegisterHotKey
- window_unregister_hotkey() — UnregisterHotKey
- window_handle_deep_link() — RegisterProtocolHandler for hermes://
- window_terminal_enable_hyperlinks() — SetConsoleMode with ENABLE_VIRTUAL_TERMINAL_PROCESSING
- window_platform_name() — return "win32"
- window_platform_has_gpu() — return true

### Batch 2: macOS Window Backend
Create/update src/window_macos.m (Objective-C):
- window_create() — NSWindow + NSView with initWithContentRect
- window_destroy() — [window close], [window release]
- window_poll_event() — [NSApp nextEventMatchingMask]
- window_set_title() — [window setTitle:]
- window_set_size() / window_get_size() — [window setContentSize:], [window contentRectForFrameRect:]
- window_show() / window_hide() — [window orderFront:], [window orderOut:]
- window_focus() — [window makeKeyAndOrderFront:]
- window_minimize() / window_maximize() — [window miniaturize:], [window zoom:]
- window_set_fullscreen() — [window toggleFullScreen:]
- window_set_opacity() — [window setAlphaValue:]
- window_set_always_on_top() — [window setLevel:NSFloatingWindowLevel]
- window_set_cursor() — [NSCursor set]
- window_clipboard_get() / window_clipboard_set() — NSPasteboard generalPasteboard
- window_swap_buffers() — [[openGLContext] flushBuffer]
- window_set_tray_icon() — NSStatusBar systemStatusBar
- window_register_hotkey() — RegisterEventHotKey
- window_platform_name() — return "macos"
- window_platform_has_gpu() — return true

### Batch 3: TUI Integration
Modify src/cli/tui_fullscreen.c (or create if needed):
- TUI rendering that uses chat_render.c for message display
- TUI input that uses chat_composer.c for text editing
- Slash command support in TUI
- Syntax highlighting in TUI output (using chat_render_highlight)
- Terminal search integration
- Session switching in TUI (F1-F12 or /session command)
- Model picker overlay in TUI
- Notification display in TUI status bar

### Batch 4: Platform Detection + Build Integration
Update Makefile:
- Detect platform via uname -s
- Link window_win32.c on Windows (MinGW/Cygwin)
- Link window_macos.m on macOS
- Link window_wayland.c on Linux
- Link window_stubs.c as fallback
- Add -framework Cocoa -framework IOKit on macOS
- Add -lgdi32 -lole32 -lshell32 on Windows
- Ensure all platform backends compile cleanly

### Batch 5: Integration Testing
- Create tests/test_window.c — basic window create/destroy on each platform
- Create tests/test_composer.c — composer lifecycle, attachments, slash commands
- Create tests/test_render.c — markdown rendering, syntax highlighting
- Create tests/test_desktop.c — session CRUD, settings, notifications
- All tests must pass on Linux (Wayland/stubs)

## Rules
1. Build must stay CLEAN
2. Scanner must stay 100%
3. Real logic, no stubs (except where platform APIs unavailable)
4. PoP annotations on all new functions
5. Use fprintf(stderr, ...) for logging
6. Commit per batch
7. Run prestige ritual after each checkpoint

## Key Files to Create/Modify
- src/window_win32.c — Full Win32 window backend (NEW)
- src/window_macos.m — Full macOS window backend (UPDATE)
- src/cli/tui_fullscreen.c — TUI integration with chat_render + chat_composer
- Makefile — Platform detection and linking
- tests/test_window.c — Window tests (NEW)
- tests/test_composer.c — Composer tests (NEW)
- tests/test_render.c — Render tests (NEW)
- tests/test_desktop.c — Desktop app tests (NEW)

## Workflow
1. Implement Batch 1 (Win32 backend)
2. Build, test, commit
3. Implement Batch 2 (macOS backend)
4. Build, test, commit
5. Implement Batch 3 (TUI integration)
6. Build, test, commit
7. Implement Batch 4 (build integration)
8. Build, test, commit
9. Implement Batch 5 (tests)
10. Build, test, commit
11. Run prestige ritual
