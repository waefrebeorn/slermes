"""Replace the junk fixture battery with an adversarial corpus.

Every generated gate ships the same five inputs -- "hello", "world", "", "test123",
"NULL" -- which is why 53 of the first 75 gates that the evidence ratchet admits read
NON-DISCRIMINATING: the fixture set never varies the answer, so the cup is identical
by construction and the gate proves nothing.

The field's answer to this is property-based differential testing: generate a wide,
structured input space and compare `ref(x) == impl(x)` on every sample (Hypothesis /
QuickCheck; shrinking keeps the minimal counterexample). We keep the generated corpus
deterministic -- the gate's cups must stay stable across runs -- and cover the input
classes that actually split a predicate:

  empty / whitespace / single char / prefixes of the interesting literal / unicode
  (including combining marks and emoji) / quotes, backslashes, JSON fragments /
  numbers and number-looking strings / booleans as text / path and URL shapes /
  separators (tab, pipe, comma, colon, semicolon) / long strings / case variants

Gates whose existing fixtures are structured ("a\\tb\\tc", "x|y") also get recombinations
of their own fields, so the structured arms are exercised instead of one canned row.

Usage:
    python scripts/expand_fixtures.py --list .testbin/evidence_candidates.txt [--apply]
        [--limit N] [--max-inputs 48]
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
GATES = REPO / "tests" / "watercup" / "gates"

BASE: list[str] = [
    # emptiness and whitespace
    "", " ", "  ", "\t", "\n", "\t\t",
    # single characters and case
    "a", "A", "0", "-1", "x", "X",
    # literal boundaries a predicate usually tests
    "0", "1", "-1", "true", "false", "TRUE", "False", "none", "None", "null", "NULL",
    "off", "OFF", "on", "yes", "no", "disabled", "auto",
    # numbers and number-looking strings
    "42", "3.14", "-0", "1e9", "0x10", "+7", " 7 ", "1_000", "007",
    # paths, urls, hosts
    "/", "/api", "/api/status", "/api/status/extension", "/etc/passwd", "C:\\\\Windows",
    "https://example.com/a?b=c", "example.com", "user@example.com",
    # json-ish and quoting
    "{}", "[]", '{"a": 1}', '["x"]', '"quoted"', "back\\\\slash", "a,b", "a;b", "a:b", "a|b",
    "a\\tb",
    # unicode
    "é", "日本", "🙂", "e\\u0301", "Ａ", "ß", "İ",
    # length and prefix relations
    "aaaa", "a" * 64, "a" * 512,
    # a few realistic payloads for parsers
    '{"model": "us.anthropic.claude-3", "stream": true}',
    "manual:cli", "manual:", "gateway", "pre_tool", "post_tool",
]

STRUCTURED_SEPS = ("\t", "|", ",", ":")


def structured_variants(existing: list[str]) -> list[str]:
    """Recombine the fields of the gate's own structured fixtures."""
    out: list[str] = []
    for sample in existing:
        for sep in STRUCTURED_SEPS:
            if sep not in sample:
                continue
            fields = sample.split(sep)
            if len(fields) < 2:
                continue
            for i in range(len(fields)):
                # one field at a time: empty, one, zero
                for repl in ("", "1", "0", "true", "false", "/api/status", "yes", "2h"):
                    mutated = list(fields)
                    mutated[i] = repl
                    out.append(sep.join(mutated))
                # drop a field
                if len(fields) > 2:
                    out.append(sep.join(fields[:i] + fields[i + 1:]))
    return out


def expand(gate: str, max_inputs: int) -> tuple[int, int] | None:
    gj = GATES / gate / "gate.json"
    if not gj.exists():
        return None
    g = json.loads(gj.read_text())
    old = [str(x) for x in (g.get("inputs") or [])]
    candidates = old + BASE + structured_variants(old)
    seen: set[str] = set()
    new: list[str] = []
    for item in candidates:
        if item in seen or "\x00" in item:      # argv cannot carry NUL
            continue
        seen.add(item)
        new.append(item)
        if len(new) >= max_inputs:
            break
    g["inputs"] = new
    g["fixtures"] = "adversarial-corpus"
    return old, new


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", required=True)
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--max-inputs", type=int, default=48)
    a = ap.parse_args()

    gates = [ln.strip() for ln in Path(a.list).read_text().splitlines() if ln.strip()]
    if a.limit:
        gates = gates[:a.limit]
    changed = 0
    for gate in gates:
        res = expand(gate, a.max_inputs)
        if not res:
            continue
        old, new = res
        if old == new:
            continue
        changed += 1
        print(f"  {gate[:66]:68s} {len(old):3d} -> {len(new):3d}")
        if a.apply:
            gj = GATES / gate / "gate.json"
            g = json.loads(gj.read_text())
            g["inputs"] = new
            g["fixtures"] = "adversarial-corpus"
            gj.write_text(json.dumps(g, indent=2) + "\n")
    print(f"{changed} of {len(gates)} gates {'updated' if a.apply else 'would change'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
