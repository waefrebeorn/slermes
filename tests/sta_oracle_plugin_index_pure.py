#!/usr/bin/env python3
"""sta_oracle_plugin_index_pure.py — oracle twin for plugin_index_pure.c.

Calls the LIVE hermes_cli/plugin_index.py pure helpers and emits one JSON line
per op. t_port_plugin_index_pure.c emits the same shape.

Fixture ops:
  {"op": "install_identifier", "repo": "owner/name", "subdir": "plugins/x"}
  {"op": "parse_entries", "raw": {"plugins": [{...}]}}
  {"op": "resolve_name", "entries": [{...}], "name": "Foo"}
  {"op": "search_index", "entries": [{...}], "term": "foo", "capability": null}
"""
import json
import os
import sys
import importlib.util


def _load_module():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(os.path.dirname(here)))
    cand = os.path.join(repo, "hermes_cli", "plugin_index.py")
    spec = importlib.util.spec_from_file_location("live_pindex", cand)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _entries_from_raw(mod, raw):
    return mod._parse_entries(raw)


def main():
    path = sys.argv[1]
    with open(path, encoding="utf-8") as f:
        spec = json.load(f)
    mod = _load_module()
    out = []
    for case in spec.get("ops", []):
        op = case.get("op")
        if op == "install_identifier":
            e = mod.PluginIndexEntry(name="x", repo=case.get("repo", ""), subdir=case.get("subdir"))
            out.append({"op": op, "out": e.install_identifier})
        elif op == "parse_entries":
            entries = _entries_from_raw(mod, case.get("raw"))
            out.append({"op": op, "out": [e.to_dict() for e in entries]})
        elif op == "resolve_name":
            entries = _entries_from_raw(mod, {"plugins": case.get("entries", [])})
            entry, cands = mod.resolve_name(entries, case.get("name", ""))
            out.append({"op": op, "out": {"entry": entry.name if entry else None,
                                          "candidates": [c.name for c in cands]}})
        elif op == "search_index":
            entries = _entries_from_raw(mod, {"plugins": case.get("entries", [])})
            res = mod.search_index(entries, case.get("term", ""), capability=case.get("capability"))
            out.append({"op": op, "out": [e.name for e in res]})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
