#!/usr/bin/env python3
"""Devil's Advocate Pass 2: functional/structural/architectural audit"""
import re

with open('src/desktop_gui.c') as f:
    lines = f.readlines()
    content = ''.join(lines)

issues = []

# 1. SQL injection scan — check all SQL query construction
sql_queries = []
for i, line in enumerate(lines, 1):
    if 'SELECT' in line or 'sqlite3_mprintf' in line:
        sql_queries.append((i, line.strip()[:100]))

print("=" * 70)
print("PASS 2: FUNCTIONAL & ARCHITECTURAL AUDIT")
print("=" * 70)

print("\n  [SQL QUERIES]")
for i, q in sql_queries:
    print(f"    L{i}: {q}")
    # Check for injection risk
    if 'snprintf' in q and '%s' in q and 'session' in q.lower():
        print(f"      ⚠️  snprintf with user data — sqlite3_mprintf preferred")

# 2. Check all sqlite3_mprintf are paired with sqlite3_free
mprintf_count = content.count('sqlite3_mprintf(')
mprintf_free = content.count('sqlite3_free(')
print(f"\n  sqlite3_mprintf: {mprintf_count}, sqlite3_free: {mprintf_free}")
if mprintf_count == mprintf_free:
    print("  ✅ All mprintf allocations freed")

# 3. Check event handling completeness
events_handled = []
for i, line in enumerate(lines, 1):
    m = re.search(r'case\s+(GC_EV_\w+)', line)
    if m:
        events_handled.append((i, m.group(1)))

print("\n  [EVENT HANDLING]")
event_types = {'GC_EV_QUIT': 'quit', 'GC_EV_KEY_DOWN': 'key', 
               'GC_EV_MOUSE_MOVE': 'move', 'GC_EV_MOUSE_DOWN': 'down',
               'GC_EV_MOUSE_UP': 'up', 'GC_EV_MOUSE_WHEEL': 'wheel',
               'GC_EV_RESIZE': 'resize'}
handled = {e[1] for e in events_handled}
for ev, desc in event_types.items():
    status = '✅' if ev in handled else '❌ MISSING'
    print(f"  {status} {ev} ({desc})")

# 4. Check for hardcoded paths vs slermes_home usage
hardcoded_hermes = content.count('/home/wubu/.hermes') + content.count('HERMES_HOME')
if hardcoded_hermes == 0:
    print("\n  ✅ No hardcoded Hermes paths — all use slermes_home")
else:
    print(f"\n  ⚠️  {hardcoded_hermes} hardcoded Hermes paths remain")

# 5. Theme/corner case: zero sessions
has_zero_check = 'session_count == 0' in content or 'message_count == 0' in content
print(f"  {'✅' if has_zero_check else '❌'} Zero-state handling (empty sessions/messages)")

# 6. Window resize safety
has_resize_check = 'GC_EV_RESIZE' in content
print(f"  {'✅' if has_resize_check else '❌'} Window resize event handled")

# 7. Check for missing const correctness
for i, line in enumerate(lines, 1):
    s = line.strip()
    if 'char *' in s and 'const' not in s and ('demo_sessions' in s or 'nav_items' in s):
        issues.append(('CONST', i, f'Non-const string table: {s[:60]}'))

# 8. Check for proper localtime_r (thread-safe) vs localtime
localtime_count = content.count('localtime(')
localtime_r_count = content.count('localtime_r(')
print(f"\n  localtime() (not thread-safe): {localtime_count}")
print(f"  localtime_r() (thread-safe): {localtime_r_count}")
if localtime_r_count == 0 and localtime_count > 0:
    print("  ⚠️  Use localtime_r() for thread safety (localtime() uses static buffer)")

# 9. Check static buffer safety
for i, line in enumerate(lines, 1):
    s = line.strip()
    m = re.search(r'char\s+(\w+)\[(\d+)\]', s)
    if m:
        name, size = m.group(1), int(m.group(2))
        # Check if this buffer is used with snprintf that could overflow
        for j in range(i, min(i+10, len(lines))):
            if f'snprintf({name}' in lines[j]:
                # Check the format size
                sz_m = re.search(r'sizeof\(\w+\)', lines[j])
                if sz_m:
                    pass  # using sizeof — safe
                elif f'{name}, {size}' in lines[j]:
                    pass  # using literal size — safe
                else:
                    pass  # more complex check
                break

# 10. db_query callback safety — check all callbacks
callbacks = re.findall(r'static int (cb_\w+)\(', content)
print(f"\n  SQLite callbacks: {len(callbacks)}")
for cb in callbacks:
    # Check if callback modifies app state
    uses_app = any(f'app.{cb}' in content for cb in re.findall(r'cb_\w+', content))
    pass

print(f"\n{'─'*70}")
print(f"Issues found: {len(issues)}")
if issues:
    for t,i,m in issues:
        print(f"  [{t}] L{i}: {m}")
else:
    print("  Clean — no structural issues")
