# Next Session Prompt -- v467: Desktop Parity Blitz (Slash Commands + Attachments + Tool Rendering + Syntax Highlighting)

## Context

You are continuing the Slermes C translation project. Current state:
- **8,688/8,688 PORTED (100%)** -- all Python→C port complete
- **Build: CLEAN** -- zero errors
- **Scanner: 100%** -- slermes_parity_battleground.py confirms
- **Desktop Parity: ~25 features implemented** (up from 5)

## What Was Done in v465-v466

Created 11 new C source files + 9 headers (~5,877 lines):
- `src/pty.c` — PTY allocation (openpty/fork), resize, I/O
- `src/terminal.c` — VT100/xterm emulation with scrollback, selection, disposal
- `src/window_compositor.c` — multi-window management (open/focus/close/reorder)
- `src/chat_render.c` — markdown/code block/tool-call rendering
- `src/chat_composer.c` — text input, autocomplete, slash commands, history
- `src/gateway_client.c` — WebSocket + JSON-RPC + streaming
- `src/clipboard.c` — platform clipboard (Wayland/xclip/xsel/pbcopy/Win32)
- `src/file_ops.c` — file read/write/browse, directory operations
- `src/gateway_probe.c` — gateway reachability check
- `src/window_stubs.c` — window_minimize/maximize/restore stubs
- `src/desktop_app_common.c` — session mgmt, model picker, profiles, settings, notifications, safe storage, auth tickets, connection revalidate, update stubs, file dialog stubs, single instance lock

## Mission: Continue Desktop Parity Blitz (Remaining P1 + P2 features)

### Batch 1: Slash Commands Full Implementation

The `src/chat_composer.c` has basic slash command detection. Expand it:

- **/help** — Show available commands list
- **/clear** — Clear current conversation (call desktop_session_create)
- **/model [name]** — Switch model (call desktop_model_select)
- **/new** — Start new session
- **/settings** — Open settings
- **/search [query]** — Search sessions
- **/profile [name]** — Switch profile
- **/copy** — Copy last assistant response to clipboard
- **/paste** — Paste from clipboard into composer
- **/undo** — Undo last action
- **/redo** — Redo last undone action
- **/archive** — Archive current session
- **/pin** — Pin/unpin current session

Modify `src/chat_composer.c`:
- Add `composer_execute_slash(composer_t *c, const char *cmd_line)` function
- Add command table with name/handler/callback
- Wire into `composer_submit()` — if text starts with `/`, execute as command instead of sending

### Batch 2: File Attachments + Image Paste

Modify `src/chat_composer.c`:
- Add `composer_attach_file(composer_t *c, const char *path)` — attach file to message
- Add `composer_attach_image(composer_t *c, const void *png_data, size_t len)` — attach image
- Add `composer_get_attachments(composer_t *c)` — get attachment list
- Add `composer_clear_attachments(composer_t *c)` — clear attachments
- Attachment types: file path, image (PNG/JPEG), text snippet

### Batch 3: Tool Call Rendering Improvements

Modify `src/chat_render.c`:
- Add `chat_render_tool_result(const char *tool_name, const char *args, const char *result, bool is_error)` — render tool call with result
- Add `chat_render_thinking_block(const char *thinking_text)` — render model reasoning
- Improve `chat_render_tool_call` to show expandable/collapsible sections
- Add syntax highlighting for code blocks (basic keyword highlighting)

### Batch 4: Code Syntax Highlighting

Modify `src/chat_render.c`:
- Add basic syntax highlighting for common languages (C, Python, JavaScript, TypeScript, JSON, Bash)
- Keyword-based highlighting: recognize language from code block lang tag
- Map keywords to color tokens (TOKEN_KEYWORD, TOKEN_STRING, TOKEN_COMMENT, TOKEN_NUMBER)
- Languages: c, py, js, ts, json, bash, sh, yaml, toml, markdown

### Batch 5: Window Titlebar + Menu Bar + System Tray

Modify `include/window.h`:
- Add `window_set_titlebar_style(window_t *w, titlebar_style_t style)` — custom titlebar
- Add `window_set_menu_bar(window_t *w, menu_bar_t *menu)` — application menu bar
- Add `window_set_tray_icon(window_t *w, const char *icon_path, const char *tooltip)` — system tray

