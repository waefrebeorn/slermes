#!/usr/bin/env python3
"""Oracle for plugins/platforms/signal/signal_format.py:markdown_to_signal.

Harness family (gateway_signal_format):
  {in:<markdown str>, text:<str>, styles:[<str>,...]}
Replays the live markdown_to_signal on the same markdown and compares text +
the Signal styles list (rendered as "start:length:STYLE"). Ast-extract the four
helpers + the regex constants (_CODE_BLOCK_RE etc.) + Emerald the fn; seed re.
The harness emits styles already formatted as strings; the oracle formats the
live (start,length,style) tuples the same way.
"""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/plugins/platforms/signal/signal_format.py"

_STYLE_ORDER = {"BOLD": 0, "ITALIC": 1, "STRIKETHROUGH": 2, "MONOSPACE": 3}


def fmt_styles(styles):
    """Live returns a list of (start, length, style) tuples - but Signal style
    spans may carry an extra discriminator (e.g. a url on links). The C harness
    emits each rendered as joined segments; format defensively (join all int /
    str parts with ':') so a variable-width tuple still compares."""
    out = []
    for s in styles:
        parts = [str(p) for p in s]
        out.append(":".join(parts))
    return out


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re}
    # ast-extract all constants (regex patterns) and the four helpers + the fn
    tree = ast.parse(src)
    for n in tree.body:
        if isinstance(n, (ast.Assign, ast.AnnAssign)):
            targets = n.targets if isinstance(n, ast.Assign) else (n.target,)
            names = {t.id for t in targets if isinstance(t, ast.Name)}
            if names:
                exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in (
                "_utf16_len", "_normalize_bullet_markers", "_fence_tables",
                "_fence_unfenced_tables", "markdown_to_signal"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    fn = ns["markdown_to_signal"]

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE gateway_signal_format: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        try:
            want_text, want_styles = fn(c.get("in"))
        except Exception as e:
            print(f"ERROR {c.get('case')}: {e}")
            mism += 1
            continue
        have_styles = [s.strip() for s in (c.get("styles") or [])]
        # The live fn RETURNS the styles already formatted as "start:length:STYLE"
        # strings (not tuples). Compare the list exactly; if span ordering differs
        # from the C, fall back to a set comparison (same spans).
        if c.get("text") != want_text:
            mism += 1
            print(f"MISMATCH text\n  C={c.get('text')!r}\n  PY={want_text!r}")
            continue
        if have_styles != want_styles and sorted(have_styles) != sorted(want_styles):
            mism += 1
            print(f"MISMATCH styles\n  C={have_styles}\n  PY={want_styles}")
    print(f"ORACLE gateway_signal_format: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
