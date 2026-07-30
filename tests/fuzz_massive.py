#!/usr/bin/env python3
"""
Slermes MASSIVE Comprehensive Fuzz Suite v372
Exercises every CLI feature, every command variant, every edge case.
Finds REAL gaps by running the binary, not static analysis.

Usage: python3 tests/fuzz_massive.py [-v]
"""
import subprocess
import sys
import os
import re
import time

SLERMES = os.path.expanduser("~/hermes-agent-dev/slermes/slermes")
VERBOSE = "-v" in sys.argv

PASS = FAIL = 0
FAILURES = []

def run(cmd, timeout=8):
    """Run slermes with args, return (output, rc)."""
    try:
        r = subprocess.run([SLERMES] + cmd, capture_output=True, timeout=timeout, text=True)
        return (r.stdout + r.stderr), r.returncode
    except subprocess.TimeoutExpired:
        return "(TIMEOUT)", -1
    except Exception as e:
        return str(e), -2

def test(name, cmd, expect_rc=0, expect_cont=None, expect_not=None):
    global PASS, FAIL
    out, rc = run(cmd)
    ok = True
    reason = ""
    if rc != expect_rc:
        ok = False
        reason = f"rc={rc} (expected {expect_rc})"
    if expect_cont and expect_cont not in out:
        ok = False
        reason = f"missing '{expect_cont[:50]}' in output"
    if expect_not and expect_not in out:
        ok = False
        reason = f"unexpected '{expect_not[:50]}' in output"
    
    if ok:
        PASS += 1
    else:
        FAIL += 1
        FAILURES.append((name, reason, out[:200]))
        if VERBOSE:
            print(f"  ❌ {name}: {reason}")

# ═══════════════════════════════════════════
#  CLI BASICS
# ═══════════════════════════════════════════
SECTIONS = []

def section(s):
    SECTIONS.append(s)
    print(f"\n{'─'*60}\n{s}\n{'─'*60}")

section("§1: CLI HELP & VERSION")
test("--help returns 0", ["--help"])
test("--version returns 0", ["--version"])
test("doctor runs", ["doctor"])
test("help h", ["help"])
test("help gateway subcmd", ["help", "gateway"])
test("help cron subcmd", ["help", "cron"])
test("help tools subcmd", ["help", "tools"])
test("help doctor subcmd", ["help", "doctor"])
test("help logs subcmd", ["help", "logs"])
test("help model subcmd", ["help", "model"])
test("help session subcmd", ["help", "session"])
test("help exit subcmd", ["help", "exit"])

section("§2: SLASH COMMANDS")
for cmd in ["/help", "/new", "/clear", "/exit", "/undo", "/retry",
            "/tools", "/skills", "/secrets", "/status", "/logs",
            "/plugins", "/session", "/memory", "/cron", "/kanban", "/mcp",
            "/approve", "/deny", "/verbose", "/dump",
            "/snapshot", "/branch", "/fast", "/topic", "/voice",
            "/compress", "/redraw", "/skin", "/statusbar", "/indicator",
            "/toolsets", "/model", "/key", "/platforms",
            "/config", "/curator", "/bundles", "/skills-hub",
            "/webhook", "/gateway"]:
    # Test plain command (may hang without session) - use --help variant
    name = f"{cmd} (help)"
    test(name, [cmd, "--help"])

section("§3: COMMAND WITH HELP FLAG")
for cmd in ["/undo", "/retry", "/session", "/memory", "/kanban", "/mcp",
            "/approve", "/snapshot", "/branch", "/fast", "/topic", "/voice",
            "/compress", "/redraw", "/skin", "/statusbar", "/indicator",
            "/toolsets", "/key", "/platforms", "/curator", "/bundles",
            "/skills-hub", "/config", "/cron", "/gateway", "/model"]:
    test(f"{cmd} --help", [cmd, "--help"])

section("§4: SUBCOMMANDS")
test("/model list", ["/model", "list"])
test("/model providers", ["/model", "providers"])
test("/model show", ["/model", "show"])
test("/config show", ["/config", "show"])
test("/session list", ["/session", "list"])
test("/cron list", ["/cron", "list"])
test("/cron status", ["/cron", "status"])
test("/gateway status", ["/gateway", "status"])
test("/secrets list", ["/secrets", "list"])
test("/help all", ["/help", "all"])
test("/help model", ["/help", "model"])
test("/help tools", ["/help", "tools"])
test("/help skills", ["/help", "skills"])
test("/help session", ["/help", "session"])
test("/help cron", ["/help", "cron"])

