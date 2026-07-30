#!/usr/bin/env python3
"""Batch add PoP annotations for all PARTIAL functions."""
import re
from pathlib import Path

SLERMES_DIR = Path("/home/wubu/hermes-agent-dev/slermes")

PARTIALS = [
    # (module_path, py_func, c_file_rel, c_func)
    ("agent/learning_graph_render.py", "build_summary", "src/tools/todo.c", "build_summary"),
    ("gateway/delivery.py", "looks_like_telegram_private_chat_id", "src/gateway/helpers.c", "looks_like_telegram_private_chat_id"),
    ("gateway/pairing.py", "generate_code", "src/plugins/plugin_google_meet.c", "generate_code"),
    ("gateway/platforms/yuanbao.py", "_patch", "src/cli/port_nous_billing.c", "http_patch"),
    ("gateway/platforms/yuanbao.py", "_authenticate", "src/acp/server.c", "handle_authenticate"),
    ("gateway/platforms/yuanbao.py", "send_text", "src/web_server.c", "ws_send_text"),
    ("gateway/platforms/yuanbao.py", "send_text", "src/web_server.c", "ws_send_text"),
    ("gateway/status.py", "_get_lock_dir", "src/cron/cron_locking.c", "get_lock_dir"),
    ("tools/browser_camofox.py", "_get_command_timeout", "src/tools/port_browser_tool.c", "browser_get_command_timeout"),
    ("tools/browser_camofox.py", "_post", "src/cli/port_nous_billing.c", "http_post"),
    ("tools/checkpoint_manager.py", "prune_checkpoints", "src/tools/checkpoint_manager.c", "prune_checkpoints"),
    ("tools/computer_use/backend.py", "click", "src/tools/browser.c", "browser_click_handler"),
    ("tools/computer_use/backend.py", "scroll", "src/tools/port_session_search_tool.c", "scroll"),
    ("tools/computer_use/permissions.py", "_doctor", "src/cli/commands.c", "cmd_doctor"),
    ("tools/computer_use/tool.py", "_get_backend", "src/tools/port_web_tools.c", "web_get_backend"),
    ("tools/computer_use/tool.py", "click", "src/tools/browser.c", "browser_click_handler"),
    ("tools/computer_use/tool.py", "scroll", "src/tools/port_session_search_tool.c", "scroll"),
    ("tools/debug_helpers.py", "log_call", "lib/libdebug/debug_helpers.c", "debug_session_log_call"),
    ("tools/delegate_tool.py", "is_spawn_paused", "include/hermes.h", "is_spawn_paused"),
    ("tools/skill_manager_tool.py", "_find_skill", "src/plugins/plugin_skills.c", "find_skill"),
    ("tools/skill_usage.py", "read_suppressed_names", "src/tools/port_skills_sync.c", "read_suppressed_names"),
    ("tools/skill_usage.py", "get_record", "lib/libskillusage/skill_usage.c", "skill_usage_get_record"),
    ("tools/skill_usage.py", "bump_view", "lib/libskillusage/skill_usage.c", "skill_usage_bump_view"),
    ("tools/skill_usage.py", "bump_use", "lib/libskillusage/skill_usage.c", "skill_usage_bump_use"),
    ("tools/skill_usage.py", "bump_patch", "lib/libskillusage/skill_usage.c", "skill_usage_bump_patch"),
    ("tools/skill_usage.py", "mark_agent_created", "lib/libskillusage/skill_usage.c", "skill_usage_mark_agent_created"),
    ("tools/skill_usage.py", "set_pinned", "lib/libskillusage/skill_usage.c", "skill_usage_set_pinned"),
    ("tools/skill_usage.py", "forget", "lib/libskillusage/skill_usage.c", "skill_usage_forget"),
    ("tools/skills_hub.py", "_search_catalog", "src/skills_hub.c", "search_catalog"),
    ("tools/skills_hub.py", "_fetch_catalog", "src/skills_hub.c", "skills_hub_fetch_catalog"),
    ("tools/terminal_tool.py", "cleanup_all_environments", "src/tools/terminal.c", "cleanup_all_environments"),
    ("tools/terminal_tool.py", "cleanup_vm", "src/tools/terminal.c", "cleanup_vm"),
    ("tools/tirith_security.py", "_get_hermes_home", "src/cli/port_web_server.c", "get_hermes_home"),
]

