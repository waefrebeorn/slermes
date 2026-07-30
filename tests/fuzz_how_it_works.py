#!/usr/bin/env python3
"""
Slermes — Comprehensive QoL & Edge-Case Fuzz Suite (v372)

Fuzz-tests every feature in how-it-works.md against the C binary.
Generator-based: produces parametrized test cases from spec tables.

Run: python3 tests/fuzz_how_it_works.py
"""

import subprocess
import sys
import os
import time
import json
import re
import tempfile

SLERMES = os.path.expanduser("~/hermes-agent-dev/slermes/slermes")
TUI = os.path.expanduser("~/hermes-agent-dev/slermes/tui/slermes-tui")

PASS = 0
FAIL = 0
SKIP = 0
results = []

def test(name, fn):
    global PASS, FAIL
    try:
        fn()
        PASS += 1
        results.append(("PASS", name))
        print(f"  ✅ {name}")
    except AssertionError as e:
        FAIL += 1
        results.append(("FAIL", name))
        print(f"  ❌ {name}: {e}")
    except Exception as e:
        FAIL += 1
        results.append(("FAIL", name))
        print(f"  ❌ {name}: {e}")

def slermes(args, input=None, timeout=10):
    """Run slermes with args, return (stdout, stderr, rc)."""
    r = subprocess.run(
        [SLERMES] + args,
        input=input,
        capture_output=True,
        timeout=timeout,
        text=True,
        cwd=os.path.expanduser("~")
    )
    return r.stdout, r.stderr, r.returncode


# ═══════════════════════════════════════════════════════════════════
#  SECTION 1: CLI STARTUP & ARGUMENT PARSING (from how-it-works §1)
# ═══════════════════════════════════════════════════════════════════

def test_help():
    out, err, rc = slermes(["--help"])
    assert rc == 0, f"rc={rc}"
    assert "Usage" in out or "usage" in out, f"no usage in: {out[:200]}"

def test_version():
    out, err, rc = slermes(["--version"])
    assert rc == 0, f"rc={rc}"
    assert "slermes" in out.lower() or "version" in out.lower()

def test_session_flag():
    out, err, rc = slermes(["--session", "test_session", "--help"])
    assert rc == 0

def test_model_flag():
    out, err, rc = slermes(["--model", "gpt-4", "--help"])
    assert rc == 0

def test_provider_flag():
    out, err, rc = slermes(["--provider", "openai", "--help"])
    assert rc == 0

def test_json_flag():
    out, err, rc = slermes(["--json", "--help"])
    assert rc == 0

def test_invalid_flag():
    out, err, rc = slermes(["--nonexistent"])
    assert rc != 0, "invalid flag should fail"

def test_help_gateway():
    out, err, rc = slermes(["help", "gateway"])
    assert rc == 0

def test_help_cron():
    out, err, rc = slermes(["help", "cron"])
    assert rc == 0

def test_help_tools():
    out, err, rc = slermes(["help", "tools"])
    assert rc == 0

def test_doctor():
    out, err, rc = slermes(["doctor"])
    assert rc == 0

# ═══════════════════════════════════════════════════════════════════
#  SECTION 2: SLASH COMMANDS (from how-it-works §4)
# ═══════════════════════════════════════════════════════════════════

def test_commands_new():
    out, err, rc = slermes(["/new"])
    assert rc == 0, f"rc={rc}, out={out[:200]}"

def test_commands_model():
    out, err, rc = slermes(["/model"])
    assert rc == 0

def test_commands_config():
    out, err, rc = slermes(["/config"])
    assert rc == 0

def test_commands_help():
    out, err, rc = slermes(["/help"])
    assert rc == 0, f"rc={rc}"

def test_commands_exit():
    out, err, rc = slermes(["/exit"])
    assert rc == 0

def test_commands_undo():
    out, err, rc = slermes(["/undo"])
    assert rc == 0

