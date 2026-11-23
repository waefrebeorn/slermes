#!/usr/bin/env python3
"""Differential oracle for hermes_cli/vault.py (gate w6_vault_9ab785).

The deterministic surface of the module is the argparse tree built by
register_cli() and the handler dispatch in vault_command().  Both are derived
here from the LIVE module (not hardcoded) so upstream edits are caught.
"""
import argparse, importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

PKG = "w6_vault_live_pkg"
pkg = types.ModuleType(PKG)
pkg.__path__ = [Q + "/hermes_cli"]
sys.modules[PKG] = pkg
spec = importlib.util.spec_from_file_location(PKG + ".vault", Q + "/hermes_cli/vault.py")
live = importlib.util.module_from_spec(spec)
sys.modules[PKG + ".vault"] = live
spec.loader.exec_module(live)

parser = argparse.ArgumentParser(prog="hermes")
live.register_cli(parser)
VSub = next(a for a in parser._actions
            if isinstance(a, argparse._SubParsersAction) and a.dest == "vault_action")


def get_parser(path):
    cur = parser
    for part in (path or "").split("/"):
        if not part:
            continue
        sub = next((a for a in cur._actions if isinstance(a, argparse._SubParsersAction)), None)
        if sub is None or part not in sub.choices:
            return None
        cur = sub.choices[part]
    return cur


def find_action(pr, opt):
    for a in pr._actions:
        if opt in list(a.option_strings) + [a.dest]:
            return a
    return None


def compute(r):
    func = r.get("func")
    if func == "actions":
        return list(VSub.choices)
    if func == "handler":
        action = r.get("action", "")
        if not action:
            return None  # vault_command falls back to _cmd_list
        pr = get_parser(action + ("/" + r["subaction"] if r.get("subaction") else ""))
        if pr is None:
            return None
        h = pr.get_default("_vault_handler")
        return getattr(h, "__name__", None)
    if func == "argmeta":
        pr = get_parser(r.get("action", ""))
        if pr is None:
            return None
        a = find_action(pr, r.get("opt", ""))
        if a is None:
            return None
        return {
            "choices": list(a.choices) if a.choices is not None else None,
            "default": None if a.default is None else a.default,
            "required": bool(a.required),
            "metavar": a.metavar,
        }
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_vault_9ab785: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