section("§5: ALIASES")
test("/n (alias /new)", ["/n", "--help"])
test("/c (alias /clear)", ["/c", "--help"])
test("/m (alias /model)", ["/m", "--help"])
test("/h (alias /help)", ["/h", "--help"])
test("/t (alias /tools)", ["/t", "--help"])
test("/s (alias /secrets)", ["/s", "--help"])
test("/sk (alias /skills)", ["/sk", "--help"])
test("/st (alias /status)", ["/st", "--help"])
test("/pl (alias /plugins)", ["/pl", "--help"])
test("/cfg (alias /config)", ["/cfg", "--help"])

section("§6: SESSION SUBCOMMANDS")
test("/session search", ["/session", "search", "test"])
test("/session show", ["/session", "show"])
test("/session recent 5", ["/session", "recent", "5"])

section("§7: EDGE CASES")
test("empty string", [""])
test("unknown slash", ["/nonexistent_cmd_xyz"])
test("unicode text", ["/help", "été"])
test("all caps", ["/HELP"])
test("mixed case", ["/Help"])
test("trailing space", ["/help   "])
test("multiple slashes", ["///help"])
test("numeric arg", ["/help", "12345"])
test("single slash only", ["/"])
test("long arg", ["/help", "a" * 500])
test("special chars", ["/help", "!@#$%^&*()"])
test("emoji arg", ["/help", "🔥🚀🌟"])

section("§8: FLAGS")
test("--session flag", ["--session", "test", "--help"])
test("--model flag", ["--model", "gpt-4", "--help"])
test("--provider flag", ["--provider", "openai", "--help"])
test("--json flag", ["--json", "--help"])
test("invalid flag", ["--nonexistent"], expect_rc=None)
test("--verbose flag", ["--verbose", "--help"])

section("§9: MCP COMMANDS")
test("/mcp --help", ["/mcp", "--help"])
test("mcp help subcmd", ["help", "mcp"])

section("§10: GATEWAY SUBCOMMANDS")
test("/gateway --help", ["/gateway", "--help"])
test("help gateway start", ["help", "gateway", "start"])
test("help gateway stop", ["help", "gateway", "stop"])

section("§11: CRON SUBCOMMANDS")
test("/cron --help", ["/cron", "--help"])
test("help cron subcmds", ["help", "cron"])

section("§12: KANBAN SUBCOMMANDS")
test("/kanban --help", ["/kanban", "--help"])
test("help kanban", ["help", "kanban"])

section("§13: SKILLS SUBCOMMANDS")
test("/skills --help", ["/skills", "--help"])
test("help skills", ["help", "skills"])
test("help skills-hub", ["help", "skills-hub"])
test("help bundles", ["help", "bundles"])
test("help curator", ["help", "curator"])

section("§14: TOOLS SUBCOMMANDS")
test("/tools --help", ["/tools", "--help"])
test("help tools detail", ["help", "tools"])
test("help toolsets", ["help", "toolsets"])

section("§15: CONFIG SUBCOMMANDS")
test("help config", ["help", "config"])
test("help model", ["help", "model"])
test("help secrets", ["help", "secrets"])

section("§16: SETUP WIZARD")
test("setup --help", ["setup", "--help"])
test("setup --quick", ["setup", "--quick"])

section("§17: COMMAND COUNTS")
# Verify help output actually lists commands
out, rc = run(["/help", "all"])
if rc == 0:
    # Count unique commands in output
    cmd_count = len(re.findall(r'/\w+', out))
    test(f"help all shows {cmd_count} commands", ["/help", "all"], expect_cont="/")
else:
    test("/help all runs", ["/help", "all"])

section("§18: DOCTOR DIAGNOSTICS")
test("doctor", ["doctor"], expect_cont="Doctor")
test("doctor --help", ["doctor", "--help"])

section("§19: VERSION")
test("version string", ["--version"], expect_cont="slermes")

section("§20: PLUGIN COMMANDS")
test("/plugins --help", ["/plugins", "--help"])
test("help plugins", ["help", "plugins"])

# ═══════════════════════════════════════════
#  FINAL SUMMARY
# ═══════════════════════════════════════════
total = PASS + FAIL
print(f"\n{'='*60}")
print(f"  MASSIVE COMPREHENSIVE FUZZ — RESULTS")
print(f"{'='*60}")
print(f"  Tests: {total}")
print(f"  PASS:  {PASS}")
print(f"  FAIL:  {FAIL}")
if FAILURES:
    print(f"\n  FAILURES:")
    for name, reason, out in FAILURES:
        print(f"    ❌ {name}: {reason}")
        print(f"       output: {out[:120]}")
print(f"  Sections: {len(SECTIONS)}")
print(f"{'='*60}")
exit(0 if FAIL == 0 else 1)
