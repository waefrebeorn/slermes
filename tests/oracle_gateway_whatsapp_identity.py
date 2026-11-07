#!/usr/bin/env python3
"""Oracle for gateway/whatsapp_identity.py JID helpers.

Harness (t_port_gateway_whatsapp_identity.c) emits per family:
  normalize: {family:"normalize", in:<str>, out:<str>} -> normalize_whatsapp_identifier
  tojid:    {family:"tojid",    in:<str>, out:<str>} -> to_whatsapp_jid
  mention:  {family:"mention",  in:<str>, out:<str>} -> normalize_whatsapp_mention_jid
All three are pure str->str. Seed re + the _BARE_PHONE_RE constant.
"""
import ast
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/whatsapp_identity.py"
FUNS = ["normalize_whatsapp_identifier", "to_whatsapp_jid", "normalize_whatsapp_mention_jid"]
_BARE_PHONE_RE = re.compile(r"^\+?[\d\s().\-]+$")


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re, "Any": object, "_BARE_PHONE_RE": _BARE_PHONE_RE}
    body = [n for n in ast.parse(src).body
            if isinstance(n, ast.FunctionDef) and n.name in FUNS]
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)

    fammap = {
        "normalize": "normalize_whatsapp_identifier",
        "tojid": "to_whatsapp_jid",
        "mention": "normalize_whatsapp_mention_jid",
    }
    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = ast.literal_eval(line)
        except ValueError:
            continue
        total += 1
        fam = c.get("family")
        fn = fammap.get(fam)
        if fn is None:
            continue
        want = ns[fn](c.get("in"))
        got = c.get("out")
        if want != got:
            mism += 1
            print(f"MISMATCH {fam} in={c.get('in')!r} C={got!r} PY={want!r}")
    if total == 0:
        print("ORACLE gateway_whatsapp_identity: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE gateway_whatsapp_identity: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