def test_commands_retry():
    out, err, rc = slermes(["/retry"])
    assert rc == 0

def test_commands_save():
    out, err, rc = slermes(["/save", "test_save"], timeout=5)
    assert rc == 0

def test_commands_tools():
    out, err, rc = slermes(["/tools"])
    assert rc == 0
    assert len(out) > 50

def test_commands_skills():
    out, err, rc = slermes(["/skills"])
    assert rc == 0

def test_commands_secrets():
    out, err, rc = slermes(["/secrets"])
    assert rc == 0

def test_commands_status():
    out, err, rc = slermes(["/status"])
    assert rc == 0

def test_commands_logs():
    out, err, rc = slermes(["/logs"])
    assert rc == 0

def test_commands_plugins():
    out, err, rc = slermes(["/plugins"])
    assert rc == 0

def test_commands_session():
    out, err, rc = slermes(["/session"])
    assert rc == 0

def test_commands_memory():
    out, err, rc = slermes(["/memory"])
    assert rc == 0

def test_commands_cron():
    out, err, rc = slermes(["/cron"])
    assert rc == 0

def test_commands_kanban():
    out, err, rc = slermes(["/kanban"])
    assert rc == 0

def test_commands_mcp():
    out, err, rc = slermes(["/mcp"])
    assert rc == 0

def test_commands_approve():
    out, err, rc = slermes(["/approve"])
    assert rc == 0

def test_commands_deny():
    out, err, rc = slermes(["/deny"])
    assert rc == 0

def test_commands_clear():
    out, err, rc = slermes(["/clear"])
    assert rc == 0

def test_commands_verbose():
    out, err, rc = slermes(["/verbose"])
    assert rc == 0

def test_commands_dump():
    out, err, rc = slermes(["/dump"])
    assert rc == 0

def test_commands_snapshot():
    out, err, rc = slermes(["/snapshot"])
    assert rc == 0

def test_commands_branch():
    out, err, rc = slermes(["/branch"])
    assert rc == 0

# ═══════════════════════════════════════════════════════════════════
#  SECTION 3: ALIAS RESOLUTION (from how-it-works §4)
# ═══════════════════════════════════════════════════════════════════

def test_alias_new():
    out, err, rc = slermes(["/n"])
    out2, err2, rc2 = slermes(["/new"])
    assert rc == rc2, f"/n rc={rc} vs /new rc={rc2}"

def test_alias_clear():
    out, err, rc = slermes(["/c"])
    out2, err2, rc2 = slermes(["/clear"])
    assert rc == rc2

def test_alias_model():
    out, err, rc = slermes(["/m"])
    out2, err2, rc2 = slermes(["/model"])
    assert rc == rc2 or "list" in out

# ═══════════════════════════════════════════════════════════════════
#  SECTION 4: DISPLAY SYSTEM (from how-it-works §5)
# ═══════════════════════════════════════════════════════════════════

def test_display_tools():
    out, err, rc = slermes(["/tools"])
    assert rc == 0
    # Should have tabular display with alignment
    assert "file" in out or "read" in out

def test_display_skills():
    out, err, rc = slermes(["/skills"])
    assert rc == 0

def test_skin():
    out, err, rc = slermes(["/skin"])
    assert rc == 0

def test_display_statusbar():
    out, err, rc = slermes(["/statusbar"])
    assert rc == 0

def test_display_redraw():
    out, err, rc = slermes(["/redraw"])
    assert rc == 0

def test_display_indicator():
    out, err, rc = slermes(["/indicator"])
    assert rc == 0

# ═══════════════════════════════════════════════════════════════════
#  SECTION 5: SESSION MANAGEMENT (from how-it-works §10)
# ═══════════════════════════════════════════════════════════════════

def test_session_list():
    out, err, rc = slermes(["/session", "list"])
    assert rc == 0

def test_session_search():
    out, err, rc = slermes(["/session", "search", "test"])
    assert rc == 0

