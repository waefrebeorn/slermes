#!/usr/bin/env python3
"""Oracle for hermes_cli/plugins_ledger.py — gate w12_pluginsled_05da3f."""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

import hermes_cli.plugins_ledger as mod                # noqa: E402

M = mod.PluginLedgerMixin()


class Obj:                      # opaque registration/callback token
    def __init__(self, ident):
        self.id = ident


def want_for(c):
    fn = c["func"]
    if fn == "remove_identity":
        items = [Obj(x) for x in c["items"]]
        target = items[c["target_index"]]
        removed = M._remove_identity(items, target)
        return {"items": [v.id for v in items], "removed": removed}
    if fn == "remove_callback":
        key = c["key"]
        items = [Obj(x) for x in c["items"]]
        target = items[c["target_index"]]
        mapping = {key: items}
        M._remove_callback(mapping, key, target)
        remaining = mapping.get(key, [])
        return {"items": [v.id for v in remaining], "has_key": key in mapping}
    if fn == "restore_mapping":
        pool = {}

        def obj(s):
            if s is None:
                return None
            if s not in pool:
                pool[s] = Obj(s)
            return pool[s]

        key = c["key"]
        mapping = {key: obj(c["map_value"])} if c["map_value"] is not None else {}
        changed = M._restore_mapping(mapping, key, obj(c["current"]), obj(c["previous"]))
        val = mapping.get(key)
        return {"value": val.id if isinstance(val, Obj) else None, "changed": changed}
    raise ValueError("unknown func %r" % fn)


def main():
    total = mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("func")
        got = c.get("out")
        try:
            want = want_for(c)
        except Exception as exc:                       # noqa: BLE001
            print("MISMATCH %s: python raised %r" % (fn, exc))
            mism += 1
            continue
        if got != want:
            mism += 1
            print("MISMATCH %s: C=%r PY=%r" % (fn, got, want))
    if total == 0:
        print("ORACLE w12_pluginsled_05da3f: 0 cases (link failure in disguise)")
        return 1
    print("ORACLE w12_pluginsled_05da3f: %d cases, %d mismatches" % (total, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
