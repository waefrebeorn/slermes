#!/usr/bin/env python3
"""Differential oracle for w11_projecttre_cfc189 (tui_gateway/project_tree.py).

LIVE module loaded with sys.path.insert(0, quarry), parent-package bootstrap
and sys.modules registration. Pure path/lane/tree-helper functions are driven
directly; every fixture line is one function call.
"""
import importlib.util
import json
import os
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)
MOD = "tui_gateway/project_tree"


def _load_live():
    parts = MOD.split("/")
    for i in range(1, len(parts)):
        pname = ".".join(parts[:i])
        if pname in sys.modules:
            continue
        pdir = os.path.join(Q, *parts[:i])
        init = os.path.join(pdir, "__init__.py")
        sp = importlib.util.spec_from_file_location(
            pname, init, submodule_search_locations=[pdir])
        pm = importlib.util.module_from_spec(sp)
        sys.modules[pname] = pm
        try:
            sp.loader.exec_module(pm)
        except Exception:
            pass
    name = ".".join(parts)
    sp = importlib.util.spec_from_file_location(name, os.path.join(Q, MOD + ".py"))
    live = importlib.util.module_from_spec(sp)
    sys.modules[name] = live
    sp.loader.exec_module(live)
    return live


live = _load_live()


def _canon(v):
    return json.loads(json.dumps(v, ensure_ascii=False))


def drive(rec):
    fn = rec.get("func")
    if fn == "constants":
        return {"DEFAULT_BRANCH_LABEL": live.DEFAULT_BRANCH_LABEL,
                "NO_PROJECT_ID": live.NO_PROJECT_ID,
                "NO_PROJECT_LABEL": live.NO_PROJECT_LABEL}
    if fn == "segments":
        return live._segments(rec.get("path") or "")
    if fn == "base_name":
        return live.base_name(rec.get("path") or "")
    if fn == "path_key":
        return live._path_key(rec.get("path") or "")
    if fn == "lane_key":
        return live._lane_key(rec.get("path") or "")
    if fn == "branch_lane_id":
        return live._branch_lane_id(rec.get("root") or "", rec.get("branch") or "")
    if fn == "kanban_lane_id":
        return live._kanban_lane_id(rec.get("root") or "")
    if fn == "is_windows_path":
        return live._is_windows_path(rec.get("path") or "")
    if fn == "kanban_worktree_dir":
        return live.kanban_worktree_dir(rec.get("path") or "")
    if fn == "with_base_name":
        return live._with_base_name(rec.get("path") or "", rec.get("name") or "")
    if fn == "field":
        return live._field(rec.get("row") or {}, rec.get("key") or "")
    if fn == "session_time":
        return live._session_time(rec.get("session") or {})
    if fn == "last_active":
        return live._last_active(rec.get("sessions") or [])
    if fn == "placement":
        return live._placement(rec.get("repo_root") or "", rec.get("lane_key") or "",
                               rec.get("lane_label") or "", rec.get("lane_path") or "",
                               bool(rec.get("is_main")), bool(rec.get("is_kanban")),
                               bool(rec.get("is_git", True)))
    if fn == "trunk_placement":
        return live._trunk_placement(rec.get("root") or "", rec.get("branch") or "")
    if fn == "kanban_placement":
        return live._kanban_placement(rec.get("root") or "", rec.get("kanban_dir") or "")
    if fn == "repo_node":
        return live._repo_node(rec.get("root") or "", rec.get("label") or "")
    if fn == "project_node":
        return live._project_node(
            rec.get("pid") or "", rec.get("label") or "", rec.get("path"),
            rec.get("repos") or [], rec.get("session_count", 0),
            rec.get("last_active", 0), rec.get("previews") or [], rec.get("rows") or [],
            **(rec.get("flags") or {}))
    if fn == "lane_sort_key":
        return live._lane_sort_key(rec.get("group") or {})
    if fn == "disambiguate_labels":
        items = rec.get("items") or []
        live._disambiguate_labels(items)
        return items
    if fn == "stamp_profile":
        projects = rec.get("projects") or []
        live.stamp_profile(projects, rec.get("profile") or "")
        return projects
    if fn == "place_by_heuristic":
        return live._place_by_heuristic(rec.get("path") or "")
    return {"error": "unknown func"}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception:
            continue
        cases += 1
        got = rec.get("out")
        try:
            want = drive(rec)
        except BaseException as exc:
            mism += 1
            print("MISMATCH oracle-drive %s: %s" % (type(exc).__name__, exc), file=sys.stderr)
            continue
        if _canon(got) != _canon(want):
            mism += 1
            print("MISMATCH w11_projecttre_cfc189 case=%d func=%s\n  py=%s\n  c =%s" %
                  (cases, rec.get("func"), json.dumps(_canon(want), ensure_ascii=False),
                   json.dumps(_canon(got), ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w11_projecttre_cfc189: %d cases, %d mismatches" % (cases, mism))
    if cases == 0:
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