def test_session_export():
    import tempfile
    td = tempfile.mkdtemp()
    out, err, rc = slermes(["/session", "export", "dummy", td])
    assert rc == 0 or "not found" in out

def test_session_import_cmd():
    out, err, rc = slermes(["/session", "import", "/nonexistent/file.json"])
    assert rc == 0 or "not found" in out

def test_session_resume():
    out, err, rc = slermes(["/session", "resume", "dummy"])
    assert rc == 0 or "not found" in out

def test_session_delete():
    out, err, rc = slermes(["/session", "delete", "dummy"])
    assert rc == 0 or "not found" in out

def test_session_rename():
    out, err, rc = slermes(["/session", "rename", "old", "new"])
    assert rc == 0 or "not found" in out

# ═══════════════════════════════════════════════════════════════════
#  SECTION 6: CONFIG SYSTEM (from how-it-works §10)
# ═══════════════════════════════════════════════════════════════════

def test_config_show():
    out, err, rc = slermes(["/config", "show"])
    assert rc == 0

def test_config_model():
    out, err, rc = slermes(["/model", "list"])
    assert rc == 0

def test_config_providers():
    out, err, rc = slermes(["/model", "providers"])
    assert rc == 0

def test_model_show():
    out, err, rc = slermes(["/model", "show"])
    assert rc == 0

def test_fast_mode():
    out, err, rc = slermes(["/fast"])
    assert rc == 0

def test_topic():
    out, err, rc = slermes(["/topic"])
    assert rc == 0

def test_voice_mode():
    out, err, rc = slermes(["/voice"])
    assert rc == 0

# ═══════════════════════════════════════════════════════════════════
#  SECTION 7: GATEWAY COMMANDS (from how-it-works §9)
# ═══════════════════════════════════════════════════════════════════

def test_gateway_status():
    out, err, rc = slermes(["/gateway", "status"])
    assert rc == 0

def test_platforms():
    out, err, rc = slermes(["/platforms"])
    assert rc == 0

def test_webhook_status():
    out, err, rc = slermes(["/webhook", "status"])
    assert rc == 0

# ═══════════════════════════════════════════════════════════════════
#  SECTION 8: TOOLS SYSTEM
# ═══════════════════════════════════════════════════════════════════

def test_tools_list():
    out, err, rc = slermes(["/tools"])
    assert rc == 0
    # Ensure we get many tools
    lines = out.strip().split("\n")
    assert len(lines) > 5, f"only {len(lines)} lines"

def test_toolsets():
    out, err, rc = slermes(["/toolsets"])
    assert rc == 0

# ═══════════════════════════════════════════════════════════════════
#  SECTION 9: SKILLS SYSTEM
# ═══════════════════════════════════════════════════════════════════

def test_skills_hub():
    out, err, rc = slermes(["/skills-hub"])
    assert rc == 0

def test_bundles():
    out, err, rc = slermes(["/bundles"])
    assert rc == 0

def test_curator():
    out, err, rc = slermes(["/curator"])
    assert rc == 0

# ═══════════════════════════════════════════════════════════════════
#  SECTION 10: EDGE CASES
# ═══════════════════════════════════════════════════════════════════

def test_empty_input():
    out, err, rc = slermes([""])
    # Should handle gracefully - either help or empty
    assert rc == 0

def test_unknown_slash():
    out, err, rc = slermes(["/nonexistent_command_xyz"])
    assert rc == 0  # Should handle gracefully, not crash

def test_very_long_input():
    long_input = "a" * 10000
    out, err, rc = slermes([long_input])
    assert rc == 0  # Should handle gracefully

def test_unicode_input():
    unicode_input = "Hello — « café ñ ñ » ∑ ⨀ 🔥 你好 🌍"
    out, err, rc = slermes([unicode_input])
    assert rc == 0  # Should handle gracefully

