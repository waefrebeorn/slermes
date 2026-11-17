#!/usr/bin/env python3
"""Rewrite an oracle's `rebind_used` donor lists down to the modules that
actually DEFINE one of the names the oracle reads.

An early run of fix_oracle_moved_attrs.py emitted every sibling module, and
importing that many modules has real side effects -- `hermes_cli._old_updater`
prints a handoff banner and re-execs the update takeover, so the oracle died at
import time and reported "no output". Donors must be minimal.
"""
from __future__ import annotations

import ast
import importlib.util
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
sys.path.insert(0, str(REPO / "scripts"))
_spec = importlib.util.spec_from_file_location(
    "fx", REPO / "scripts" / "fix_oracle_moved_attrs.py")
fx = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(fx)

LINE = re.compile(
    r"^(\s*)_ldm\.rebind_used\(\s*(\w+)\s*,\s*['\"]\2['\"]\s*,\s*\[(.*?)\]\s*,\s*Path\(__file__\)\)",
    re.S | re.M,
)


def narrow(path: Path) -> str:
    raw = path.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in raw else "\n"
    src = raw.replace("\r\n", "\n")
    tree = ast.parse(src)

    aliases: dict[str, str] = {}
    for n in ast.walk(tree):
        if isinstance(n, ast.Import):
            for a in n.names:
                aliases[a.asname or a.name.split(".")[0]] = a.name
        elif isinstance(n, ast.ImportFrom) and n.module and n.level == 0:
            for a in n.names:
                if a.name != "*":
                    aliases[a.asname or a.name] = n.module + "." + a.name

    def repl(m: re.Match) -> str:
        indent, alias, _old = m.group(1), m.group(2), m.group(3)
        dotted = aliases.get(alias)
        if not dotted or "." not in dotted:
            return m.group(0)
        resolved = fx.resolve_module(dotted)
        if resolved is None:
            return m.group(0)
        have = fx.module_names(resolved)
        need = set(fx.used_attrs(tree, alias))
        missing = need - have
        if not missing:
            return m.group(0)
        pkg = dotted.rsplit(".", 1)[0]
        donors = ['"%s.%s"' % (pkg, c.stem) for c in fx.candidates_for(dotted)
                  if c != resolved and fx.module_names(c).intersection(missing)]
        if not donors:
            return m.group(0)
        return "%s_ldm.rebind_used(%s, '%s', [%s], Path(__file__))" % (
            indent, alias, alias, ", ".join(donors))

    out = LINE.sub(repl, src)
    if out == src:
        return "no change"
    path.write_bytes(out.replace("\n", nl).encode("utf-8"))
    return "rewrote"


if __name__ == "__main__":
    for arg in sys.argv[1:]:
        print(arg, "->", narrow(Path(arg)))
