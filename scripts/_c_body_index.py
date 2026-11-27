"""Index of C function bodies, and the placeholder detector.

Measured on this tree (3,483 port symbols with a definition):
  1,594 (45.8%) have EVERY return statement a constant
  1,370 (39.3%) both print (printf/fprintf/puts) and always return 0/NULL/false/-1

A function like that does not compute anything: the gate that wraps it hashes the
same constant for every input, so it reads NON-DISCRIMINATING or (worse) agrees with
a constant Python answer by accident. This is the largest single reason the fleet
number lied, and it is a property of the C code, not of the fixtures or the twin.
"""
from __future__ import annotations

import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]

SIG = re.compile(r"^[A-Za-z_][\w \t\*]*\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{", re.M)
PRINT = re.compile(r"\b(printf|fprintf|puts|fputs|dprintf)\s*\(")
RET = re.compile(r"\breturn\s+([^;]+);")

_cache: dict[str, tuple[bool, bool] | None] | None = None


def _build() -> dict[str, tuple[bool, bool]]:
    out: dict[str, tuple[bool, bool]] = {}
    for root in ("src", "lib"):
        for c in (REPO / root).rglob("*.c"):
            try:
                txt = c.read_text(errors="ignore")
            except OSError:
                continue
            txt = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
            for m in SIG.finditer(txt):
                name = m.group(1)
                if name in out:
                    continue
                i = m.end() - 1
                depth = 0
                while i < len(txt):
                    if txt[i] == "{":
                        depth += 1
                    elif txt[i] == "}":
                        depth -= 1
                        if depth == 0:
                            break
                    i += 1
                body = txt[m.end():i]
                rets = [r.strip() for r in RET.findall(body)]
                const = bool(rets) and all(r in ("0", "NULL", "false", "-1", "(0)") for r in rets)
                out[name] = (const, PRINT.search(body) is not None)
    return out


def index() -> dict[str, tuple[bool, bool]]:
    global _cache
    if _cache is None:
        _cache = _build()
    return _cache


def placeholder(symbol: str) -> bool:
    """True when the function both prints and never returns a computed value."""
    rec = index().get(symbol)
    if not rec:
        return False
    const, prints = rec
    return const and prints