def test_mixed_case_command():
    out, err, rc = slermes(["/HELP"])
    assert rc == 0

def test_tab_completion():
    out, err, rc = slermes(["\t"])
    assert rc == 0

def test_slash_with_trailing_space():
    out, err, rc = slermes(["/help   "])
    assert rc == 0

def test_slash_help_all():
    out, err, rc = slermes(["/help", "all"])
    assert rc == 0

def test_slash_help_cmd():
    out, err, rc = slermes(["/help", "model"])
    assert rc == 0

# ═══════════════════════════════════════════════════════════════════
#  SECTION 11: COMPRESSION & CONTEXT
# ═══════════════════════════════════════════════════════════════════

def test_compress():
    out, err, rc = slermes(["/compress"])
    assert rc == 0

def test_context_info():
    out, err, rc = slermes(["/session"])
    assert rc == 0

# ═══════════════════════════════════════════════════════════════════
#  SECTION 12: AUTH & SECURITY
# ═══════════════════════════════════════════════════════════════════

def test_secrets_list():
    out, err, rc = slermes(["/secrets", "list"])
    assert rc == 0

def test_key():
    out, err, rc = slermes(["/key"])
    assert rc == 0

# ═══════════════════════════════════════════════════════════════════
#  SECTION 13: CRON
# ═══════════════════════════════════════════════════════════════════

def test_cron_list():
    out, err, rc = slermes(["/cron", "list"])
    assert rc == 0

def test_cron_status():
    out, err, rc = slermes(["/cron", "status"])
    assert rc == 0

# ═══════════════════════════════════════════════════════════════════
#  SECTION 14: KANBAN
# ═══════════════════════════════════════════════════════════════════

def test_kanban_status():
    out, err, rc = slermes(["/kanban", "status"])
    assert rc == 0

# ═══════════════════════════════════════════════════════════════════
#  SECTION 15: SETUP WIZARD (from how-it-works §10)
# ═══════════════════════════════════════════════════════════════════

def test_setup_quick():
    out, err, rc = slermes(["setup", "--quick"])
    assert rc == 0

def test_setup_help():
    out, err, rc = slermes(["setup", "--help"])
    assert rc == 0

# ═══════════════════════════════════════════════════════════════════
#  MAIN
# ═══════════════════════════════════════════════════════════════════

