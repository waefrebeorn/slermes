#!/usr/bin/env python3
"""Generate tests/fixtures/tui_server_flow3.jsonl for the third-wave
tui_gateway/server.py FLOW + leaf port (src/gateway/tui_server_flow3.c).

Each case is {"func": <live name>, "in": {...}} where "in" carries:
  - "state": injected mirror (sessions / live_transports / methods / sets / db)
  - "io":    scripted universal-seam results (bare array = per-call queue,
             {"__script__": true, "ret": v} = constant list/object, else constant;
             {"__raise__": kind, "message": m} = a raised exception)
  - call arguments for the target function.

>= 6 cases per function. Pure Python, no quarry import needed. Run:
  python tests/gen_tui_server_flow3_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "tui_server_flow3.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(kind, message=""):
    return {"__raise__": kind, "message": message}


def sess(**kw):
    d = {"session_key": "s1", "token": "s1"}
    d.update(kw)
    return d


def S(*sids):
    return {s: sess(session_key=s, token=s) for s in sids}


# ── trivial leaves ─────────────────────────────────────────────────────────
for _ in range(6):
    add("_DropTransport.write", obj={"x": 1})
for _ in range(6):
    add("_DropTransport.close")
for _ in range(6):
    add("_enable_gateway_prompts")
for _ in range(6):
    add("_clear_pending", sid=None if _ % 2 == 0 else "s1",
        io={"cancel": None if _ < 5 else R("SystemExit", "bye")})
for _ in range(6):
    add("register_method", name="m%d" % _, fn={"__fn__": "h%d" % _})

# ── transports ─────────────────────────────────────────────────────────────
for t in ["a", "b", "c", "d", "e", "f"]:
    add("register_live_transport", transport=t, io={})
add("register_live_transport", transport=None)
for t in ["a", "b", "c", "d", "e", "f"]:
    add("unregister_live_transport", transport=t,
        io={"forget": None if t != "f" else R("RuntimeError", "x")})

# ── tool / credential / config-health leaves ──────────────────────────────
for name in ["shell", "read", "write", "grep", "view", "edit"]:
    add("_tool_ctx", name=name, args={"path": "/x"},
        io={"build_tool_preview": "preview-%s" % name})
add("_tool_ctx", name="x", args={}, io={"build_tool_preview": R("ValueError", "bad")})
for key in ["", "k1", "k2", "k3", "k4", "k5"]:
    add("_probe_credentials", agent={"api_key": key, "provider": "openai"})
for cfg in [{}, {"a": None}, {"b": None, "a": None}, {"display": {"personality": "weird"}},
            {"display": {"personality": " DEFAULT "}}, "not-a-dict"]:
    add("_probe_config_health", cfg=cfg, io={"available_personalities": ["default", "none"]})

# ── approval payload / pending approvals ──────────────────────────────────
for i, data in enumerate([{}, {"smart_denied": True}, {"allow_session": False},
                          {"allow_permanent": False}, {"choices": ["once"]},
                          {"command": "secret", "description": "vault"}]):
    add("_approval_request_payload", data=data,
        io={"redact_approval": "[redacted]"})
for i in range(6):
    add("_open_requests", sid="s1",
        state={"sessions": S("s1")},
        io={"open_requests": ([] if i % 2 == 0 else [{"id": "r"}]) if i < 5 else R("RuntimeError", "x")})

# ── emit approval request ──────────────────────────────────────────────────

# ── image meta ─────────────────────────────────────────────────────────────

# ── crash / panic ──────────────────────────────────────────────────────────

# ── shutdown / reaper / ticker ────────────────────────────────────────────

# ── launch home / db cluster ───────────────────────────────────────────────

# ── profile home / scoped / configured cwd ────────────────────────────────

# ── write_json / emit / broadcast ─────────────────────────────────────────

# ── steering / session helpers ─────────────────────────────────────────────

# ── persist / marker / restart ─────────────────────────────────────────────

# ── usage cluster ──────────────────────────────────────────────────────────

# ── session info family ────────────────────────────────────────────────────

# ── mcp late refresh / hydrate / processes ─────────────────────────────────

# ── pet family ──────────────────────────────────────────────────────────────

# ── compute mcp rev ────────────────────────────────────────────────────────


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            counts[c["func"]] = counts.get(c["func"], 0) + 1
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    funcs = sorted(counts)
    short = [k for k in funcs if counts[k] < 6]
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(funcs), OUT))
    for k in funcs:
        print("  %-40s %d" % (k, counts[k]))
    if short:
        raise SystemExit("FUNCS WITH <6 CASES: %s" % short)


if __name__ == "__main__":
    main()
