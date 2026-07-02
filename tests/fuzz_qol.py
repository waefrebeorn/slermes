#!/usr/bin/env python3
"""
Slermes QoL Fuzz Suite v372 — runs all 80+ tests directly.
Each test uses timeout to prevent hangs.
"""
import subprocess
import sys
import os
import tempfile

SLERMES = os.path.expanduser("~/hermes-agent-dev/slermes/slermes")
PASS = FAIL = 0
RESULTS = []

def slermes(args, timeout=8):
    try:
        r = subprocess.run([SLERMES] + args, capture_output=True, timeout=timeout, text=True)
        return r.stdout + r.stderr, r.returncode
    except subprocess.TimeoutExpired:
        return "(timeout)", -1
    except Exception as e:
        return str(e), -2

def t(name, args, expect_rc=0):
    global PASS, FAIL
    out, rc = slermes(args)
    if rc == expect_rc:
        PASS += 1; RESULTS.append(("PASS", name))
    elif rc == -1:
        FAIL += 1; RESULTS.append(("TIMEOUT", name)); print(f"  ⏰ {name}: timeout")
    elif rc == -2:
        FAIL += 1; RESULTS.append(("ERROR", name)); print(f"  ❌ {name}: {out}")
    else:
        FAIL += 1; RESULTS.append(("FAIL", name)); print(f"  ❌ {name}: rc={rc} out={out[:100]}")

# §1: CLI STARTUP
t("help", ["--help"])
t("version", ["--version"])
t("doctor", ["doctor"])
t("help gateway", ["help", "gateway"])
t("help cron", ["help", "cron"])
t("help tools", ["help", "tools"])

# §2: SLASH COMMANDS
for cmd in ["/help", "/new", "/clear", "/undo", "/retry", "/save", "/exit",
            "/tools", "/skills", "/secrets", "/status", "/logs",
            "/plugins", "/session", "/memory", "/cron", "/kanban", "/mcp",
            "/approve", "/deny", "/verbose", "/dump",
            "/snapshot", "/branch", "/fast", "/topic", "/voice",
            "/compress", "/redraw", "/skin", "/statusbar", "/indicator",
            "/config", "/curator", "/bundles", "/skills-hub",
            "/toolsets", "/model", "/key", "/platforms"]:
    t(cmd, [cmd])

# §3: SUBCOMMANDS
for cmd in ["/model list", "/model providers", "/model show",
            "/config show", "/session list", "/session search test",
            "/cron list", "/cron status", "/gateway status",
            "/secrets list", "/help all", "/help model",
            "/webhook status", "/kanban status"]:
    t(cmd, cmd.split())

# §4: ALIASES
for alias, full in [("/n", "/new"), ("/c", "/clear"), ("/m", "/model"),
                     ("/h", "/help"), ("/t", "/tools"), ("/s", "/secrets")]:
    out1, rc1 = slermes([alias])
    out2, rc2 = slermes([full])
    if rc1 == rc2:
        PASS += 1; RESULTS.append(("PASS", f"alias {alias}"))
    else:
        FAIL += 1; RESULTS.append(("FAIL", f"alias {alias}: {alias} rc={rc1} {full} rc={rc2}"))

# §5: EDGE CASES
t("empty", [""])
t("unknown slash", ["/nonexistent_xyz"])
t("unicode", ["Hello — « café ∑ 🔥 你好"])
t("mixed case", ["/HELP"])
t("long input", ["a" * 5000])
t("trailing space", ["/help   "])
t("multiple slashes", ["///help"])
t("numeric input", ["12345"])
t("single char", ["/"])
t("newline input", ["--json", "invalid_json_test"])

# §6: FLAGS
t("session flag", ["--session", "test", "--help"])
t("model flag", ["--model", "gpt-4", "--help"])
t("provider flag", ["--provider", "openai", "--help"])
t("json flag", ["--json", "--help"])
t("invalid flag", ["--nonexistent"])
t("help doctor", ["help", "doctor"])
t("help logs", ["help", "logs"])

# SUMMARY
total = PASS + FAIL
print(f"\n{'='*60}")
print(f"RESULTS: {PASS}/{total} PASS, {FAIL} FAIL")
for s, n in RESULTS:
    if s != "PASS":
        print(f"  {s}: {n}")
exit(0 if FAIL == 0 else 1)