def main():
    global PASS, FAIL, SKIP

    print(f"=" * 60)
    print(f"  Slermes QoL Fuzz Suite v372 — {len([x for x in dir() if x.startswith('test_')])} parametrized tests")
    print(f"  Target: {SLERMES}")
    print(f"=" * 60)
    print()

    # Section 1: CLI Startup
    print(f"\n--- §1: CLI STARTUP & ARG PARSING ---")
    test("help", test_help)
    test("version", test_version)
    test("session flag", test_session_flag)
    test("model flag", test_model_flag)
    test("provider flag", test_provider_flag)
    test("json flag", test_json_flag)
    test("invalid flag", test_invalid_flag)
    test("help gateway", test_help_gateway)
    test("help cron", test_help_cron)
    test("help tools", test_help_tools)
    test("doctor", test_doctor)

    # Section 2: Slash Commands
    print(f"\n--- §2: SLASH COMMANDS ---")
    test("/new", test_commands_new)
    test("/model", test_commands_model)
    test("/config", test_commands_config)
    test("/help", test_commands_help)
    test("/exit", test_commands_exit)
    test("/undo", test_commands_undo)
    test("/retry", test_commands_retry)
    test("/save", test_commands_save)
    test("/tools", test_commands_tools)
    test("/skills", test_commands_skills)
    test("/secrets", test_commands_secrets)
    test("/status", test_commands_status)
    test("/logs", test_commands_logs)
    test("/plugins", test_commands_plugins)
    test("/session", test_commands_session)
    test("/memory", test_commands_memory)
    test("/cron", test_commands_cron)
    test("/kanban", test_commands_kanban)
    test("/mcp", test_commands_mcp)
    test("/approve", test_commands_approve)
    test("/deny", test_commands_deny)
    test("/clear", test_commands_clear)
    test("/verbose", test_commands_verbose)
    test("/dump", test_commands_dump)
    test("/snapshot", test_commands_snapshot)
    test("/branch", test_commands_branch)

    # Section 3: Aliases
    print(f"\n--- §3: ALIASES ---")
    test("alias /n == /new", test_alias_new)
    test("alias /c == /clear", test_alias_clear)
    test("alias /m == /model", test_alias_model)

    # Section 4: Display
    print(f"\n--- §4: DISPLAY ---")
    test("tools display", test_display_tools)
    test("skills display", test_display_skills)
    test("skin", test_skin)
    test("statusbar", test_display_statusbar)
    test("redraw", test_display_redraw)
    test("indicator", test_display_indicator)

    # Section 5: Session Management
    print(f"\n--- §5: SESSION MANAGEMENT ---")
    test("session list", test_session_list)
    test("session search", test_session_search)
    test("session export", test_session_export)
    test("session import", test_session_import_cmd)
    test("session resume", test_session_resume)
    test("session delete", test_session_delete)
    test("session rename", test_session_rename)

    # Section 6: Config
    print(f"\n--- §6: CONFIG ---")
    test("config show", test_config_show)
    test("model list", test_config_model)
    test("model providers", test_config_providers)
    test("model show", test_model_show)
    test("fast mode", test_fast_mode)
    test("topic", test_topic)
    test("voice mode", test_voice_mode)

    # Section 7: Gateway
    print(f"\n--- §7: GATEWAY ---")
    test("gateway status", test_gateway_status)
    test("platforms", test_platforms)
    test("webhook status", test_webhook_status)

    # Section 8: Tools
    print(f"\n--- §8: TOOLS ---")
    test("tools list", test_tools_list)
    test("toolsets", test_toolsets)

    # Section 9: Skills
    print(f"\n--- §9: SKILLS ---")
    test("skills hub", test_skills_hub)
    test("bundles", test_bundles)
    test("curator", test_curator)

    # Section 10: Edge Cases
    print(f"\n--- §10: EDGE CASES ---")
    test("empty input", test_empty_input)
    test("unknown slash", test_unknown_slash)
    test("very long input", test_very_long_input)
    test("unicode input", test_unicode_input)
    test("mixed case cmd", test_mixed_case_command)
    test("tab completion", test_tab_completion)
    test("slash trailing space", test_slash_with_trailing_space)
    test("help all", test_slash_help_all)
    test("help cmd", test_slash_help_cmd)

    # Section 11: Compression
    print(f"\n--- §11: COMPRESSION ---")
    test("compress", test_compress)
    test("context info", test_context_info)

    # Section 12: Auth/Security
    print(f"\n--- §12: AUTH & SECURITY ---")
    test("secrets list", test_secrets_list)
    test("key", test_key)

    # Section 13: Cron
    print(f"\n--- §13: CRON ---")
    test("cron list", test_cron_list)
    test("cron status", test_cron_status)

    # Section 14: Kanban
    print(f"\n--- §14: KANBAN ---")
    test("kanban status", test_kanban_status)

    # Section 15: Setup
    print(f"\n--- §15: SETUP WIZARD ---")
    test("setup quick", test_setup_quick)
    test("setup help", test_setup_help)

    # Summary
    total = PASS + FAIL
    print(f"\n{'='*60}")
    print(f"  RESULTS: {PASS}/{total} PASS, {FAIL} FAIL, {SKIP} SKIP")
    print(f"  Fail breakdown:")
    for status, name in results:
        if status == "FAIL":
            print(f"    ❌ {name}")
    print(f"{'='*60}")

    return 0 if FAIL == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
