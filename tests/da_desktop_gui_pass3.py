#!/usr/bin/env python3
"""Devil's Advocate Pass 3: UX depth parity audit"""
import re, os

# The SDL GUI was modularized (v500s): desktop_gui.c is now a thin entry
# point. Audit the full module set so feature checks reflect reality.
MODULES = [
    'src/desktop_gui.c', 'src/gui_core.c', 'src/app_state.c', 'src/session_db.c',
    'src/sidebar.c', 'src/chat_view.c', 'src/titlebar.c', 'src/event_handling.c',
    'src/hud.c', 'src/desktop_controller.c', 'src/pet_ui.c', 'src/session_switcher.c',
    'src/chat_render.c', 'src/chat_composer.c', 'src/desktop_app_common.c',
    'src/desktop_sessions.c', 'src/desktop_models.c', 'src/desktop_profiles.c',
    'src/desktop_settings.c',
]
content = "\n".join(open(m).read() for m in MODULES if os.path.exists(m))

print("=" * 70)
print("PASS 3: UX DEPTH PARITY AUDIT")
print("=" * 70)

# Check each interactive feature
checks = {
    'Sidebar search bar (visual)': 'Search' in content,
    'Sidebar search bar (functional)': 'search_query' in content,
    'Session hover bg': 'hovered' in content or 'hover' in content,
    'Session selected bg': 'selected_session' in content,
    'Session age metadata': 'format_age' in content or 'started_at' in content,
    'Session message count': 'msg_count' in content,
    '+New Chat button': 'New Chat' in content,
    '+New Chat hover': 'hover' in content,
    '+New Chat functional': 'HIT_NEWCHAT' in content and 'Placeholder' not in content,
    'Disclosure carets': '\xe2\x96\xbc' in content or '\xe2\x96\xb6' in content,
    'Collapsible sections': 'sessions_expanded' in content,
    'Nav hover bg': 'hover' in content,
    'Nav selected bg': 'selected_nav' in content,
    'Model pill in composer': 'pill' in content,
    'Model pill hover': 'hover' in content,
    'Model picker (functional)': 'HIT_PILL' in content and 'model picker' in content.lower(),
    'Composer text input (visual)': 'composer' in content,
    'Composer hover': 'hover' in content,
    'Message bubbles (visual)': 'bubble' in content,
    'Message role labels': 'role' in content,
    'Date separators': 'timestamp' in content,
    'Titlebar tools hover': 'hover' in content and 'titlebar' in content,
    'Right sidebar toggle': 'Right sidebar' in content or '\xe2\x96\xa0' in content,
    'Statusbar info': 'statusbar' in content,
    'Profile section at bottom': 'profile' in content,
    'Profile hover': 'hover' in content,
    'Scrollable sidebar': 'sidebar_scroll' in content or 'scroll' in content,
    'Scrollable chat': 'chat_scroll' in content,
    'Scrollbar visual': 'scrollbar' in content,
    'Scroll wheel support': 'GC_EV_MOUSE_WHEEL' in content,
    'Keyboard scroll': 'SDLK_UP' in content and 'SDLK_DOWN' in content,
    'Scroll reset on session change': 'chat_scroll' in content,
    'Code block rendering (visual)': 'TOKEN_CODE_BLOCK' in content or 'code_block' in content,
    'Message actions (copy/edit)': 'copy' in content.lower() and 'edit' in content.lower(),
    'Right-rail/preview pane': False,  # gap
    'Text selection': False,  # gap
    'Theme toggle (light/dark)': 'toggle_theme' in content or 'dark_mode' in content,
    'Full composable input (rich text)': 'autocomplete' in content or 'slash' in content.lower(),
    'Attachment support': False,  # gap
    'Voice input': False,  # gap (P2)
}

done = sum(1 for v in checks.values() if v)
total = len(checks)
pct = done * 100 // total

print(f"\n  {'Feature':40s} {'Status':10s}")
print(f"  {'─'*50}")
for feature, status in sorted(checks.items()):
    icon = '✅' if status else '🔴'
    print(f"  {feature:40s} {icon}")

print(f"\n  {'─'*50}")
print(f"  Implemented: {done}/{total} ({pct}%)")
print(f"  Missing:     {total - done}/{total} ({100 - pct}%)")
print()

# Detail the missing features
print("  DETAIL: Missing features and their impact:")
missing = [f for f, s in checks.items() if not s]
for f in missing:
    impact = {
        'Sidebar search bar (functional)': 'User cannot search/filter sessions',
        'Message actions (copy/edit)': 'Cannot copy or edit messages',
        'Code block rendering (visual)': 'Code blocks not highlighted with syntax',
        'Right-rail/preview pane': 'No terminal, file preview, or agents panel',
        'Text selection': 'Cannot select text to copy',
        'Theme toggle (light/dark)': 'Cannot switch to light mode',
        'Full composable input (rich text)': 'No markdown input, @mentions, /commands',
        'Attachment support': 'Cannot attach files/images',
        'Voice input': 'No microphone input support',
        'Model picker (functional)': 'Model pill exists but clicking does nothing',
        '+New Chat functional': 'Button exists but clicking does nothing',
    }.get(f, '')
    print(f"    🔴 {f}: {impact}")
