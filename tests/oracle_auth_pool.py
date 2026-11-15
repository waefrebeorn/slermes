#!/usr/bin/env python3
"""oracle_auth_pool.py -- diffs the C auth.py credential-pool port.

ast-extracted from hermes_cli/auth.py. The cooldown merge is a SEAM on both
sides: the harness passes NULL and the oracle passes `lambda e, d, p: e`, so what
is under test is the token-generation logic, which is the part with the bug
history. _merge_disk_cooldown_state needs wall-clock and the PooledCredential
model and is out of scope.
"""
import ast
import json
import sys
import types

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/auth.py"
with open(SRC, encoding="utf-8") as f:
    text = f.read()
tree = ast.parse(text)

# _merge_pool_row_generation imports STATUS_DEAD at CALL time.
_cp = types.ModuleType("agent.credential_pool")
_cp.STATUS_DEAD = "dead"
_cp.STATUS_EXHAUSTED = "exhausted"
sys.modules.setdefault("agent", types.ModuleType("agent"))
sys.modules.setdefault("agent.credential_pool", _cp)

WANTED = {"_credential_token_pair", "_entry_ids", "_token_pairs_by_id",
          "_suppressed_source_list", "_merge_pool_row_generation"}
CONSTS = {"_POOL_STATUS_FIELDS", "_POOL_TOKEN_GENERATION_FIELDS"}

ns = {"Optional": object, "Any": object, "str": str, "bool": bool, "int": int,
      "list": list, "dict": dict, "len": len, "any": any,
      "__name__": "oracle_auth_pool_extracted"}

picked = set()
for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        exec(compile(ast.get_source_segment(text, node), SRC, "exec"), ns)
        picked.add(node.name)
    elif isinstance(node, ast.Assign):
        names = {t.id for t in node.targets if isinstance(t, ast.Name)}
        if names & CONSTS:
            exec(compile(ast.get_source_segment(text, node), SRC, "exec"), ns)
            picked |= (names & CONSTS)

missing = (WANTED | CONSTS) - picked
if missing:
    print(f"ORACLE auth_pool: FATAL, source not found: {sorted(missing)}")
    raise SystemExit(1)

# The cooldown seam, identical on both sides: the harness passes NULL and the
# oracle injects the identity, so what is under test is the token-generation
# logic -- the part with the bug history. _merge_disk_cooldown_state needs
# wall-clock plus the PooledCredential model and stays out of scope.
ns["_merge_disk_cooldown_state"] = lambda entry, disk, provider_id: entry


def _norm(v):
    return v if v is not None else ""


cases = mismatches = shown = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        c = json.loads(line)
    except json.JSONDecodeError:
        continue
    fn = c["fn"]
    cases += 1

    if fn == "pair":
        row = json.loads(c["arg"])
        want = {"isobj": isinstance(row, dict),
                "access": _norm(row.get("access_token")) if isinstance(row, dict) else "",
                "refresh": _norm(row.get("refresh_token")) if isinstance(row, dict) else ""}
        # the reference rejects non-str tokens the same way the C does
        if isinstance(row, dict):
            want["access"] = row["access_token"] if isinstance(row.get("access_token"), str) else ""
            want["refresh"] = row["refresh_token"] if isinstance(row.get("refresh_token"), str) else ""
        got = {"isobj": bool(c["isobj"]), "access": c["access"],
               "refresh": c["refresh"]}
    elif fn == "entryids":
        pool = json.loads(c["arg"])
        want = [e["id"] for e in pool
                if isinstance(e, dict) and isinstance(e.get("id"), str) and e.get("id")]
        got = c["out"]
    elif fn == "tokenpairs":
        pool = json.loads(c["arg"])
        want = [[e["id"],
                 e.get("access_token") if isinstance(e.get("access_token"), str) else "",
                 e.get("refresh_token") if isinstance(e.get("refresh_token"), str) else ""]
                for e in pool
                if isinstance(e, dict) and isinstance(e.get("id"), str) and e.get("id")]
        got = c["out"]
    elif fn == "suppressed":
        sup = json.loads(c["arg"])
        prov = c.get("prov", "")
        listed = sup.get(prov) if isinstance(sup, dict) else None
        if isinstance(listed, list):
            want = [x for x in listed if isinstance(x, str)]
            after = sup
        elif isinstance(listed, dict):
            sup[prov] = [str(k) for k in listed]
            want = [str(k) for k in listed]
            after = sup
        else:
            want = []
            after = sup
        got = {"out": c["out"], "after": json.loads(c["after"])}
        want = {"out": want, "after": after}
    elif fn == "merge":
        entry = json.loads(c["entry"])
        disk = json.loads(c["disk"])
        ba = None if c.get("ba") == "__none__" else c.get("ba")
        br = None
        want = ns["_merge_pool_row_generation"](
            entry, disk, "openrouter",
            base_pair=None if ba is None else (ba, br),
            status_cleared=bool(c.get("cleared")))
        got = c["out"]
        if isinstance(got, str) and got[:1] in ("[", "{"):
            try:
                got = json.loads(got)
            except json.JSONDecodeError:
                pass
    else:
        continue

    if got != want:
        mismatches += 1
        if shown < 12:
            print(f"MISMATCH {fn} {c.get('arg','')!r}: got {got!r} want {want!r}")
            shown += 1

print(f"ORACLE auth_pool: {cases} cases, {mismatches} mismatches")
raise SystemExit(1 if mismatches else 0)