Modify `src/window_stubs.c`:
- Add stub implementations for titlebar, menu bar, tray icon

### Batch 6: Remaining P2 Features

- **Window transparency/blur** — `window_set_opacity(window_t *w, float opacity)`
- **Window always-on-top** — `window_set_always_on_top(window_t *w, bool on_top)`
- **Global keyboard shortcuts** — `desktop_register_hotkey(const char *shortcut, hotkey_cb cb)`
- **Deep linking (hermes://)** — `desktop_handle_deep_link(const char *url)`
- **Terminal search** — `terminal_search(terminal_t *term, const char *query)`
- **Terminal web links** — `terminal_detect_urls(terminal_t *term)`
- **Session export** — `desktop_session_export(const char *id, const char *path)`
- **Session drag & drop** — `desktop_session_move(int from_idx, int to_idx)`
- **Math rendering** — `chat_render_math(const char *latex)`
- **Voice input/output** — stubs for VAD + TTS
- **Artifact rendering** — `chat_render_artifact(const char *html)`
- **Reasoning display** — `chat_render_reasoning_chain(const char *reasoning)`
- **Context menu** — `desktop_show_context_menu(int x, int y, menu_t *menu)`
- **Profile scope** — `desktop_profile_set_scope(const char *name, const char *scope)`
- **Auxiliary models** — `desktop_model_add_auxiliary(const char *model_id, const char *task)`
- **Model analytics** — `desktop_model_get_stats(const char *model_id)`
- **Model visibility** — `desktop_model_set_visible(const char *model_id, bool visible)`
- **Font settings** — `desktop_settings_set_font(const char *family, int size)`
- **Default project dir** — `desktop_settings_set_default_dir(const char *path)`
- **Environment vars** — `desktop_settings_set_env(const char *key, const char *value)`
- **Show in folder** — `desktop_reveal_in_folder(const char *path)`
- **Dark mode detection** — `desktop_system_dark_mode(void)`
- **Microphone access** — `desktop_request_microphone(void)`
- **Update branch** — `desktop_update_set_branch(const char *branch)`
- **Update marker** — `desktop_update_get_marker(void)`
- **OAuth login** — `desktop_oauth_start(const char *provider)`
- **File watch** — `desktop_file_watch(const char *path, file_watch_cb cb)`
- **Git root** — `desktop_git_root(const char *path)`
- **Clipboard image save** — `desktop_clipboard_save_image(const char *path)`
- **Image from URL save** — `desktop_save_image_from_url(const char *url, const char *path)`
- **Uninstall summary/run** — `desktop_uninstall_summary(void)`, `desktop_uninstall_run(void)`
- **Recent logs** — `desktop_logs_recent(int count)`
- **Reveal logs** — `desktop_logs_reveal(void)`

## Key Files to Modify

- `src/chat_composer.c` — Slash commands, file attachments, image paste
- `src/chat_render.c` — Tool call rendering, syntax highlighting, thinking blocks
- `include/window.h` — Titlebar, menu bar, tray, transparency, always-on-top
- `src/window_stubs.c` — Stubs for new window functions
- `src/desktop_app_common.c` — Deep linking, hotkeys, OAuth, file watch, git root, uninstall, logs
- `include/desktop_app.h` — New API declarations

## Rules

1. **Build must stay CLEAN** -- zero errors after every commit
2. **Scanner must stay 100%** — don't break Python→C port
3. **Real logic, no stubs** (except where platform-specific APIs are needed)
4. **PoP annotations** on all new functions
5. **Use fprintf(stderr, ...)** for logging (not hermes_log)
6. **Commit per batch** — don't accumulate uncommitted code
7. **Autonomous loop** — no questions, no choices, just implement
8. **Run prestige ritual** after every checkpoint

## Workflow

1. Read this prompt
2. Implement Batch 1 (slash commands)
3. Build, test, commit
4. Implement Batch 2 (attachments)
5. Build, test, commit
6. Continue through all batches
7. Run prestige ritual
