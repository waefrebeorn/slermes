"""twin for plugin_index_parse_entries.

Imports LIVE hermes_cli/plugin_index.py::_parse_entries and traces the SAME
ladder as the C harness. Value event per accepted entry: "name=<n>|repo=<r>".
"""
import importlib.util as ilu
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_plugin_index", os.path.join(DEV, "hermes_cli", "plugin_index.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_plugin_index"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def _valid_repo(repo):
    if not isinstance(repo, str) or not repo:
        return False
    return repo.count("/") == 1 and all(repo.split("/"))


def run(inputs):
    cups = []
    for raw_s in inputs:
        try:
            raw = json.loads(raw_s) if raw_s else None
        except ValueError:
            raw = None
        c = Cup("parse_entries")
        is_dict = isinstance(raw, dict)
        c.branch(is_dict)
        items = None
        if is_dict:
            has_key = "plugins" in raw
            pl = raw.get("plugins")
            items = pl if isinstance(pl, list) else None
            c.branch(has_key)
            # C treats absent key as empty list; present-but-not-a-list raises in
            # LIVE so such inputs are excluded from the walk.
            if not has_key:
                items = []
        else:
            is_list = isinstance(raw, list)
            c.branch(is_list)
            items = raw if is_list else []
        if isinstance(items, list):
            for item in items:
                c.loop()
                if not isinstance(item, dict):
                    c.branch(False)
                    continue
                c.branch(True)
                name = item.get("name")
                name_ok = isinstance(name, str) and name.strip()
                c.branch(name_ok)
                if not name_ok:
                    continue
                repo = item.get("repo")
                repo_ok = _valid_repo(repo)
                c.branch(repo_ok)
                if not repo_ok:
                    continue
                c.valstr("name=%s|repo=%s" % (name.strip(), repo.strip()))
        c.valstr("count=%d" % len(mod._parse_entries(raw)))
        cups.append(c.finalize())
    return cups
