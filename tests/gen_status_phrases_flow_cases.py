#!/usr/bin/env python3
"""Generate tests/fixtures/status_phrases_flow.jsonl.

Inputs only (no expected values): the oracle recomputes the live reference.
Scripted seam results live in in.io (see the harness header). Representation
scope: ASCII posix paths, scalar str() values, no float values in str()-projected
positions (documented in the oracle/port header). Every ported entry gets >=5
cases; Exception-swallow vs BaseException-passthrough and fail-closed scripts are
covered explicitly.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "status_phrases_flow.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def sc(ret=None, raise_spec=None):
    s = {"__script__": True, "ret": ret}
    if raise_spec is not None:
        s["raise"] = raise_spec
    return s


def rz(kind, msg):
    return {"type": kind, "message": msg}


# ── _clean_phrase_list ──────────────────────────────────────────────────────
add("_clean_phrase_list", value=["a", "b", "a"])
add("_clean_phrase_list", value=["  x  ", "", "y", None, 0, False])
add("_clean_phrase_list", value=["z" * 161])
add("_clean_phrase_list", value=["z" * 160])
add("_clean_phrase_list", value="notlist")
add("_clean_phrase_list", value=None)
add("_clean_phrase_list", value=[1, 2, True])
add("_clean_phrase_list", value=[f"p{i}" for i in range(85)])
add("_clean_phrase_list", value=["  ", "\t\n", "ok"])

# ── _copy_catalog ───────────────────────────────────────────────────────────
add("_copy_catalog", catalog={"status": ["a"], "generic": ["b"]})
add("_copy_catalog", catalog={})
add("_copy_catalog", catalog={"status": []})
add("_copy_catalog", catalog={"x": ["1", "2"]})
add("_copy_catalog", catalog={"status": ["a"], "generic": ["b"], "extra": ["c"]})

# ── classify_status_context ─────────────────────────────────────────────────
for kind in ["heartbeat", "waiting", "long_running", "status", "  STATUS  ",
             "tool", "", None, "Heartbeat", 5, "waiting ", "generic"]:
    add("classify_status_context", kind=kind, tool_name="t", preview="p", args={"k": 1})

# ── _merge_phrase_mapping ───────────────────────────────────────────────────
add("_merge_phrase_mapping", catalog={"status": ["a"], "generic": ["b"]},
    section={"status": ["x", "y"]}, inherited_mode=None)
add("_merge_phrase_mapping", catalog={"status": ["a"], "generic": ["b"]},
    section={"mode": "replace", "status": ["x"]}, inherited_mode=None)
add("_merge_phrase_mapping", catalog={"status": ["a"], "generic": ["b"]},
    section={"phrases": {"status": ["x"]}}, inherited_mode=None)
add("_merge_phrase_mapping", catalog={"status": ["a"], "generic": ["b"]},
    section={"status": ["x", "x", "  ", "z"]}, inherited_mode=None)
add("_merge_phrase_mapping", catalog={"status": ["a"], "generic": ["b"]},
    section={"mode": "  REPLACE  ", "generic": ["g"]}, inherited_mode=None)
add("_merge_phrase_mapping", catalog={"status": ["a"], "generic": ["b"]},
    section={"status": []}, inherited_mode=None)
add("_merge_phrase_mapping", catalog={"status": ["a"], "generic": ["b"]},
    section={"status": ["x"]}, inherited_mode="replace")
add("_merge_phrase_mapping", catalog={"status": ["a"], "generic": ["b"]},
    section={"phrases": "notmap", "generic": ["g"]}, inherited_mode=None)
add("_merge_phrase_mapping", catalog={}, section={"status": ["s"], "generic": ["g"]},
    inherited_mode=None)

# ── _merge_phrase_file ──────────────────────────────────────────────────────
CAT = {"status": ["a"], "generic": ["b"]}
add("_merge_phrase_file", catalog=CAT, path="/home/cat.yaml", inherited_mode=None,
    io={"Path.read_text": "status:\n- x\n", "yaml.safe_load": {"status": ["x"]}})
add("_merge_phrase_file", catalog=CAT, path="/home/cat.yaml", inherited_mode="replace",
    io={"Path.read_text": "status:\n- x\n", "yaml.safe_load": {"status": ["x"]}})
add("_merge_phrase_file", catalog=CAT, path="/home/cat.yaml", inherited_mode=None,
    io={"Path.read_text": sc(raise_spec=rz("OSError", "boom"))})
add("_merge_phrase_file", catalog=CAT, path="/home/cat.yaml", inherited_mode=None,
    io={"Path.read_text": "text", "yaml.safe_load": sc(raise_spec=rz("ValueError", "bad yaml"))})
add("_merge_phrase_file", catalog=CAT, path="/home/cat.yaml", inherited_mode=None,
    io={"Path.read_text": sc(raise_spec=rz("SystemExit", "bye"))})
add("_merge_phrase_file", catalog=CAT, path="/home/cat.yaml", inherited_mode=None,
    io={"Path.read_text": "text", "yaml.safe_load": sc(raise_spec=rz("KeyboardInterrupt", "ki"))})
add("_merge_phrase_file", catalog=CAT, path="/home/cat.yaml", inherited_mode=None,
    io={"Path.read_text": "text", "yaml.safe_load": sc(raise_spec=rz("GeneratorExit", "ge"))})
add("_merge_phrase_file", catalog=CAT, path="/home/cat.yaml", inherited_mode=None,
    io={"Path.read_text": "- x\n", "yaml.safe_load": sc(["x"])})
add("_merge_phrase_file", catalog=CAT, path="/home/cat.yaml", inherited_mode=None, io={})
add("_merge_phrase_file", catalog=CAT, path="/home/cat.yaml", inherited_mode=None,
    io={"Path.read_text": "text", "yaml.safe_load": None})

# ── _iter_phrase_files ──────────────────────────────────────────────────────
add("_iter_phrase_files", base_dir="/home", raw_path="cat.yaml",
    io={"Path.expanduser": "cat.yaml", "Path.resolve": ["/home", "/home/cat.yaml"],
        "Path.is_file": True})
add("_iter_phrase_files", base_dir="/home", raw_path="cats",
    io={"Path.expanduser": "cats", "Path.resolve": ["/home", "/home/cats"],
        "Path.is_file": [False, True, True, True, False], "Path.is_dir": True,
        "Path.iterdir": sc(["/home/cats/b.yaml", "/home/cats/a.yml", "/home/cats/c.txt", "/home/cats/d"])})
add("_iter_phrase_files", base_dir="/home", raw_path="/etc/x.yaml",
    io={"Path.expanduser": "/etc/x.yaml"})
add("_iter_phrase_files", base_dir="/home", raw_path="../x",
    io={"Path.expanduser": "../x"})
add("_iter_phrase_files", base_dir="/home", raw_path=None,
    io={"Path.expanduser": "."})
add("_iter_phrase_files", base_dir="/home", raw_path="link",
    io={"Path.expanduser": "link", "Path.resolve": ["/home", "/elsewhere/link"]})
add("_iter_phrase_files", base_dir="/home", raw_path="cat.txt",
    io={"Path.expanduser": "cat.txt", "Path.resolve": ["/home", "/home/cat.txt"],
        "Path.is_file": True, "Path.is_dir": False})
add("_iter_phrase_files", base_dir="/home", raw_path="  ",
    io={"Path.expanduser": "."})
add("_iter_phrase_files", base_dir="/home", raw_path="cat.yaml", io={})  # fail-closed
add("_iter_phrase_files", base_dir="/home", raw_path="cats",
    io={"Path.expanduser": "cats", "Path.resolve": ["/home", "/home/cats"],
        "Path.is_file": [False, True], "Path.is_dir": True,
        "Path.iterdir": sc(["/home/cats/a.yml"])})
add("_iter_phrase_files", base_dir="/home", raw_path="dbl",
    io={"Path.expanduser": "//srv/x.yaml"})  # "//" root is absolute -> []
add("_iter_phrase_files", base_dir="", raw_path="cat.yaml",
    io={"Path.expanduser": "cat.yaml", "Path.resolve": ["", "/home/cat.yaml"]})  # empty base anchor mismatch
add("_iter_phrase_files", base_dir="/home//x", raw_path="cat.yaml",
    io={"Path.expanduser": "cat.yaml", "Path.resolve": ["/home/x", "/home/x/cat.yaml"],
        "Path.is_file": True})  # base double-slash normalization

# ── _merge_phrase_paths ─────────────────────────────────────────────────────
add("_merge_phrase_paths", catalog=CAT, paths=None, base_dir="/home", inherited_mode=None, io={})
add("_merge_phrase_paths", catalog=CAT, paths="cat.yaml", base_dir="/home", inherited_mode=None,
    io={"Path.expanduser": "cat.yaml", "Path.resolve": ["/home", "/home/cat.yaml"],
        "Path.is_file": True, "Path.read_text": "status:\n- x\n", "yaml.safe_load": {"status": ["x"]}})
add("_merge_phrase_paths", catalog=CAT, paths=["cat.yaml", "dog.yaml"], base_dir="/home",
    inherited_mode=None,
    io={"Path.expanduser": ["cat.yaml", "dog.yaml"],
        "Path.resolve": ["/home", "/home/cat.yaml", "/home", "/home/dog.yaml"],
        "Path.is_file": [True, True],
        "Path.read_text": ["status:\n- x\n", "generic:\n- y\n"],
        "yaml.safe_load": [{"status": ["x"]}, {"generic": ["y"]}]})
add("_merge_phrase_paths", catalog=CAT, paths=["/abs"], base_dir="/home", inherited_mode=None,
    io={"Path.expanduser": "/abs"})
add("_merge_phrase_paths", catalog=CAT, paths="cats", base_dir="/home", inherited_mode="replace",
    io={"Path.expanduser": "cats", "Path.resolve": ["/home", "/home/cats"],
        "Path.is_file": [False, True], "Path.is_dir": True, "Path.iterdir": sc(["/home/cats/a.yml"]),
        "Path.read_text": "status:\n- x\n", "yaml.safe_load": {"status": ["x"]}})
add("_merge_phrase_paths", catalog=CAT, paths="cat.yaml", base_dir="/home", inherited_mode=None,
    io={"Path.expanduser": "cat.yaml", "Path.resolve": ["/home", "/home/cat.yaml"],
        "Path.is_file": True, "Path.read_text": sc(raise_spec=rz("SystemExit", "stop"))})

# ── _merge_phrase_config ────────────────────────────────────────────────────
add("_merge_phrase_config", catalog=CAT, section=None, base_dir="/home", io={})
add("_merge_phrase_config", catalog=CAT, section={"status": ["x"]}, base_dir="/home", io={})
add("_merge_phrase_config", catalog=CAT, section={"mode": "replace", "generic": ["g"]},
    base_dir="/home", io={})
add("_merge_phrase_config", catalog=CAT, section={"path": "cat.yaml", "status": ["x"]},
    base_dir="/home",
    io={"Path.expanduser": "cat.yaml", "Path.resolve": ["/home", "/home/cat.yaml"],
        "Path.is_file": True, "Path.read_text": "status:\n- f\n", "yaml.safe_load": {"status": ["f"]}})
add("_merge_phrase_config", catalog=CAT, section={"paths": ["a.yaml"], "mode": "append"},
    base_dir="/home",
    io={"Path.expanduser": "a.yaml", "Path.resolve": ["/home", "/home/a.yaml"],
        "Path.is_file": True, "Path.read_text": "generic:\n- g\n", "yaml.safe_load": {"generic": ["g"]}})
add("_merge_phrase_config", catalog=CAT, section="notmap", base_dir="/home", io={})

# ── resolve_status_phrase_catalog ───────────────────────────────────────────
DEF = {"status": ["s"], "generic": ["g"]}
add("resolve_status_phrase_catalog", user_config=None, platform_key=None, default_phrases=DEF,
    io={"get_hermes_home": "/home", "Path.expanduser": ["/abs1", "/abs2"]})
add("resolve_status_phrase_catalog",
    user_config={"display": {"status_phrases": {"status": ["x"]}}}, platform_key=None,
    default_phrases=DEF, io={"get_hermes_home": "/home", "Path.expanduser": ["/abs1", "/abs2"]})
add("resolve_status_phrase_catalog",
    user_config={"display": {"platforms": {"telegram": {"status_phrases": {"generic": ["tg"]}}}}},
    platform_key="telegram", default_phrases=DEF,
    io={"get_hermes_home": "/home", "Path.expanduser": ["/abs1", "/abs2"]})
add("resolve_status_phrase_catalog",
    user_config={"display": {"platforms": {"telegram": {"status_phrases": {"generic": ["tg"]}}}}},
    platform_key=None, default_phrases=DEF,
    io={"get_hermes_home": "/home", "Path.expanduser": ["/abs1", "/abs2"]})
add("resolve_status_phrase_catalog", user_config={"display": "notmap"}, platform_key=None,
    default_phrases=DEF, io={"get_hermes_home": "/home", "Path.expanduser": ["/abs1", "/abs2"]})
add("resolve_status_phrase_catalog",
    user_config={"display": {"generic_status_phrases": {"status": ["legacy"]}}}, platform_key=None,
    default_phrases=DEF, io={"get_hermes_home": "/home", "Path.expanduser": ["/abs1", "/abs2"]})
add("resolve_status_phrase_catalog", user_config=None, platform_key=None, default_phrases=DEF,
    io={"get_hermes_home": sc(raise_spec=rz("SystemExit", "nohome"))})
add("resolve_status_phrase_catalog",
    user_config={"display": {"status_phrases": {"mode": "replace", "generic": ["rg"]}}},
    platform_key="telegram", default_phrases=DEF,
    io={"get_hermes_home": "/home", "Path.expanduser": ["/abs1", "/abs2"]})

# ── choose_status_phrase ────────────────────────────────────────────────────
add("choose_status_phrase", kind="status", catalog={"status": ["a", "b"], "generic": ["g"]},
    recent=None, io={"random.choice": "a"})
add("choose_status_phrase", kind="status", catalog={"status": ["a", "b"], "generic": ["g"]},
    recent=["a"], io={"random.choice": "b"})
add("choose_status_phrase", kind="tool", catalog={"status": ["a"], "generic": ["g1", "g2"]},
    recent=[], io={"random.choice": "g1"})
add("choose_status_phrase", kind="status", catalog={"status": ["a", "h"], "generic": ["g"]},
    recent=["a", "b", "c", "d", "e", "f", "g"], io={"random.choice": "h"})
add("choose_status_phrase", kind="status", catalog={}, default_phrases=DEF, recent=None,
    io={"random.choice": "s"})
add("choose_status_phrase", kind="status", catalog={"status": ["a"], "generic": ["g"]},
    recent=["a"], io={"random.choice": "a"})
add("choose_status_phrase", kind="status", catalog={"status": ["a", "b"], "generic": ["g"]},
    recent=None, rng=True, io={"random.choice": "b"})
add("choose_status_phrase", kind="tool", catalog={}, default_phrases={"status": ["s"]},
    recent=None, io={})  # KeyError 'generic'
add("choose_status_phrase", kind="status", catalog={"status": ["a", "b"], "generic": ["g"]},
    recent=["a", "b"], io={"random.choice": "a"})
add("choose_status_phrase", kind="status", catalog={"status": ["a"], "generic": ["g"]},
    recent=None, io={"random.choice": sc(raise_spec=rz("SystemExit", "stop"))})

# ── _load_default_phrases (module-level construction) ───────────────────────
MF = "/quarry/gateway/status_phrases.py"
add("_load_default_phrases", module_file=MF,
    io={"Path.resolve": MF, "Path.read_text": "status:\n- s1\ngeneric:\n- g1\n",
        "yaml.safe_load": {"status": ["s1"], "generic": ["g1"]}})
add("_load_default_phrases", module_file=MF,
    io={"Path.resolve": MF, "Path.read_text": "text",
        "yaml.safe_load": sc(raise_spec=rz("ValueError", "bad"))})
add("_load_default_phrases", module_file=MF,
    io={"Path.resolve": MF, "Path.read_text": sc(raise_spec=rz("KeyboardInterrupt", "ki"))})
add("_load_default_phrases", module_file=MF,
    io={"Path.resolve": MF, "Path.read_text": "- x\n", "yaml.safe_load": sc(["x"])})
add("_load_default_phrases", module_file=MF,
    io={"Path.resolve": sc(raise_spec=rz("OSError", "noresolve"))})
add("_load_default_phrases", module_file=MF, io={})  # fail-closed resolve -> propagates


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for f, n in sorted(counts.items()):
        print(f"  {f}: {n}")
    assert all(n >= 5 for n in counts.values()), "every port needs >=5 cases"


if __name__ == "__main__":
    main()