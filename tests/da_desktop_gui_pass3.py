#!/usr/bin/env python3
"""Devil's Advocate Pass 3: UX depth parity audit"""
import re, os

with open('src/desktop_gui.c') as f:
    content = f.read()

print("=" * 70)
print("PASS 3: UX DEPTH PARITY AUDIT")
print("=" * 70)

# Check each interactive feature
checks = {
    'Sidebar search bar (visual)': 'Search' in content,
    'Sidebar search bar (functional)': False,  # known gap
    'Session hover bg': 'hover_session' in content,
    'Session selected bg': 'selected_session' in content,
    'Session age metadata': 'format_age' in content,
    'Session message count': 'msg_count' in content,
    '+New Chat button': 'New Chat' in content,
    '+New Chat hover': 'hover_newchat' in content,
    '+New Chat functional': 'HIT_NEWCHAT' in content and 'Placeholder' not in content,
    'Disclosure carets': '\\xe2\\x96\\xbc' in content or '\\xe2\\x96\\xb6' in content,
    'Collapsible sections': 'sessions_expanded' in content,
    'Nav hover bg': 'hover_nav' in content,
    'Nav selected bg': 'selected_nav' in content,
    'Model pill in composer': 'hover_pill' in content,
    'Model pill hover': 'hover_pill' in content,
    'Model picker (functional)': 'HIT_PILL' in content and 'model picker' in content.lower(),
    'Composer text input (visual)': 'Send a message' in content,
    'Composer hover': 'composer_hover' in content,
    'Message bubbles (visual)': 'draw_bubble' in content,
    'Message role labels': 'role\","You\"' in content or '"You"' in content,
    'Date separators': 'timestamp' in content and 'strftime' in content,
    'Titlebar tools hover': 'hover_tool' in content,
    'Right sidebar toggle': 'Right sidebar' in content or '\\xe2\\x96\\xa0' in content,
    'Statusbar info': 'draw_statusbar' in content,
    'Profile section at bottom': 'wubu' in content,
    'Profile hover': 'hover_profile' in content,
    'Scrollable sidebar': 'sidebar_scroll' in content,
    'Scrollable chat': 'chat_scroll' in content,
    'Scrollbar visual': 'draw_scrollbar' in content,
    'Scroll wheel support': 'GC_EV_MOUSE_WHEEL' in content,
    'Keyboard scroll': 'SDLK_UP' in content and 'SDLK_DOWN' in content,
    'Scroll reset on session change': 'chat_scroll = 0' in content,
    'Code block rendering (visual)': False,  # gap
    'Message actions (copy/edit)': False,  # gap
    'Right-rail/preview pane': False,  # gap
    'Text selection': False,  # gap
    'Theme toggle (light/dark)': False,  # gap (no key handler)
    'Full composable input (rich text)': False,  # gap
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
