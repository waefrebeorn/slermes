#!/usr/bin/env python3
"""Generate tests/fixtures/skill_usage_flow.jsonl — differential cases for the
unclaimed tools/skill_usage.py FLOW ports (src/cli/skill_usage_flow.c).

Every case is {"func", "in"}; "in.seams" scripts the universal seam (single
value reused, array consumed per call with the last element repeating,
{"__raise__": kind, "msg": ...} scripts an exception, MISSING script is
fail-closed RuntimeError on both sides). logger.debug defaults to null (inert).

Domain (see the port header): ASCII inputs; paths are POSIX strings; int()-parse
strings use ASCII whitespace; json.loads scripts supply the parsed value (no
integral floats in non-dict top-level positions); op scripts are 2-element
[result, dirty] arrays; open returns a string fd tag; stat returns {st_size};
logged raises restricted to kinds whose str(exc)==msg.

Run:  python tests/gen_skill_usage_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "skill_usage_flow.jsonl")

SKILLS = "/h/skills"
USAGE = SKILLS + "/.usage.json"
LOCK = USAGE + ".lock"
ARCH = SKILLS + "/.archive"

EMPTY_REC = {
    "created_by": None, "use_count": 0, "view_count": 0, "last_used_at": None,
    "last_viewed_at": None, "patch_count": 0, "patch_generation": 0,
    "last_reused_patch_generation": 0, "last_patched_at": None,
    "created_at": "2026-01-01T00:00:00+00:00", "state": "active", "pinned": False,
    "archived_at": None, "first_seen_at": None,
}


def case(func, seams=None, **inp):
    s = {"logger.debug": None}
    s.update(seams or {})
    d = dict(inp)
    d["seams"] = s
    return {"func": func, "in": d}


def bare(func, seams=None, **inp):
    """Case WITHOUT the default logger script (fail-closed probes)."""
    d = dict(inp)
    d["seams"] = dict(seams or {})
    return {"func": func, "in": d}


def raise_(kind, msg="boom"):
    return {"__raise__": kind, "msg": msg}


CASES = []

# ── _int_or_zero (pure) ──
CASES += [
    case("int_or_zero", value=5),
    case("int_or_zero", value="12"),
    case("int_or_zero", value=" 12 "),
    case("int_or_zero", value=3.7),
    case("int_or_zero", value=-3.7),
    case("int_or_zero", value=True),
    case("int_or_zero", value=False),
    case("int_or_zero", value=None),
    case("int_or_zero", value=""),
    case("int_or_zero", value="abc"),
    case("int_or_zero", value="12.5"),
    case("int_or_zero", value=[1]),
    case("int_or_zero", value=[]),
    case("int_or_zero", value={"a": 1}),
    case("int_or_zero", value="1_2"),
    case("int_or_zero", value="1__2"),
    case("int_or_zero", value="\t12\n"),
    case("int_or_zero", value="0"),
]

# ── _read_lines ──
RL = {"path.exists": True, "path.read_text": "a\nb\n"}
CASES += [
    case("read_lines", RL, path="/h/f", fail_log="fail %s"),
    case("read_lines", {"path.exists": False}, path="/h/f", fail_log="fail %s"),
    case("read_lines", {"path.exists": True, "path.read_text": "  a  \n\n b \n"}, path="/h/f"),
    case("read_lines", {"path.exists": True, "path.read_text": ""}, path="/h/f"),
    case("read_lines", {"path.exists": True, "path.read_text": "a\r\nb\rc"}, path="/h/f"),
    case("read_lines", {"path.exists": True, "path.read_text": "x\x1cy"}, path="/h/f"),
    case("read_lines", {"path.exists": True, "path.read_text": raise_("OSError", "disk")},
         path="/h/f", fail_log="fail %s"),
    case("read_lines", {"path.exists": True, "path.read_text": raise_("ValueError")}, path="/h/f"),
    case("read_lines", {"path.exists": True, "path.read_text": raise_("EnvironmentError", "alias")},
         path="/h/f", fail_log="fail %s"),
    case("read_lines", {"path.exists": raise_("OSError")}, path="/h/f"),
    bare("read_lines", path="/h/f"),
]

# ── _backfilled ──
CASES += [
    case("backfilled", {"_empty_record": EMPTY_REC}, rec=None),
    case("backfilled", {"_empty_record": EMPTY_REC}, rec="x"),
    case("backfilled", {"_empty_record": EMPTY_REC}, rec={"use_count": 5}),
    case("backfilled", {"_empty_record": EMPTY_REC}, rec={}),
    case("backfilled", {"_empty_record": EMPTY_REC}, rec=dict(EMPTY_REC, use_count=9)),
    case("backfilled", {"_empty_record": raise_("RuntimeError")}, rec=None),
    bare("backfilled", rec=None),
]

# ── _report_row ──
RR = {"_empty_record": EMPTY_REC, "latest_activity_at": "2026-02-01T00:00:00+00:00",
      "activity_count": 3}
CASES += [
    case("report_row", RR, name="s", raw=None, extra={}),
    case("report_row", RR, name="s", raw={"use_count": 3}, extra={"has_record": True}),
    case("report_row", RR, name="s", raw=None, extra={"provenance": "agent", "_persisted": False}),
    case("report_row", dict(RR, latest_activity_at=None), name="s", raw={}),
    case("report_row", dict(RR, latest_activity_at=raise_("ValueError")), name="s", raw=None),
    case("report_row", dict(RR, activity_count=raise_("RuntimeError")), name="s", raw=None),
    case("report_row", dict(RR, _empty_record=raise_("OSError")), name="s", raw=None),
    bare("report_row", name="s", raw=None),
]

# ── _load_usage_strict ──
def lus_seams(parsed, exists=True, text="{}"):
    jl = [parsed] if isinstance(parsed, list) else parsed
    return {"_usage_file": USAGE, "path.exists": exists,
            "path.read_text": text, "json.loads": jl}


CASES += [
    case("load_usage_strict", lus_seams({}, exists=False)),
    case("load_usage_strict", lus_seams({})),
    case("load_usage_strict", lus_seams({"a": {"x": 1}, "b": 5})),
    case("load_usage_strict", lus_seams({"a": {"x": 1}, "b": None, "c": {"y": 2}})),
    case("load_usage_strict", lus_seams([1, 2])),
    case("load_usage_strict", lus_seams("x")),
    case("load_usage_strict", lus_seams(5)),
    case("load_usage_strict", lus_seams(None)),
    case("load_usage_strict", lus_seams(5.5)),
    case("load_usage_strict", lus_seams(raise_("ValueError", "Expecting value"))),
    case("load_usage_strict", {"_usage_file": USAGE, "path.exists": True,
                               "path.read_text": raise_("OSError")}),
    bare("load_usage_strict"),
]

# ── is_locked ──
CASES += [
    case("is_locked", lus_seams({"s": {"locked": True}}), skill_name="s"),
    case("is_locked", lus_seams({"s": {"locked": False}}), skill_name="s"),
    case("is_locked", lus_seams({"s": {}}), skill_name="s"),
    case("is_locked", lus_seams({}), skill_name="s"),
    case("is_locked", lus_seams({"s": {"locked": 1}}), skill_name="s"),
    case("is_locked", lus_seams([1]), skill_name="s"),
    bare("is_locked", skill_name="s"),
]

# ── locked_skill_names ──
CASES += [
    case("locked_skill_names", {"load_usage": {"a": {"locked": True}, "b": {"locked": False},
                                               "c": {"locked": True}}}, strict=False),
    case("locked_skill_names", {"load_usage": {}}, strict=False),
    case("locked_skill_names", lus_seams({}, exists=False), strict=True),
    case("locked_skill_names", lus_seams({"a": {"locked": True}, "z": {"locked": True}}), strict=True),
    case("locked_skill_names", lus_seams([1]), strict=True),
    case("locked_skill_names", {"load_usage": raise_("OSError")}, strict=False),
    bare("locked_skill_names", strict=False),
]

# ── _match_skill_dir ──
CASES += [
    case("match_skill_dir", {"read_skill_name": "foo"},
         skill_mds=["/h/skills/foo/SKILL.md"], skill_name="foo"),
    case("match_skill_dir", {"read_skill_name": "bar"},
         skill_mds=["/h/skills/foo/SKILL.md"], skill_name="foo"),
    case("match_skill_dir", {"read_skill_name": ["bar", "foo"]},
         skill_mds=["/h/a/SKILL.md", "/h/b/SKILL.md"], skill_name="foo"),
    case("match_skill_dir", {}, skill_mds=[], skill_name="foo"),
    case("match_skill_dir", {"read_skill_name": raise_("OSError")},
         skill_mds=["/h/a/SKILL.md"], skill_name="foo"),
    bare("match_skill_dir", skill_mds=["/h/a/SKILL.md"], skill_name="foo"),
]

# ── is_external_only ──
CASES += [
    case("is_external_only", {"_find_skill_dir": None, "_find_external_skill_dir": "/ext/s"},
         skill_name="s"),
    case("is_external_only", {"_find_skill_dir": "/h/s"}, skill_name="s"),
    case("is_external_only", {"_find_skill_dir": None, "_find_external_skill_dir": None},
         skill_name="s"),
    case("is_external_only", {"_find_skill_dir": raise_("OSError")}, skill_name="s"),
    case("is_external_only", {"_find_skill_dir": None,
                              "_find_external_skill_dir": raise_("RuntimeError")}, skill_name="s"),
    bare("is_external_only", skill_name="s"),
]

# ── _external_skill_names ──
def esn_seams(dirs, exists=True, mds=None, excl=False, names="a"):
    return {"get_all_skills_dirs": [dirs], "path.exists": exists,
            "path.rglob": [mds if mds is not None else ["/ext1/a/SKILL.md"]],
            "is_excluded_skill_path": excl, "read_skill_name": names}


CASES += [
    case("external_skill_names", esn_seams([SKILLS, "/ext1"])),
    case("external_skill_names", esn_seams([SKILLS])),
    case("external_skill_names", esn_seams([SKILLS, "/ext1"], exists=False)),
    case("external_skill_names", esn_seams([SKILLS, "/ext1"], excl=True)),
    case("external_skill_names", esn_seams([SKILLS, "/ext1", "/ext2"],
         mds=["/ext1/b/SKILL.md", "/ext1/a/SKILL.md"], names=["b", "a"])),
    case("external_skill_names", esn_seams([SKILLS, "/ext1"], names=raise_("OSError"))),
    bare("external_skill_names"),
]

# ── _iter_skill_mds ──
def iter_seams(mds, excl=False, ext=False, name="a"):
    return {"path.rglob": [mds], "is_excluded_skill_path": excl,
            "is_external_skill_path": ext, "read_skill_name": name}


CASES += [
    case("iter_skill_mds", iter_seams(["/h/skills/a/SKILL.md"]), base=SKILLS, local_only=True),
    case("iter_skill_mds", iter_seams(["/h/skills/a/SKILL.md"], excl=True), base=SKILLS),
    case("iter_skill_mds", iter_seams(["/h/skills/a/SKILL.md"], ext=True), base=SKILLS, local_only=True),
    case("iter_skill_mds", iter_seams(["/h/skills/a/SKILL.md"], ext=True), base=SKILLS, local_only=False),
    case("iter_skill_mds", iter_seams([]), base=SKILLS),
    case("iter_skill_mds", iter_seams(["/h/skills/a/SKILL.md"], name=raise_("OSError")), base=SKILLS),
    bare("iter_skill_mds", base=SKILLS),
]

# ── _read_bundled_names ──
def rbn_seams(text="foo:hash\nbar:hash\n", exists=True, supp=None):
    return {"_skills_dir": SKILLS, "path.exists": exists, "path.read_text": text,
            "read_suppressed_names": [supp if supp is not None else []]}


CASES += [
    case("read_bundled_names", rbn_seams()),
    case("read_bundled_names", rbn_seams(text="foo:hash\n\n  :hash\n")),
    case("read_bundled_names", rbn_seams(exists=False, supp=["baz"])),
    case("read_bundled_names", rbn_seams(supp=["foo", "qux"])),
    case("read_bundled_names", rbn_seams(text=raise_("OSError"), supp=["baz"])),
    case("read_bundled_names", dict(rbn_seams(), read_suppressed_names=raise_("RuntimeError"))),
    bare("read_bundled_names"),
]

# ── _toggle_suppressed_name ──
def tog_seams(supp, write=None):
    return {"read_suppressed_names": [supp], "_skills_dir": SKILLS,
            "atomic_write_text": write}


CASES += [
    case("toggle_suppressed_name", tog_seams([]), skill_name="foo", add=True),
    case("toggle_suppressed_name", tog_seams(["foo"]), skill_name="foo", add=True),
    case("toggle_suppressed_name", tog_seams(["foo", "bar"]), skill_name="foo", add=False),
    case("toggle_suppressed_name", tog_seams([]), skill_name="foo", add=False),
    case("toggle_suppressed_name", tog_seams([]), skill_name="", add=True),
    case("toggle_suppressed_name", tog_seams([], write=raise_("OSError", "disk")),
         skill_name="foo", add=True),
    case("toggle_suppressed_name", tog_seams([], write=raise_("SystemExit", "1")),
         skill_name="foo", add=True),
    case("toggle_suppressed_name", {"read_suppressed_names": raise_("RuntimeError")},
         skill_name="foo", add=True),
    bare("toggle_suppressed_name", skill_name="foo", add=True),
]

# ── _scan_local_skills ──
def scan_seams(exists=True, hub=None, mds=None, prot=False, keep=True,
               bundled_text="", supp=None, usage=None):
    s = {"_skills_dir": SKILLS, "path.exists": exists,
         "_read_hub_installed_names": [hub if hub is not None else []],
         "path.read_text": bundled_text, "read_suppressed_names": [supp if supp is not None else []],
         "load_usage": usage if usage is not None else {},
         "path.rglob": [mds if mds is not None else ["/h/skills/a/SKILL.md"]],
         "is_excluded_skill_path": False, "is_external_skill_path": False,
         "read_skill_name": "a", "is_protected_builtin": prot, "keep": keep}
    return s


CASES += [
    case("scan_local_skills", scan_seams(exists=False)),
    case("scan_local_skills", scan_seams()),
    case("scan_local_skills", scan_seams(hub=["a"])),
    case("scan_local_skills", scan_seams(prot=True)),
    case("scan_local_skills", scan_seams(keep=False)),
    case("scan_local_skills", scan_seams(keep=raise_("RuntimeError"))),
    case("scan_local_skills", scan_seams(mds=["/h/skills/b/SKILL.md", "/h/skills/a/SKILL.md"],
                                         keep=True)),
    bare("scan_local_skills"),
]

# ── _bump ──
CASES += [
    case("bump", {"_non_negative_int": 2, "_now_iso": "T"},
         rec={"use_count": 2}, count_key="use_count", ts_key="last_used_at"),
    case("bump", {"_non_negative_int": 0, "_now_iso": "T"},
         rec={}, count_key="view_count", ts_key="last_viewed_at"),
    case("bump", {"_non_negative_int": raise_("RuntimeError"), "_now_iso": "T"},
         rec={"use_count": 2}, count_key="use_count", ts_key="last_used_at"),
    case("bump", {"_non_negative_int": 2, "_now_iso": raise_("OSError")},
         rec={"use_count": 2}, count_key="use_count", ts_key="last_used_at"),
    case("bump", {"_non_negative_int": 0, "_now_iso": "T"},
         rec=None, count_key="use_count", ts_key="last_used_at"),
    bare("bump", rec={}, count_key="use_count", ts_key="last_used_at"),
]

# ── _locked_update ──
def lu_seams(op, save=True, enter=None, exit_=None, guard=None, parsed=None):
    s = {"_usage_file_lock.enter": enter, "_usage_file_lock.exit": exit_,
         "_usage_file": USAGE, "path.exists": True, "path.read_text": "{}",
         "json.loads": parsed if parsed is not None else {},
         "op": [op], "save_usage": save}
    if guard is not None:
        s["guard"] = guard
    return s


CASES += [
    case("locked_update", lu_seams([None, False]), skill_name="s", fail_log="lu %s %s"),
    case("locked_update", lu_seams(["res", True]), skill_name="s", fail_log="lu %s %s"),
    case("locked_update", lu_seams(["res", True], save=False), skill_name="s", fail_log="lu %s %s"),
    case("locked_update", lu_seams([None, True]), skill_name="s", fail_log="lu %s %s"),
    case("locked_update", lu_seams(["r", False], guard=False), skill_name="s",
         fail_log="lu %s %s", has_guard=True),
    case("locked_update", lu_seams(["r", False], guard=True), skill_name="s",
         fail_log="lu %s %s", has_guard=True),
    case("locked_update", lu_seams(raise_("ValueError")), skill_name="s", fail_log="lu %s %s"),
    case("locked_update", lu_seams([1]), skill_name="s", fail_log="lu %s %s"),
    case("locked_update", lu_seams(None), skill_name="s", fail_log="lu %s %s"),
    case("locked_update", lu_seams([None, False], enter=raise_("OSError")),
         skill_name="s", fail_log="lu %s %s"),
    case("locked_update", lu_seams([None, False], exit_=raise_("RuntimeError")),
         skill_name="s", fail_log="lu %s %s"),
    case("locked_update", lu_seams([None, False], parsed=[1]), skill_name="s", fail_log="lu %s %s"),
    case("locked_update", lu_seams(raise_("SystemExit", "1")), skill_name="s", fail_log="lu %s %s"),
    bare("locked_update", skill_name="s", fail_log="lu %s %s"),
]

# ── _set_field ──
CASES += [
    case("set_field", {"_mutate": True}, skill_name="s", key="created_by", value="agent"),
    case("set_field", {"_mutate": False}, skill_name="s", key="pinned", value=True),
    case("set_field", {"_mutate": None}, skill_name="s", key="pinned", value=True),
    case("set_field", {"_mutate": {"x": 1}}, skill_name="s", key="pinned", value=True),
    case("set_field", {"_mutate": raise_("OSError")}, skill_name="s", key="pinned", value=True),
    bare("set_field", skill_name="s", key="pinned", value=True),
]

# ── _mutate_and_emit ──
CASES += [
    case("mutate_and_emit", {"_mutate": {"created_by": "agent"}, "_emit_skill_lifecycle": None},
         skill_name="s", action="loaded", mutator="bump_use.<locals>._apply",
         hook_kwargs={"task_id": "t1", "session_id": "s1"}),
    case("mutate_and_emit", {"_mutate": None, "_emit_skill_lifecycle": None},
         skill_name="s", action="loaded", mutator="m"),
    case("mutate_and_emit", {"_mutate": "x", "_emit_skill_lifecycle": None},
         skill_name="s", action="loaded", mutator="m"),
    case("mutate_and_emit", {"_mutate": raise_("OSError")},
         skill_name="s", action="loaded", mutator="m"),
    case("mutate_and_emit", {"_mutate": {"created_by": "agent"},
                             "_emit_skill_lifecycle": raise_("RuntimeError")},
         skill_name="s", action="loaded", mutator="m"),
    case("mutate_and_emit", {"_mutate": {"created_by": "agent"}, "_emit_skill_lifecycle": None},
         skill_name="s", action="patched", mutator="m", hook_kwargs={}),
    bare("mutate_and_emit", skill_name="s", action="loaded", mutator="m"),
]

# ── reanchor_clock ──
CASES += [
    case("reanchor_clock", {"_mutate": None}, skill_name="s"),
    case("reanchor_clock", {"_mutate": {"x": 1}}, skill_name="s"),
    case("reanchor_clock", {"_mutate": raise_("OSError")}, skill_name="s"),
    case("reanchor_clock", {"_mutate": raise_("SystemExit", "1")}, skill_name="s"),
    case("reanchor_clock", {"_mutate": None}, skill_name=""),
    bare("reanchor_clock", skill_name="s"),
]

# ── set_locked ──
CASES += [
    case("set_locked", {"_mutate": True}, skill_name="s", locked=True),
    case("set_locked", {"_mutate": False}, skill_name="s", locked=False),
    case("set_locked", {"_mutate": None}, skill_name="s", locked=True),
    case("set_locked", {"_mutate": raise_("OSError")}, skill_name="s", locked=True),
    case("set_locked", {"_mutate": True}, skill_name="", locked=True),
    bare("set_locked", skill_name="s", locked=True),
]

# ── _flock ──
CASES += [
    case("flock", {"fcntl.flock": None}, fd="fd1", lock=True, fcntl=True),
    case("flock", {"fcntl.flock": None}, fd="fd1", lock=False, fcntl=True),
    case("flock", {"fd.seek": None, "fd.fileno": 3, "msvcrt.locking": None},
         fd="fd1", lock=True, msvcrt=True),
    case("flock", {"fd.seek": None}, fd="fd1", lock=True),
    case("flock", {"fcntl.flock": raise_("OSError")}, fd="fd1", lock=True, fcntl=True),
    case("flock", {"fd.seek": raise_("OSError")}, fd="fd1", lock=True, msvcrt=True),
    bare("flock", fd="fd1", lock=True, fcntl=True),
]

# ── skill_file_lock ──
def sfl_seams(fcntl=True, msvcrt=False, exists=False, st_size=0, mkdir=None,
              open_="fd1", flock=None, body=None, write=None):
    s = {"path.mkdir": mkdir, "open": open_, "body": body}
    if fcntl:
        s["fcntl.flock"] = flock
    if msvcrt:
        s["path.exists"] = exists
        s["path.stat"] = {"st_size": st_size}
        s["path.write_text"] = write
        s["fd.seek"] = None
        s["fd.fileno"] = 3
        s["msvcrt.locking"] = flock
    return s


CASES += [
    case("skill_file_lock", sfl_seams(), lock_path=LOCK, fcntl=True, has_paths=False, held=[]),
    case("skill_file_lock", sfl_seams(), lock_path=LOCK, fcntl=True, has_paths=True, held=[LOCK]),
    case("skill_file_lock", sfl_seams(fcntl=False), lock_path=LOCK, fcntl=False, msvcrt=False),
    case("skill_file_lock", sfl_seams(fcntl=False, msvcrt=True, exists=False),
         lock_path=LOCK, fcntl=False, msvcrt=True),
    case("skill_file_lock", sfl_seams(fcntl=False, msvcrt=True, exists=True, st_size=0),
         lock_path=LOCK, fcntl=False, msvcrt=True),
    case("skill_file_lock", sfl_seams(fcntl=False, msvcrt=True, exists=True, st_size=5),
         lock_path=LOCK, fcntl=False, msvcrt=True),
    case("skill_file_lock", sfl_seams(body=raise_("ValueError")), lock_path=LOCK, fcntl=True),
    case("skill_file_lock", sfl_seams(flock=raise_("OSError")), lock_path=LOCK, fcntl=True),
    case("skill_file_lock", sfl_seams(mkdir=raise_("OSError")), lock_path=LOCK, fcntl=True),
    bare("skill_file_lock", lock_path=LOCK, fcntl=True),
]

# ── _relocate ──
def rel_seams(action="archive", capture=None, rename=None, move=None, utime=None,
              bundled=False, set_state=None, record=None, supp=None, toggle_write=None):
    s = {"skill_ledger.capture_before": [capture], "path.rename": rename,
         "shutil.move": move, "os.utime": utime, "is_bundled": bundled,
         "set_state": set_state, "skill_ledger.record_mutation": record,
         # _toggle_suppressed_name composed seams
         "read_suppressed_names": [supp if supp is not None else []],
         "_skills_dir": SKILLS, "atomic_write_text": toggle_write}
    return s


CASES += [
    case("relocate", rel_seams(capture=["before"]), src="/h/skills/s", dest=ARCH + "/s",
         skill_name="s", action="archive", capture_kwargs={"complete_package": True, "skill": "s"}),
    case("relocate", rel_seams(action="restore", capture=None), src=ARCH + "/s",
         dest="/h/skills/s", skill_name="s", action="restore", capture_kwargs={}),
    case("relocate", rel_seams(rename=raise_("OSError"), move=None), src="/h/skills/s",
         dest=ARCH + "/s", skill_name="s", action="archive"),
    case("relocate", rel_seams(rename=raise_("OSError"), move=raise_("ValueError", "xdev")),
         src="/h/skills/s", dest=ARCH + "/s", skill_name="s", action="archive"),
    case("relocate", rel_seams(rename=raise_("ValueError")), src="/h/skills/s",
         dest=ARCH + "/s", skill_name="s", action="archive"),
    case("relocate", rel_seams(rename=raise_("WindowsError", "win"), move=None), src="/h/skills/s",
         dest=ARCH + "/s", skill_name="s", action="archive"),
    case("relocate", rel_seams(capture=raise_("ValueError")), src="/h/skills/s",
         dest=ARCH + "/s", skill_name="s", action="archive"),
    case("relocate", rel_seams(bundled=True, supp=["s"]), src="/h/skills/s",
         dest=ARCH + "/s", skill_name="s", action="archive"),
    case("relocate", rel_seams(utime=raise_("OSError")), src="/h/skills/s",
         dest=ARCH + "/s", skill_name="s", action="archive"),
    case("relocate", rel_seams(capture=["b"], record=raise_("ValueError")), src="/h/skills/s",
         dest=ARCH + "/s", skill_name="s", action="archive"),
    case("relocate", rel_seams(capture=["b"], record=raise_("SystemExit", "1")), src="/h/skills/s",
         dest=ARCH + "/s", skill_name="s", action="archive"),
    bare("relocate", src="/h/skills/s", dest=ARCH + "/s", skill_name="s", action="archive"),
]


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    per = {}
    for c in CASES:
        per[c["func"]] = per.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(per):
        print("  %-24s %d" % (k, per[k]))
    low = [k for k, v in per.items() if v < 5]
    if low:
        print("WARNING: funcs with <5 cases: %s" % low)


if __name__ == "__main__":
    main()