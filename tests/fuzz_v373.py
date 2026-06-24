#!/usr/bin/env python3
"""Slermes fuzz v373 — all CLI features, edge cases, non-interactive mode."""
import subprocess, sys, os

SLERMES = os.path.expanduser("~/hermes-agent-dev/slermes/slermes")
PASS = FAIL = 0; FAILURES = []

def run(cmd, timeout=8):
    try:
        r = subprocess.run([SLERMES] + cmd, capture_output=True, timeout=timeout, text=True)
        return (r.stdout + r.stderr), r.returncode
    except subprocess.TimeoutExpired:
        return "(TIMEOUT)", -1
    except Exception as e:
        return str(e), -2

def t(name, cmd, rc_good=(0,), cont=None, not_cont=None):
    global PASS, FAIL
    out, rc = run(cmd)
    ok = True; reason = ""
    if rc not in rc_good:
        ok = False; reason = f"rc={rc} (expected {rc_good})"
    if cont and cont not in out:
        ok = False; reason = f"missing '{cont[:50]}'"
    if not_cont and not_cont in out:
        ok = False; reason = f"contains '{not_cont[:50]}'"
    if ok:
        PASS += 1
    else:
        FAIL += 1; FAILURES.append((name, reason, out[:100]))

def section(s):
    print(f"\n{'─'*50}\n{s}")

section("§1 HELP & VERSION")
t("--help", ["--help"])
t("--version", ["--version"])
t("doctor", ["doctor"], cont="Doctor")

section("§2 HELP SUBCOMMANDS")
for s in ["gateway","cron","tools","doctor","logs","model","session","exit","skills","plugins","config"]:
    t(f"help {s}", ["help",s])

section("§3 SLASH CMD --HELP")
for c in ["/undo","/retry","/session","/memory","/kanban","/mcp","/approve",
          "/snapshot","/branch","/fast","/topic","/voice","/compress","/redraw",
          "/skin","/statusbar","/indicator","/toolsets","/key","/platforms",
          "/curator","/bundles","/skills-hub","/config","/cron","/gateway",
          "/model","/help","/new","/clear","/exit","/tools","/skills",
          "/secrets","/status","/logs","/plugins","/deny","/verbose","/dump"]:
    t(f"{c} --help", [c,"--help"])

section("§4 DIRECT CMDS (interactive-only expect rc=1, non-zero is ok)")
for c in ["/help","/new","/clear","/exit","/tools","/skills","/secrets",
          "/status","/logs","/plugins","/deny","/verbose","/dump","/model","/config"]:
    # These may return 1 in non-interactive mode (need session) — just don't crash
    t(f"{c} direct", [c], rc_good=(0,1))

section("§5 SUBCOMMANDS")
for cmd,args,expect in [
    ("/model",["list"],None), ("/model",["providers"],None),
    ("/model",["show"],None), ("/config",["show"],None),
    ("/cron",["list"],None), ("/cron",["status"],None),
    ("/secrets",["list"],None), ("/help",["all"],None),
    ("/help",["model"],None), ("/help",["tools"],None),
]:
    t(f"{' '.join([cmd]+args)}", [cmd]+args, rc_good=(0,1))

section("§6 EDGE CASES")
t("unknown slash", ["/nonexistent_xyz"], rc_good=(1,), cont="Unknown command")
t("all caps /HELP", ["/HELP"], rc_good=(0,1))
t("mixed /Help", ["/Help"], rc_good=(0,1))
t("trailing space", ["/help   "], rc_good=(0,1))
t("multi slash", ["///help"], rc_good=(0,1))
t("empty arg", [""], rc_good=(0,1))
t("single /", ["/"], rc_good=(0,1))
t("unicode /help", ["/help","été"], rc_good=(0,1))
t("numeric arg", ["/help","12345"], rc_good=(0,1))
t("emoji arg", ["/help","🔥🚀🌟"], rc_good=(0,1))
t("long arg", ["/help", "a"*500], rc_good=(0,1))

section("§7 FLAGS")
t("--session flag", ["--session","test","--help"])
t("--model flag", ["--model","gpt-4","--help"])
t("--provider flag", ["--provider","openai","--help"])
t("--json flag", ["--json","--help"])
t("invalid flag", ["--nonexistent"], rc_good=(1,), cont="Error: unknown flag")

section("§8 ALIASES")
for a,f in [("/n","/new"),("/c","/clear"),("/m","/model"),("/h","/help"),
            ("/t","/tools"),("/s","/secrets"),("/sk","/skills"),
            ("/st","/status"),("/pl","/plugins"),("/cfg","/config")]:
    out1,rc1 = run([a,"--help"])
    out2,rc2 = run([f,"--help"])
    if rc1==0 and rc2==0:
        PASS += 1
    else:
        FAIL += 1; FAILURES.append((f"alias {a}",f"rc1={rc1} rc2={rc2}",""))

section("§9 SETUP")
t("setup --help", ["setup","--help"])
t("setup --quick", ["setup","--quick"])

section("§10 MCP & GATEWAY")
t("/mcp --help", ["/mcp","--help"])
t("/gateway --help", ["/gateway","--help"])
t("help mcp", ["help","mcp"])
t("help gateway start", ["help","gateway","start"])
t("help gateway stop", ["help","gateway","stop"])

section("§11 CRON & KANBAN")
t("/cron --help", ["/cron","--help"])
t("/kanban --help", ["/kanban","--help"])

section("§12 VERSION")
t("version str", ["--version"], cont="slermes")
t("version cmd", ["version"], cont="WuBu")

total = PASS+FAIL
print(f"\n{'='*50}")
print(f"  FUZZ v373: {PASS}/{total} PASS, {FAIL} FAIL")
for s,r,o in FAILURES: print(f"    ❌ {s}: {r}")
exit(0 if FAIL==0 else 1)
