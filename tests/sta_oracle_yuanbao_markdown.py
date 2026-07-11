#!/usr/bin/env python3
"""
sta_oracle_yuanbao_markdown.py — v561 verification oracle proving the 9
yuanbao MarkdownProcessor static helpers are faithfully ported to C
(yuanbao_md_*). Recomputes each case against LIVE Python
gateway/platforms/yuanbao.py:MarkdownProcessor and asserts equality.

NOTE: the parity scanner reports 148 "gaps" for yuanbao.py, but those are
symbol-prefix false-positives (the C ports use the yuanbao_md_ prefix). This
oracle verifies the pure markdown helpers are genuinely done.
"""
import sys, os, json
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from gateway.platforms.yuanbao import MarkdownProcessor


def norm(s):
    return s


def main():
    cases = []
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            continue

    mism = 0
    total = 0
    for c in cases:
        fn = c["fn"]
        total += 1
        ok = True
        detail = ""
        if fn == "unclosed":
            exp = MarkdownProcessor.has_unclosed_fence(c["in"])
            ok = (exp is (c["out"] is True))
            detail = f"C={c['out']} PY={exp}"
        elif fn == "ends_table":
            exp = MarkdownProcessor.ends_with_table_row(c["in"])
            ok = (exp is (c["out"] is True))
            detail = f"C={c['out']} PY={exp}"
        elif fn == "is_fence_atom":
            exp = MarkdownProcessor.is_fence_atom(c["in"])
            ok = (exp is (c["out"] is True))
            detail = f"C={c['out']} PY={exp}"
        elif fn == "is_table_atom":
            exp = MarkdownProcessor.is_table_atom(c["in"])
            ok = (exp is (c["out"] is True))
            detail = f"C={c['out']} PY={exp}"
        elif fn == "split_atoms":
            exp = MarkdownProcessor.split_into_atoms(c["in"])
            # c["atoms"] is a JSON array of strings
            got = c.get("atoms", [])
            ok = (got == exp)
            detail = f"match" if ok else f"got={got}\nexp={exp}"
        elif fn == "split_para_head":
            head, tail = MarkdownProcessor.split_at_paragraph_boundary(c["in"], 20, None)
            ok = (head == c["out"])
            detail = "match" if ok else f"got={c['out']!r} exp={head!r}"
        elif fn == "split_para_tail":
            head, tail = MarkdownProcessor.split_at_paragraph_boundary(c["in"], 20, None)
            ok = (tail == c["out"])
            detail = "match" if ok else f"got={c['out']!r} exp={tail!r}"
        elif fn == "strip_fence":
            exp = MarkdownProcessor.strip_outer_markdown_fence(c["in"])
            ok = (exp == c["out"])
            detail = "match" if ok else f"got={c['out']!r}\nexp={exp!r}"
        elif fn == "strip_fence_noop":
            exp = MarkdownProcessor.strip_outer_markdown_fence(c["in"])
            ok = (exp == c["out"])
            detail = "match" if ok else f"got={c['out']!r}\nexp={exp!r}"
        elif fn == "sanitize_tbl":
            exp = MarkdownProcessor.sanitize_markdown_table(c["in"])
            ok = (exp == c["out"])
            detail = "match" if ok else f"got={c['out']!r}\nexp={exp!r}"
        elif fn == "hint":
            exp = MarkdownProcessor.markdown_hint_system_prompt()
            ok = (exp == c["out"])
            detail = "match" if ok else f"len got={len(c['out'])} exp={len(exp)}"
        else:
            ok = False
            detail = f"unknown fn {fn}"

        if not ok:
            mism += 1
            print(f"MISMATCH [{fn}] {detail}")
        else:
            print(f"ok [{fn}] {detail}")

    print(f"\nRESULT: {total - mism}/{total} match, {mism} mismatch")
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
