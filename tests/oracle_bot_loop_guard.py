#!/usr/bin/env python3
"""Oracle for gateway/bot_loop_guard.py pure coercion + settings helpers.

The module imports cleanly; replay it directly. Harness families:
  bool:  {family:"bool", raw:<json>, def:<0>, out:<0|1>}   -> _as_bool(raw, default)
  pos:   {family:"pos", raw:<json>, def:300.0, out:<float>} -> _as_positive
  pint:  {family:"pint", raw:<json>, def:20, out:<int>}    -> _as_positive_int
  set:   {family:"set", block:<json>, out:<json>}           -> settings_from_config(cfg)
          where cfg holds the block under gateway.bot_loop_guard (the live fn
          inline-imports cfg_get; inject a fake hermes_cli.config module).

Compare numbers canonically (libjson renders integral doubles as ints).
"""
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

PYROOT = "C:/Users/eman5/hermes-agent"


def _canon(x):
    if x is None:
        return ("null",)
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, (list, tuple)):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def main() -> int:
    sys.path.insert(0, PYROOT)
    import gateway.bot_loop_guard as blg

    # inline-import: settings_from_config does `from hermes_cli.config import cfg_get`
    # inside its body — inject a fake module that returns the block passed through.
    cfgf = types.ModuleType("hermes_cli.config")
    _held = {}
    def cfg_get(cfg, *path, default=None):
        node = cfg.get(path[0]) if isinstance(cfg, dict) else None
        for k in path[1: -1]:
            node = node.get(k) if isinstance(node, dict) else None
        return (node.get(path[-1]) if isinstance(node, dict) else None) \
            if node is not None else default
    cfgf.cfg_get = cfg_get
    sys.modules.setdefault("hermes_cli", types.ModuleType("hermes_cli"))
    sys.modules["hermes_cli.config"] = cfgf

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fam = c.get("family")
        if fam == "bool":
            want = blg._as_bool(c.get("raw"), bool(c.get("def")))
            got = c.get("out")
            if (1 if want else 0) != got:
                mism += 1
                print(f"MISMATCH bool case={c.get('case')} raw={c.get('raw')!r} C={got} PY={want}")
        elif fam == "pos":
            want = blg._as_positive(c.get("raw"), float(c.get("def")))
            got = float(c.get("out"))
            if want != got:
                mism += 1
                print(f"MISMATCH pos case={c.get('case')} raw={c.get('raw')!r} C={got} PY={want}")
        elif fam == "pint":
            want = blg._as_positive_int(c.get("raw"), int(c.get("def")))
            got = int(c.get("out"))
            if want != got:
                mism += 1
                print(f"MISMATCH pint case={c.get('case')} raw={c.get('raw')!r} C={got} PY={want}")
        elif fam == "set":
            cfg = {"gateway": {"bot_loop_guard": c.get("block")}}
            s = blg.settings_from_config(cfg)
            want = {"enabled": s.enabled, "max_events": s.max_events,
                    "window_seconds": s.window_seconds,
                    "cooldown_seconds": s.cooldown_seconds}
            got = c.get("out")
            if _canon(want) != _canon(got):
                mism += 1
                print(f"MISMATCH set case={c.get('case')} block={c.get('block')}\n"
                      f"  C={got}\n  PY={want}")
    if total == 0:
        print("ORACLE bot_loop_guard: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE bot_loop_guard: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
