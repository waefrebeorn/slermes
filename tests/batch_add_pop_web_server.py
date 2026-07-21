#!/usr/bin/env python3
"""Batch add PoP annotations for web_server.py PARTIAL functions."""
import re
from pathlib import Path

SLERMES_DIR = Path("/home/wubu/hermes-agent-dev/slermes")

PARTIALS = [
    # From port_web_server_auth.c
    ("hermes_cli/web_server.py", "_has_valid_session_token", "src/cli/port_web_server_auth.c", "ws_has_valid_session_token"),
    ("hermes_cli/web_server.py", "should_require_auth", "src/cli/port_web_server_auth.c", "ws_should_require_auth"),
    ("hermes_cli/web_server.py", "_is_accepted_host", "src/cli/port_web_server_auth.c", "ws_is_accepted_host"),
    # From port_web_server.c
    ("hermes_cli/web_server.py", "_infer_type", "src/cli/port_web_server.c", "infer_type"),
    ("hermes_cli/web_server.py", "_normalize_main_model_assignment", "src/cli/port_web_server.c", "normalize_main_model_assignment"),
    ("hermes_cli/web_server.py", "_apply_main_model_assignment", "src/cli/port_web_server.c", "apply_main_model_assignment"),
    ("hermes_cli/web_server.py", "_display_system_platform", "src/cli/port_web_server.c", "web_display_system_platform"),
    ("hermes_cli/web_server.py", "_safe_call", "src/cli/port_web_server.c", "web_safe_call"),
    ("hermes_cli/web_server.py", "_elevenlabs_voice_label", "src/cli/port_web_server.c", "web_elevenlabs_voice_label"),
    ("hermes_cli/web_server.py", "_voice_list_error_logged_once", "src/cli/port_web_server.c", "web_voice_list_error_logged_once"),
    ("hermes_cli/web_server.py", "_redact_mcp_env", "src/cli/port_web_server.c", "web_redact_mcp_env"),
    ("hermes_cli/web_server.py", "_mcp_server_summary", "src/cli/port_web_server.c", "web_mcp_server_summary"),
    ("hermes_cli/web_server.py", "_safe_backup_upload_name", "src/cli/port_web_server.c", "web_safe_backup_upload_name"),
    ("hermes_cli/web_server.py", "_normalise_prefix", "src/cli/port_web_server.c", "web_normalise_prefix"),
    ("hermes_cli/web_server.py", "_parse_theme_layer", "src/cli/port_web_server.c", "web_parse_theme_layer"),
    ("hermes_cli/web_server.py", "_normalise_theme_definition", "src/cli/port_web_server.c", "web_normalise_theme_definition"),
    ("hermes_cli/web_server.py", "_validate_plugin_name", "src/cli/port_web_server.c", "web_validate_plugin_name"),
    ("hermes_cli/web_server.py", "_read_bound_port", "src/cli/port_web_server.c", "web_read_bound_port"),
    ("hermes_cli/web_server.py", "_write_dashboard_ready_file", "src/cli/port_web_server.c", "web_write_dashboard_ready_file"),
    ("hermes_cli/web_server.py", "_maybe_open_browser", "src/cli/port_web_server.c", "web_maybe_open_browser"),
    # From run_pure.c
    ("hermes_cli/web_server.py", "_record_completed_action", "src/gateway/run_pure.c", "web_record_completed_action"),
    ("hermes_cli/web_server.py", "_dashboard_spawn_executable", "src/gateway/run_pure.c", "web_dashboard_spawn_executable"),
    ("hermes_cli/web_server.py", "_tail_lines", "src/gateway/run_pure.c", "web_tail_lines"),
    ("hermes_cli/web_server.py", "_normalize_config_for_web", "src/gateway/run_pure.c", "web_normalize_config_for_web"),
    # From checkpoint_manager.c
    ("tools/checkpoint_manager.py", "prune_checkpoints", "src/tools/checkpoint_manager.c", "prune_checkpoints"),
]


def find_c_function_definition(content, func_name):
    """Find the line number of a C function definition."""
    patterns = [
        rf'^(static\s+)?\w+(?:\s*\*)?\s+{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?const\s+\w+(?:\s*\*)?\s+{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?unsigned\s+\w+\s+{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?long\s+(?:long\s+)?{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?struct\s+\w+\s*\*?\s*{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?enum\s+\w+\s+{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?inline\s+.*?{re.escape(func_name)}\s*\(',
        # Also handle json_t *func_name pattern
        rf'^(static\s+)?json_t\s*\*\s*{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?bool\s+{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?int\s+{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?char\s*\*\s*{re.escape(func_name)}\s*\(',
        rf'^(static\s+)?void\s+{re.escape(func_name)}\s*\(',
    ]

    lines = content.split('\n')
    for i, line in enumerate(lines):
        stripped = line.strip()
        for pat in patterns:
            if re.match(pat, stripped):
                return i + 1
    return None


def add_pop_to_file(filepath, func_name, module_path, py_func):
    full_path = SLERMES_DIR / filepath
    if not full_path.exists():
        print(f"  SKIP: {filepath} not found")
        return False

    content = full_path.read_text()

    # Check if PoP already exists anywhere in the file for this function
    if f'PoP: {func_name} @' in content:
        print(f"  SKIP: {func_name} already has PoP in {filepath}")
        return False

    # Use the C function name for the PoP annotation
    pop_line = f"/* PoP: {func_name} @ {module_path}:{py_func} */"

    line_num = find_c_function_definition(content, func_name)

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


# Remove duplicates
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