#!/usr/bin/env python3
"""Oracle for port_portal_cli.c -- drives the LIVE portal_cli.py:_feature_state.

The live function wraps its result in `color(...)`, which emits ANSI codes; the
oracle compares the SELECTOR and the rendered text (the decision), not the
escape sequences.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\portal_cli.py"


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {"color": lambda text, _color=None: text,
          "Colors": type("C", (), {"GREEN": "green", "DIM": "dim"})()}
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name == "_feature_state":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def main():
    ns = extract()
    fn = ns.get("_feature_state")
    if fn is None:
        print("ORACLE portal_cli: _feature_state not found")
        return 2

    cfgs = {
        "managed": (1, 1, "anthropic"),
        "managed-notact": (1, 0, None),
        "active-provider": (0, 1, "anthropic"),
        "active-empty": (0, 1, ""),
        "active-null": (0, 1, None),
        "inactive": (0, 0, None),
        "inactive-with": (0, 0, "openai"),
    }
    # which selector each Python branch returns
    sel_of = {"nous": 0, "anthropic": 1, "active": 1, "openai": 1, "not configured": 2}

    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        managed, active, provider = cfgs.get(c.get("case"), (0, 0, None))

        class Feat:
            pass
        f = Feat()
        f.managed_by_nous = bool(managed)
        f.active = bool(active)
        f.current_provider = provider

        # strip ANSI codes - the colour is a rendering choice
        want_raw = fn(f, via_nous="nous")
        want = want_raw
        while "\x1b[" in want:
            i = want.find("\x1b[")
            j = want.find("m", i)
            want = want[:i] + want[j + 1:]

        got = c.get("out")
        got_sel = c.get("selector")
        want_sel = sel_of.get(want, -1)
        if got != want or got_sel != want_sel:
            mism += 1
            print(f"MISMATCH _feature_state case={c.get('case')} "
                  f"C=(sel={got_sel},text={got!r}) PY=(sel={want_sel},text={want!r})")

    print(f"ORACLE portal_cli: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
