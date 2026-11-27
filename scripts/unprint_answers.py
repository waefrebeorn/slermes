"""Turn "print the answer and return 0" ports into functions that RETURN the answer.

The dominant defect in this tree is a convention, not a stub body: the C function
computes the right value, prints it, and returns 0. A gate whose harness captures
stdout can read that print, so the gate looks green while every real caller in the
binary receives 0 -- the approval predicate always false, the timeout always 0.

For each named gate this script:
  1. finds the C function the gate calls,
  2. if its body is exactly `<setup>; printf(<one conversion>); return 0;` and the
     function returns int, rewrites the tail to `return <expr>;`,
  3. rewrites the harness to mix the RETURNED value instead of captured stdout,
  4. leaves the twin untouched (its norm already renders int/bool the same way).

Everything is written to a backup directory first, so a gate that turns red can be
restored with --revert. A red gate after conversion is a real divergence and is
reported as such -- never silently kept.

Usage:
    python scripts/unprint_answers.py --apply <gate>...
    python scripts/unprint_answers.py --revert <gate>...
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SRC = REPO / "src"
GATES = REPO / "tests" / "watercup" / "gates"
BACKUP = REPO / ".testbin" / "unprint_backup"

SIG = re.compile(r"^([A-Za-z_][\w \t\*]*?)\b(\w+)\s*\(([^;{}]*)\)\s*\{", re.M)
PRINT_TAIL = re.compile(
    r"\n(?P<indent>[ \t]*)printf\\((?P<fmt>\"[^\"]*\"),\s*(?P<arg>[^;]+?)\\);\s*\n[ \t]*return\s+0;\s*\n\}",
    re.S,
)
INT_FMT = re.compile(r"^(%[-+ #0]*\d*[diu]|%ld|%lld|%zu|%lu)\\n$")


_index: dict[str, tuple[Path, str, int, int, str]] | None = None


def _match_brace(txt: str, start: int) -> int:
    """Index of the '}' closing the '{' at `start`, skipping strings and comments.

    Brace counting alone truncates any function that prints JSON: `printf("{...}")`
    ends the body early and the rewrite eats the rest of the file.
    """
    depth = 0
    i = start
    n = len(txt)
    state = "code"
    while i < n:
        ch = txt[i]
        nxt = txt[i + 1] if i + 1 < n else ""
        if state == "code":
            if ch == "/" and nxt == "/":
                state = "line"
                i += 2
                continue
            if ch == "/" and nxt == "*":
                state = "block"
                i += 2
                continue
            if ch == '"':
                state = "str"
            elif ch == "'":
                state = "chr"
            elif ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return i
        elif state == "line":
            if ch == "\n":
                state = "code"
        elif state == "block":
            if ch == "*" and nxt == "/":
                state = "code"
                i += 2
                continue
        elif state in ("str", "chr"):
            if ch == "\\":
                i += 2
                continue
            if (state == "str" and ch == '"') or (state == "chr" and ch == "'"):
                state = "code"
        i += 1
    return n - 1


def index() -> dict[str, tuple[Path, str, int, int, str]]:
    """symbol -> (path, return type, body start, body end, body text); first def wins.

    Built once: reading 2,400 C files per gate is what made the first version take
    minutes for a batch of 2,000.
    """
    global _index
    if _index is not None:
        return _index
    idx: dict[str, tuple[Path, str, int, int, str]] = {}
    for root in ("src", "lib"):
        for path in (REPO / root).rglob("*.c"):
            try:
                with open(path, "r", encoding="utf-8", errors="ignore", newline="") as fh:
                    txt = fh.read()      # keep CRLF: rewriting line endings changes 151 lines of a file for one edit
            except OSError:
                continue
            for m in SIG.finditer(txt):
                name = m.group(2)
                if name in idx:
                    continue
                body_start = m.end()
                body_end = _match_brace(txt, body_start - 1)
                ret = m.group(1).strip().split()[-1].strip("*")
                idx[name] = (path, ret, body_start, body_end, txt[body_start:body_end])
    _index = idx
    return idx


def c_files():
    for root in ("src", "lib"):
        yield from (REPO / root).rglob("*.c")


def transform_body(body: str) -> tuple[str, str]:
    """Rewrite `printf(<int>) ; return 0;` tails into `return <int expr>;`.

    Most of these functions print inside branches:

        if (cond) { printf("1\\n"); return 0; }
        printf("0\\n");
        return 0;

    so the whole body is rewritten, not just its tail. Returns (new_body, verdict);
    verdict is "ok" only when no printf survives, which is what makes the function
    usable by a real caller.
    """
    new = body

    # printf("<literal int>\n"); [return 0;]  ->  return <literal>;
    new = re.sub(
        r'printf\(\s*"(-?\d+)\\n"\s*\);\s*(?:\r?\n[ \t]*return\s+0;)?',
        lambda m: f"return {m.group(1)};",
        new,
    )
    # printf("%d\n", EXPR); [return 0;]  ->  return EXPR;
    new = re.sub(
        r'printf\(\s*"(?:%[-+ #0]*\d*[diu]|%ld|%lld|%zu|%lu)\\n"\s*,\s*([^;]+?)\);\s*(?:\r?\n[ \t]*return\s+0;)?',
        lambda m: f"return {m.group(1).strip()};",
        new,
    )
    # a branch that printed twice can leave `return x;return x;` behind
    new = re.sub(r"return ([^;]+);\s*return \1;", r"return \1;", new)
    if "printf" in new:
        return body, "skip-mixed-output"
    if not re.search(r"\breturn\b", new):
        return body, "skip-no-return"
    return new, "would-convert"


def classify(fn: str) -> str:
    rec = index().get(fn)
    if not rec:
        return "no-def"
    _path, ret, _bs, _be, body = rec
    if ret not in ("int", "long", "bool"):
        return f"skip-ret {ret}"
    _new, verdict = transform_body(body)
    return verdict


def convert_gate(gate: str, apply: bool = True) -> str:
    gd = GATES / gate
    gj = gd / "gate.json"
    if not gj.exists():
        return "no-gate"
    g = json.loads(gj.read_text())
    fn = g["c_fn"]
    verdict = classify(fn)
    if verdict != "would-convert":
        return verdict
    if not apply:
        return "would-convert"

    path, _ret, body_start, body_end, body = index()[fn]
    new_body, verdict = transform_body(body)
    if verdict != "would-convert":
        return verdict
    with open(path, "r", encoding="utf-8", errors="ignore", newline="") as fh:
        txt = fh.read()
    # Replace the body in place rather than splicing by offset: offset splicing plus
    # newline translation rewrote every line ending in the file for a one-line edit.
    if body not in txt:
        return "skip-body-not-found"
    new_txt = txt.replace(body, new_body, 1)

    BACKUP.mkdir(parents=True, exist_ok=True)
    bak = BACKUP / (gate + "__" + path.name)
    if not bak.exists():
        shutil.copy2(path, bak)
    with open(path, "w", encoding="utf-8", newline="") as fh:
        fh.write(new_txt)
    # Positions in the index are offsets into the text we just rewrote; every symbol
    # defined later in the same file would land in the wrong place. Rebuild lazily.
    global _index
    _index = None

    # harness: mix the returned value
    h = gd / "harness.c"
    if h.exists():
        hb = BACKUP / (gate + "__harness.c")
        if not hb.exists():
            shutil.copy2(h, hb)
    h.write_text(harness_for(gate, fn, g), encoding="utf-8")
    return "converted"


def harness_for(gate: str, fn: str, g: dict) -> str:
    return f'''/* gate: {gate} -- measures the RETURNED answer.
 *
 * The port used to print this value and return 0, so a caller in the binary always
 * received 0 while a stdout-capturing gate could still read the print. The function
 * now returns the value; this harness mixes what the caller actually gets.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern int {fn}(const char *arg);

int main(int argc, char **argv) {{
    if (argc < 3) {{ fprintf(stderr, "usage: %s {fn} <input>...\\n", argv[0]); return 2; }}
    for (int i = 2; i < argc; i++) {{
        char buf[64];
        WCUP_BEGIN("{fn}");
        WCUP_BRANCH(argv[i][0] != '\\0');
        int r = {fn}(argv[i]);
        snprintf(buf, sizeof buf, "%d", r);
        WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\\n", cup);
        WCUP_END("{fn}");
    }}
    return 0;
}}
'''


def revert_gate(gate: str) -> str:
    restored = 0
    if not BACKUP.exists():
        return "no-backup"
    for bak in BACKUP.glob(gate + "__*"):
        target_name = bak.name.split("__", 1)[1]
        if target_name == "harness.c":
            target = GATES / gate / "harness.c"
        else:
            hits = [p for p in c_files() if p.name == target_name]
            if not hits:
                continue
            target = hits[0]
        shutil.copy2(bak, target)
        restored += 1
    return f"restored {restored}"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("gates", nargs="*")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--revert", action="store_true")
    ap.add_argument("--scan", help="file with one gate per line: count verdicts, list the convertible ones")
    a = ap.parse_args()

    if a.scan:
        names = [ln.strip() for ln in Path(a.scan).read_text().splitlines() if ln.strip()]
        counts: dict[str, int] = {}
        ok: list[str] = []
        for gate in names:
            gj = GATES / gate / "gate.json"
            verdict = classify(json.loads(gj.read_text())["c_fn"]) if gj.exists() else "no-gate"
            counts[verdict] = counts.get(verdict, 0) + 1
            if verdict == "would-convert":
                ok.append(gate)
        for k, v in sorted(counts.items(), key=lambda kv: -kv[1]):
            print(f"{v:6d}  {k}")
        out = Path(a.scan + ".convertible")
        out.write_text("\n".join(ok))
        print(f"convertible -> {out} ({len(ok)})")
        return 0

    if not a.gates:
        ap.error("name at least one gate, or use --scan")
    for gate in a.gates:
        if a.revert:
            print(f"{gate:70s} {revert_gate(gate)}")
        elif a.apply:
            print(f"{gate:70s} {convert_gate(gate, True)}")
        else:
            gj = GATES / gate / "gate.json"
            verdict = classify(json.loads(gj.read_text())["c_fn"]) if gj.exists() else "no-gate"
            print(f"{gate:70s} {verdict}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
