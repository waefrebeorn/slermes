#!/usr/bin/env python3
"""Oracle for gateway/platforms/access_policy_mixin.py pure helpers
(port include/port_access_policy.h). The live helpers are indented methods
(inside a mixin class) reading self._dm_policy / self._group_policy / env
secrets / allow lists; the C port takes those as pre-resolved params. We
ast-extract the live methods + OPTIN_TRUTHY (descending into the ClassDef) and
drive them through a fake self whose collaborators resolve from the harness
inputs, so the oracle is the live code (not a C copy)."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

NAMES = {"_open_dm_opted_in", "_is_dm_allowed", "_is_dm_intake_allowed",
         "_is_group_allowed", "_entry_matches"}


def extract(path):
    src = open(path, encoding="utf-8").read()
    g = {"Iterable": None, "Any": None, "get_scoped_secret": lambda *a, **k: ""}
    for n in ast.parse(src).body:
        body = n.body if isinstance(n, ast.ClassDef) else [n]
        for m in body:
            if isinstance(m, ast.FunctionDef) and m.name in NAMES:
                exec(compile(ast.Module(body=[m], type_ignores=[]), "apm", "exec"), g)
            elif isinstance(m, ast.Assign):
                for tar in m.targets:
                    if isinstance(tar, ast.Name) and tar.id == "OPTIN_TRUTHY":
                        exec(compile(ast.Module(body=[m], type_ignores=[]), "apm", "exec"), g)
    return g


class _Self:
    def __init__(self, env, g, dm_policy=None, group_policy=None, entries=None):
        self._g = g
        self._dm_policy = dm_policy
        self._group_policy = group_policy
        self._entries = entries or []
        self._group_allow_from = self._entries
        self._env = env
        self._open_dm_opted_in = None  # bound to live later
    def _allow_all_env_names(self):
        return [f"ENV{i}" for i in range(len(self._env))]
    def _live_dm_allow_from(self):
        return self._entries
    def _entry_matches(self, entries, target):
        return target in entries


def _arr(v):
    return v if isinstance(v, list) else json.loads(v)


def main() -> int:
    g = extract("C:/Users/eman5/hermes-agent/gateway/platforms/access_policy_mixin.py")
    for k in NAMES:
        if k not in g:
            print(f"ORACLE access_policy: live {k} not extracted")
            return 1
    _open, _dm, _dmi, _grp = g["_open_dm_opted_in"], g["_is_dm_allowed"], \
        g["_is_dm_intake_allowed"], g["_is_group_allowed"]

    total, mism = 0, 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fam = c.get("family")
        env = [c.get("e0", ""), c.get("e1", "")] if "e0" in c else []
        def scoped(name, _d=""):
            i = int(name.replace("ENV", "")) if name.startswith("ENV") else -1
            return env[i] if 0 <= i < len(env) else ""
        g["get_scoped_secret"] = scoped
        if fam == "optin":
            want = bool(_open(_Self(env, g)))
            if want != bool(c.get("out")):
                mism += 1
                print(f"MISMATCH optin case{c.get('case')} C={c.get('out')} PY={want}")
        elif fam == "match":
            entries = _arr(c["entries"])
            want = c["target"] in entries
            if want != bool(c.get("out")):
                mism += 1
                print(f"MISMATCH match case{c.get('case')} C={c.get('out')} PY={want}")
        elif fam == "dma":
            inst = _Self(env, g, dm_policy=c["policy"], entries=_arr(c["allow"]))
            inst._open_dm_opted_in = lambda: bool(_open(inst))
            want = bool(_dm(inst, c["sender"]))
            if want != bool(c.get("out")):
                mism += 1
                print(f"MISMATCH dma case{c.get('case')} C={c.get('out')} PY={want}")
        elif fam == "dmi":
            inst = _Self(env, g, dm_policy=c["policy"], entries=_arr(c["allow"]))
            inst._open_dm_opted_in = lambda: bool(_open(inst))
            inst._is_dm_allowed = lambda sender: bool(_dm(inst, sender))
            want = bool(_dmi(inst, c["sender"]))
            if want != bool(c.get("out")):
                mism += 1
                print(f"MISMATCH dmi case{c.get('case')} C={c.get('out')} PY={want}")
        elif fam == "grp":
            inst = _Self([], g, group_policy=c["policy"], entries=_arr(c["allow"]))
            want = bool(_grp(inst, c["chat"]))
            if want != bool(c.get("out")):
                mism += 1
                print(f"MISMATCH grp case{c.get('case')} C={c.get('out')} PY={want}")
    if total == 0:
        print("ORACLE access_policy: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE access_policy: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