def find_c_function_definition(content, func_name):
    """Find the line number of a C function definition.
    Tries: return_type func_name( or static return_type func_name(
    """
    # Look for function definition with multiline return type
    patterns = [
        rf'^(static\s+)?\w+(?:\s*\*)?\s+{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?const\s+\w+(?:\s*\*)?\s+{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?unsigned\s+\w+\s+{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?long\s+(?:long\s+)?{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?struct\s+\w+\s*\*?\s*{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?enum\s+\w+\s+{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?inline\s+.*?{re.escape(func_name)}\s*\(',
        # Also handle declaration in headers: extern/type on previous line
    ]
    
    lines = content.split('\n')
    for i, line in enumerate(lines):
        stripped = line.strip()
        for pat in patterns:
            if re.match(pat, stripped):
                return i + 1  # 1-indexed
    return None


def add_pop_to_file(filepath, func_name, module_path, py_func):
    """Add /* PoP: func @ module/path.py:py_func */ before the function definition."""
    full_path = SLERMES_DIR / filepath
    if not full_path.exists():
        print(f"  SKIP: {filepath} not found")
        return False
    
    content = full_path.read_text()
    
    # Check if PoP already exists anywhere in the file for this function
    if f'PoP: {func_name} @' in content:
        print(f"  SKIP: {func_name} already has PoP in {filepath}")
        return False
    
    # Use the C function name for the PoP annotation (matches the annotation format)
    pop_line = f"/* PoP: {func_name} @ {module_path}:{py_func} */"
    
    # Try to find function definition
    if filepath.endswith('.c'):
        # For .c files, look for the function definition
        line_num = find_c_function_definition(content, func_name)
    elif filepath.endswith('.h'):
        # For headers, look for declaration
        line_num = find_c_function_definition(content, func_name)
    else:
        line_num = None
    
    if line_num:
        lines = content.split('\n')
        # Check if an annotation already exists within 3 lines before
        start = max(0, line_num - 4)
        before = '\n'.join(lines[start:line_num - 1])
        if 'PoP:' in before:
            print(f"  SKIP: PoP already near line {line_num} in {filepath}")
            return False
        
        # Insert PoP before the function definition
        insert_pos = line_num - 1  # 0-indexed
        lines.insert(insert_pos, pop_line)
        full_path.write_text('\n'.join(lines) + '\n')
        print(f"  ADDED line {line_num}: {pop_line}")
        return True
    else:
        print(f"  FAIL: Could not find definition of {func_name} in {filepath}")
        # Try searching with a broader pattern
        for i, line in enumerate(content.split('\n'), 1):
            if func_name in line and ('(' in line or ')' in line):
                stripped = line.strip()
                if not stripped.startswith('//') and not stripped.startswith('/*') and not stripped.startswith('*'):
                    print(f"    Candidate at line {i}: {stripped[:80]}")
        return False


# Remove duplicates (same c_file + c_func)
seen = set()
unique_partials = []
for p in PARTIALS:
    key = (p[2], p[3])
    if key not in seen:
        seen.add(key)
        unique_partials.append(p)
    else:
        print(f"  DEDUP: {p[2]}:{p[3]} (module {p[0]})")

print(f"Processing {len(unique_partials)} unique PARTIALs...")
added = 0
for mod_path, py_func, c_file, c_func in unique_partials:
    print(f"  [{c_file:45s}] {c_func}")
    if add_pop_to_file(c_file, c_func, mod_path, py_func):
        added += 1

print(f"\nAdded {added} PoP annotations. Build and re-scan to verify.")