#!/usr/bin/env python3
"""Regenerate tests/legacy_port_headers.txt -- port units whose header does not
follow include/port_<stem>.h. These are pre-existing; the naming gate fails only
on NEW violations, so the debt cannot grow silently."""
import re
from pathlib import Path

R = Path("C:/Users/eman5/slermes")
POP = re.compile(r"PoP:\s+(\S+)\s+@\s+([\w./]+\.py):")

rows = []
for c in sorted(R.glob("src/**/port_*.c")) + sorted(R.glob("src/**/*_ports.c")):
    t = c.read_text(encoding="utf-8", errors="replace")
    if not POP.search(t):
        continue
    stem = re.sub(r"^port_", "", c.stem)
    stem = re.sub(r"_ports$", "", stem)
    hdr_any = list((R / "include").glob(f"*{stem}*.h"))
    if hdr_any and not (R / "include" / f"port_{stem}.h").exists():
        rows.append((c.relative_to(R).as_posix(), hdr_any[0].name))

out = R / "tests" / "legacy_port_headers.txt"
body = ["# Port units whose header is not include/port_<stem>.h (pre-existing).",
        "# The naming gate fails on NEW violations only; regenerate with",
        "# scripts/gen_legacy_port_headers.py. Do not add to this list by hand.",
        ""]
body += [f"{c}\t{h}" for c, h in rows]
out.write_bytes(("\n".join(body) + "\n").encode("utf-8"))
print(f"wrote {len(rows)} rows to {out}")
