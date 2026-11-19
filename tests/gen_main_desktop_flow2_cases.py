#!/usr/bin/env python3
"""Generate tests/fixtures/main_desktop_flow2.jsonl for the main_desktop.py
UNCLAIMED-remainder FLOW port. >=5 cases per ported function, covering the
decision branches, error/rollback paths, fail-closed script gaps and
BaseException propagation. Scripts use the flow1 universal-seam semantics:
plain value = constant (reused); JSON array = per-call queue;
{"__script__": true, "ret": v, "raise": spec} = wrapped result/raise.
List-valued seam results are wrapped (a bare array would be read as a queue).

Run: python tests/gen_main_desktop_flow2_cases.py
"""
import hashlib
import json
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "main_desktop_flow2.jsonl")

CASES = []


def case(func, **inp):
    # path.resolve is an identity echo seam (fixture paths are canonical):
    # normalize any non-raise script value to the None marker.
    io = inp.get("io")
    if isinstance(io, dict) and "path.resolve" in io:
        v = io["path.resolve"]
        if not (isinstance(v, dict) and v.get("__script__")):
            io["path.resolve"] = None
    CASES.append({"func": func, "in": inp})


def script(ret=None, raise_=None):
    d = {"__script__": True}
    if raise_ is not None:
        d["raise"] = raise_
    else:
        d["ret"] = ret
    return d


def raise_spec(type_, message="", **kw):
    d = {"type": type_, "message": message}
    d.update(kw)
    return d


def cpe_spec(rc, cmd, output=None):
    e = subprocess.CalledProcessError(rc, cmd, output=output)
    return {"type": "CalledProcessError", "message": str(e),
            "returncode": rc, "cmd": cmd, "output": output}


DD = "/repo/apps/desktop"
REL = DD + "/release"
APP = REL + "/mac-arm64/Hermes.app"
EXE = APP + "/Contents/MacOS/Hermes"
STAGING = DD + "/.staging-123-456"
SAPP = STAGING + "/mac-arm64/Hermes.app"
SEXEC = SAPP + "/Contents/MacOS/Hermes"
INST = "/Applications/Hermes.app"
IEXEC = INST + "/Contents/MacOS/Hermes"
CS = "/usr/bin/codesign"
LIVE = REL + "/win-unpacked"
ASIDE = LIVE + ".previous"
PREV = LIVE + ".previous"
PEX = REL + "/linux-unpacked/hermes"
TMP = INST + ".hermes-update-new"
OLD = INST + ".hermes-update-old"
SIG_OUT = "Executable=/x\nIdentifier=com.hermes.app\nTeamIdentifier=ABC123\n"
RB = b"rebuilt"
RB_HEX = RB.hex()
RB_DIG = hashlib.sha256(RB).hexdigest()


def ok_cp(rc=0, stdout="", stderr=""):
    return {"returncode": rc, "stdout": stdout, "stderr": stderr}


# ── _packaged_resources_dir ───────────────────────────────────────────────
case("_packaged_resources_dir", desktop_dir=DD, platform="darwin",
     io={"_desktop_packaged_executable": EXE})
case("_packaged_resources_dir", desktop_dir=DD, platform="win32",
     io={"_desktop_packaged_executable": REL + "/win-unpacked/Hermes.exe"})
case("_packaged_resources_dir", desktop_dir=DD, platform="linux",
     io={"_desktop_packaged_executable": REL + "/linux-unpacked/hermes"})
case("_packaged_resources_dir", desktop_dir=DD, platform="darwin",
     io={"_desktop_packaged_executable": None})
case("_packaged_resources_dir", desktop_dir=DD, platform="darwin",
     io={"_desktop_packaged_executable": script(raise_=raise_spec("SystemExit", code=3))})
case("_packaged_resources_dir", desktop_dir=DD, platform="linux", io={})

# ── _recover_interrupted_swap ─────────────────────────────────────────────
case("_recover_interrupted_swap", live=LIVE, aside=ASIDE,
     io={"path.exists": False, "path.is_dir": True, "path.rename": None,
         "logger.warning": None})
case("_recover_interrupted_swap", live=LIVE, aside=ASIDE,
     io={"path.exists": False, "path.is_dir": True,
         "path.rename": script(raise_=raise_spec("OSError", "locked", errno=13)),
         "logger.warning": None})
case("_recover_interrupted_swap", live=LIVE, aside=ASIDE,
     io={"path.exists": True, "shutil.rmtree": None})
case("_recover_interrupted_swap", live=LIVE, aside=ASIDE,
     io={"path.exists": False, "path.is_dir": False, "shutil.rmtree": None})
case("_recover_interrupted_swap", live=LIVE, aside=ASIDE,
     io={"path.exists": False, "path.is_dir": True,
         "path.rename": script(raise_=raise_spec("ValueError", "bad"))})
case("_recover_interrupted_swap", live=LIVE, aside=ASIDE,
     io={"path.exists": False, "path.is_dir": True, "path.rename": None})  # no logger script

# ── _move_live_app_aside ──────────────────────────────────────────────────
case("_move_live_app_aside", live_root=LIVE, previous=PREV, held_by=[],
     consent=None, platform="win32",
     io={"_rename_riding_out_file_lock": None})
case("_move_live_app_aside", live_root=LIVE, previous=PREV, held_by=[],
     consent="yes", platform="win32",
     io={"_rename_riding_out_file_lock": [
         script(raise_=raise_spec("PermissionError", "locked", errno=13)), None],
         "release_after_refused_rename": script(ret={"holders": ["Hermes.exe"], "closed": True})})
case("_move_live_app_aside", live_root=LIVE, previous=PREV, held_by=["prev"],
     consent=None, platform="win32",
     io={"_rename_riding_out_file_lock": script(raise_=raise_spec("PermissionError", "locked", errno=13)),
         "release_after_refused_rename": script(ret={"holders": ["A.exe", "B.exe"], "closed": False})})
case("_move_live_app_aside", live_root=LIVE, previous=PREV, held_by=None,
     consent=None, platform="linux",
     io={"_rename_riding_out_file_lock": script(raise_=raise_spec("PermissionError", "locked", errno=13))})
case("_move_live_app_aside", live_root=LIVE, previous=PREV, held_by=[],
     consent=None, platform="win32",
     io={"_rename_riding_out_file_lock": [
         script(raise_=raise_spec("PermissionError", "locked", errno=13)),
         script(raise_=raise_spec("PermissionError", "still locked", errno=13))],
         "release_after_refused_rename": script(ret={"holders": [], "closed": True}),
         "find_release_cwd_holders": script(ret=["C.exe"])})
case("_move_live_app_aside", live_root=LIVE, previous=PREV, held_by=[],
     consent=None, platform="win32",
     io={"_rename_riding_out_file_lock": script(raise_=raise_spec("OSError", "exdev", errno=18))})

# ── _discard_desktop_staging ──────────────────────────────────────────────
case("_discard_desktop_staging", staging_dir=STAGING, io={"shutil.rmtree": None})
case("_discard_desktop_staging", staging_dir=STAGING,
     io={"shutil.rmtree": script(raise_=raise_spec("OSError", "busy", errno=13))})
case("_discard_desktop_staging", staging_dir=STAGING, io={})
case("_discard_desktop_staging", staging_dir=STAGING,
     io={"shutil.rmtree": script(raise_=raise_spec("KeyboardInterrupt"))})
case("_discard_desktop_staging", staging_dir="", io={"shutil.rmtree": None})

# ── _kernel32 ─────────────────────────────────────────────────────────────
case("_kernel32", io={"ctypes.WinDLL": {"double": "kernel32"}})
case("_kernel32", io={"ctypes.WinDLL": script(ret={"handle": 7})})
case("_kernel32", io={"ctypes.WinDLL": script(raise_=raise_spec("OSError", "no dll", errno=2))})
case("_kernel32", io={"ctypes.WinDLL": script(raise_=raise_spec("SystemExit", code=1))})
case("_kernel32", io={})

# ── _runs_from ────────────────────────────────────────────────────────────
case("_runs_from", proc={"id": "p1"}, release_dir=REL,
     io={"proc.exe": EXE, "path.resolve": EXE})
case("_runs_from", proc={"id": "p1"}, release_dir=REL,
     io={"proc.exe": "/usr/bin/python3", "path.resolve": "/usr/bin/python3"})
case("_runs_from", proc={"id": "p1"}, release_dir=REL, io={"proc.exe": None})
case("_runs_from", proc={"id": "p1"}, release_dir=REL,
     io={"proc.exe": script(raise_=raise_spec("RuntimeError", "access denied"))})
case("_runs_from", proc={"id": "p1"}, release_dir=REL,
     io={"proc.exe": EXE, "path.resolve": script(raise_=raise_spec("OSError", "gone", errno=2))})
case("_runs_from", proc={"id": "p1"}, release_dir=REL,
     io={"proc.exe": script(raise_=raise_spec("SystemExit", code=2))})
case("_runs_from", proc={"id": "p1"}, release_dir=REL, io={})

# ── _desktop_ancestor_in ──────────────────────────────────────────────────
# path.resolve queue: [release_dir, ancestor1.exe, ancestor2.exe, ...]
case("_desktop_ancestor_in", desktop_dir=DD,
     io={"path.resolve": [REL, EXE, "/usr/bin/bash"], "os.getpid": 100,
         "psutil.Process": script(ret={"id": "me"}),
         "proc.parents": script(ret=[{"id": "a1", "pid": 50}, {"id": "a2", "pid": 10}]),
         "proc.exe": [EXE, "/usr/bin/bash"]})
case("_desktop_ancestor_in", desktop_dir=DD,
     io={"path.resolve": [REL, "/usr/bin/bash"], "os.getpid": 100,
         "psutil.Process": script(ret={"id": "me"}),
         "proc.parents": script(ret=[{"id": "a1", "pid": 50}]),
         "proc.exe": "/usr/bin/bash"})
case("_desktop_ancestor_in", desktop_dir=DD,
     io={"path.resolve": REL, "os.getpid": 100,
         "psutil.Process": script(ret={"id": "me"}),
         "proc.parents": script(ret=[])})
case("_desktop_ancestor_in", desktop_dir=DD,
     io={"path.resolve": script(raise_=raise_spec("OSError", "nope", errno=2))})
case("_desktop_ancestor_in", desktop_dir=DD,
     io={"path.resolve": REL, "os.getpid": 100,
         "psutil.Process": script(raise_=raise_spec("RuntimeError", "no proc"))})
case("_desktop_ancestor_in", desktop_dir=DD,
     io={"path.resolve": [REL, EXE], "os.getpid": 100,
         "psutil.Process": script(ret={"id": "me"}),
         "proc.parents": script(ret=[{"id": "a1", "pid": 50}]),
         "proc.exe": script(raise_=raise_spec("KeyboardInterrupt"))})

# ── _sandbox_helper_lstat ─────────────────────────────────────────────────
case("_sandbox_helper_lstat", packaged_executable=PEX,
     io={"path.lstat": {"st_mode": 0o104755, "st_uid": 0}})
case("_sandbox_helper_lstat", packaged_executable=PEX,
     io={"path.lstat": script(raise_=raise_spec("OSError", "missing", errno=2))})
case("_sandbox_helper_lstat", packaged_executable=PEX,
     io={"path.lstat": {"st_mode": 0o120777, "st_uid": 1000}})
case("_sandbox_helper_lstat", packaged_executable=PEX,
     io={"path.lstat": script(raise_=raise_spec("ValueError", "bad"))})
case("_sandbox_helper_lstat", packaged_executable=PEX, io={})

# ── _stage_macos_bundle_copy ──────────────────────────────────────────────
case("_stage_macos_bundle_copy", src=APP, dst=INST + ".new",
     io={"subprocess.run": ok_cp()})
case("_stage_macos_bundle_copy", src=APP, dst=INST + ".new",
     io={"subprocess.run": ok_cp(rc=1, stderr="ditto fail")})
case("_stage_macos_bundle_copy", src=APP, dst=INST + ".new",
     io={"subprocess.run": script(raise_=raise_spec("OSError", "no ditto", errno=2))})
case("_stage_macos_bundle_copy", src=APP, dst=INST + ".new",
     io={"subprocess.run": script(raise_=raise_spec("SystemExit", code=1))})
case("_stage_macos_bundle_copy", src=APP, dst=INST + ".new", io={})

# ── _launch_installed_macos_desktop_app ───────────────────────────────────
case("_launch_installed_macos_desktop_app", platform="darwin",
     io={"path.is_file": True, "launch_detached": 4242, "print": None})
case("_launch_installed_macos_desktop_app", platform="darwin",
     io={"path.is_file": False})
case("_launch_installed_macos_desktop_app", platform="linux", io={})
case("_launch_installed_macos_desktop_app", platform="darwin",
     io={"path.is_file": True, "launch_detached": script(raise_=raise_spec("OSError", "spawn", errno=2))})
case("_launch_installed_macos_desktop_app", platform="darwin",
     io={"path.is_file": True, "launch_detached": 7})  # no print script
case("_launch_installed_macos_desktop_app", platform="win32", io={})

# ── _desktop_macos_bundle_id ──────────────────────────────────────────────
FW = APP + "/Contents/Frameworks/Electron Framework.framework"
case("_desktop_macos_bundle_id", bundle=APP,
     io={"path.exists": True, "path.read_bytes": "00",
         "plistlib.loads": script(ret={"CFBundleIdentifier": "com.hermes.app"})})
case("_desktop_macos_bundle_id", bundle=APP, io={"path.exists": False})
case("_desktop_macos_bundle_id", bundle=FW,
     io={"path.exists": [False, True],
         "path.glob": [script(ret=[FW + "/Versions/A/Resources/Info.plist"]), script(ret=[])],
         "path.read_bytes": "00",
         "plistlib.loads": script(ret={"CFBundleIdentifier": "com.github.electron"})})
case("_desktop_macos_bundle_id", bundle=APP,
     io={"path.exists": True, "path.read_bytes": "00",
         "plistlib.loads": script(ret={"CFBundleIdentifier": ""})})
case("_desktop_macos_bundle_id", bundle=APP,
     io={"path.exists": True, "path.read_bytes": "00",
         "plistlib.loads": script(ret={"CFBundleIdentifier": 42})})
case("_desktop_macos_bundle_id", bundle=APP,
     io={"path.exists": True, "path.read_bytes": "00",
         "plistlib.loads": script(raise_=raise_spec("ValueError", "bad plist"))})
case("_desktop_macos_bundle_id", bundle=APP,
     io={"path.exists": True, "path.read_bytes": "00",
         "plistlib.loads": script(raise_=raise_spec("KeyboardInterrupt"))})

# ── _desktop_macos_local_signing_identity ─────────────────────────────────
case("_desktop_macos_local_signing_identity", platform="darwin",
     io={"load_config": {"desktop": {"macos_signing_identity": "Developer ID Application: X"}}})
case("_desktop_macos_local_signing_identity", platform="darwin",
     io={"load_config": {"desktop": {"macos_signing_identity": "  "}}})
case("_desktop_macos_local_signing_identity", platform="darwin",
     io={"load_config": {"desktop": "not-a-dict"}})
case("_desktop_macos_local_signing_identity", platform="darwin",
     io={"load_config": {"desktop": {"macos_signing_identity": 5}}})
case("_desktop_macos_local_signing_identity", platform="linux", io={})
case("_desktop_macos_local_signing_identity", platform="darwin",
     io={"load_config": script(raise_=raise_spec("RuntimeError", "config boom")), "print": None})
case("_desktop_macos_local_signing_identity", platform="darwin",
     io={"load_config": None, "print": None})
case("_desktop_macos_local_signing_identity", platform="darwin",
     io={"load_config": script(raise_=raise_spec("SystemExit", code=1))})

# ── _codesign_verify ──────────────────────────────────────────────────────
case("_codesign_verify", codesign=CS, app=APP, kwargs={"check": False},
     io={"subprocess.run": ok_cp()})
case("_codesign_verify", codesign=CS, app=APP, kwargs={"check": True},
     io={"subprocess.run": ok_cp(rc=1)})
case("_codesign_verify", codesign=CS, app=APP, kwargs={},
     io={"subprocess.run": ok_cp(rc=0, stdout="ok")})
case("_codesign_verify", codesign=CS, app=APP, kwargs={"check": False},
     io={"subprocess.run": script(raise_=raise_spec("OSError", "no codesign", errno=2))})
case("_codesign_verify", codesign=CS, app=APP, kwargs={"check": False}, io={})

# ── _macos_signature_summary ──────────────────────────────────────────────
case("_macos_signature_summary", codesign=CS, app=APP,
     io={"subprocess.run": [ok_cp(stdout=SIG_OUT), ok_cp()]})
case("_macos_signature_summary", codesign=CS, app=APP,
     io={"subprocess.run": [ok_cp(stdout="TeamIdentifier=not set\nIdentifier=com.x\n"), ok_cp(rc=1)]})
case("_macos_signature_summary", codesign=CS, app=APP,
     io={"subprocess.run": ok_cp(rc=1)})
case("_macos_signature_summary", codesign=CS, app=APP,
     io={"subprocess.run": script(raise_=raise_spec("OSError", "boom", errno=2))})
case("_macos_signature_summary", codesign=CS, app=APP,
     io={"subprocess.run": [ok_cp(stdout="Identifier=com.x\n"), ok_cp()]})
case("_macos_signature_summary", codesign=CS, app=APP,
     io={"subprocess.run": [ok_cp(stdout=SIG_OUT), script(raise_=raise_spec("SystemExit", code=1))]})

# ── _desktop_macos_has_valid_real_signature ───────────────────────────────
case("_desktop_macos_has_valid_real_signature", app=APP,
     io={"shutil.which": CS, "subprocess.run": [ok_cp(stdout=SIG_OUT), ok_cp()]})
case("_desktop_macos_has_valid_real_signature", app=APP, io={"shutil.which": None})
case("_desktop_macos_has_valid_real_signature", app=APP,
     io={"shutil.which": CS, "subprocess.run": ok_cp(stdout="TeamIdentifier=not set\n")})
case("_desktop_macos_has_valid_real_signature", app=APP,
     io={"shutil.which": CS, "subprocess.run": ok_cp(rc=1)})
case("_desktop_macos_has_valid_real_signature", app=APP,
     io={"shutil.which": CS, "subprocess.run": [ok_cp(stdout=SIG_OUT), ok_cp(rc=1)]})
case("_desktop_macos_has_valid_real_signature", app=APP,
     io={"shutil.which": CS, "subprocess.run": script(raise_=raise_spec("RuntimeError", "x"))})

# ── _macos_legacy_adhoc_resign ────────────────────────────────────────────
case("_macos_legacy_adhoc_resign", codesign=CS, app=APP,
     io={"subprocess.run": [ok_cp(), ok_cp()], "print": None})
case("_macos_legacy_adhoc_resign", codesign=CS, app=APP,
     io={"subprocess.run": ok_cp(rc=1), "print": None})
case("_macos_legacy_adhoc_resign", codesign=CS, app=APP,
     io={"subprocess.run": [ok_cp(), ok_cp(rc=1)], "print": None})
case("_macos_legacy_adhoc_resign", codesign=CS, app=APP,
     io={"subprocess.run": script(raise_=raise_spec("OSError", "boom", errno=2)), "print": None})
case("_macos_legacy_adhoc_resign", codesign=CS, app=APP,
     io={"subprocess.run": [ok_cp(), ok_cp()]})  # no print script
case("_macos_legacy_adhoc_resign", codesign=CS, app=APP,
     io={"subprocess.run": script(raise_=raise_spec("KeyboardInterrupt"))})

# ── _app_asar_hash ────────────────────────────────────────────────────────
DATA = b"hello asar world" * 5000
HEX = DATA.hex()
CH1, CH2 = HEX[:131072], HEX[131072:]
case("_app_asar_hash", app_path=APP,
     io={"path.is_file": True, "open": None, "file.read": [HEX, ""]})
case("_app_asar_hash", app_path=APP, io={"path.is_file": False})
case("_app_asar_hash", app_path=APP,
     io={"path.is_file": True, "open": script(raise_=raise_spec("OSError", "locked", errno=13))})
case("_app_asar_hash", app_path=APP,
     io={"path.is_file": True, "open": None,
         "file.read": script(raise_=raise_spec("OSError", "read fail", errno=5))})
case("_app_asar_hash", app_path=APP,
     io={"path.is_file": True, "open": None, "file.read": [""]})
case("_app_asar_hash", app_path=APP,
     io={"path.is_file": True, "open": None,
         "file.read": script(raise_=raise_spec("SystemExit", code=1))})
case("_app_asar_hash", app_path=APP,
     io={"path.is_file": True, "open": None, "file.read": [CH1, CH2, ""]})

# ── _swap_in_new_macos_bundle ─────────────────────────────────────────────
case("_swap_in_new_macos_bundle", tmp=TMP, target=INST, old=OLD,
     io={"path.exists": True, "path.rename": [None, None], "shutil.rmtree": None})
case("_swap_in_new_macos_bundle", tmp=TMP, target=INST, old=OLD,
     io={"path.exists": False, "path.rename": None, "shutil.rmtree": None})
case("_swap_in_new_macos_bundle", tmp=TMP, target=INST, old=OLD,
     io={"path.exists": True,
         "path.rename": script(raise_=raise_spec("OSError", "rename fail", errno=13)),
         "shutil.rmtree": None})
case("_swap_in_new_macos_bundle", tmp=TMP, target=INST, old=OLD,
     io={"path.exists": True,
         "path.rename": [None, script(raise_=raise_spec("OSError", "install fail", errno=5)),
                         script(raise_=raise_spec("OSError", "rollback fail", errno=13))],
         "shutil.rmtree": None})
case("_swap_in_new_macos_bundle", tmp=TMP, target=INST, old=OLD,
     io={"path.exists": True,
         "path.rename": [None, script(raise_=raise_spec("OSError", "install fail", errno=5)), None],
         "shutil.rmtree": None})
case("_swap_in_new_macos_bundle", tmp=TMP, target=INST, old=OLD,
     io={"path.exists": True,
         "path.rename": [None, script(raise_=raise_spec("ValueError", "bad"))]})

# ── _running_macos_app_bundles ────────────────────────────────────────────
case("_running_macos_app_bundles",
     io={"psutil.process_iter": script(ret=[{"exe": EXE}, {"exe": "/usr/bin/bash"}]),
         "path.resolve": EXE})
case("_running_macos_app_bundles",
     io={"psutil.process_iter": script(ret=[{"exe": EXE}, {"exe": EXE}]),
         "path.resolve": EXE})
case("_running_macos_app_bundles",
     io={"psutil.process_iter": script(ret=[{"exe": None}, {"exe": ""}])})
case("_running_macos_app_bundles",
     io={"psutil.process_iter": script(ret=[{"exe": "/a/Contents/MacOS/Hermes"}]),
         "path.resolve": "/a/Contents/MacOS/Hermes"})  # parents[2] OOR -> IndexError(2)
case("_running_macos_app_bundles",
     io={"psutil.process_iter": script(ret=[{"exe": EXE}]),
         "path.resolve": script(raise_=raise_spec("OSError", "gone", errno=2))})
case("_running_macos_app_bundles", io={})

# ── _update_owned_macos_bundles ───────────────────────────────────────────
case("_update_owned_macos_bundles", candidates=[APP, INST],
     io={"path.read_text": ['{"updateMechanism": "self"}', '{"updateMechanism": "bundled"}']})
case("_update_owned_macos_bundles", candidates=[APP], io={"path.read_text": '{}'})
case("_update_owned_macos_bundles", candidates=[APP, INST],
     io={"path.read_text": [script(raise_=raise_spec("OSError", "missing", errno=2)),
                            '{"updateMechanism": "self"}']})
case("_update_owned_macos_bundles", candidates=[APP], io={"path.read_text": ""})
case("_update_owned_macos_bundles", candidates=[APP], io={"path.read_text": '[1, 2]'})
case("_update_owned_macos_bundles", candidates=[APP],
     io={"path.read_text": "\ufeff" + '{"updateMechanism": "self"}'})
case("_update_owned_macos_bundles", candidates=[APP],
     io={"path.read_text": script(raise_=raise_spec("SystemExit", code=1))})

# ── _owns_installed_desktop_apps ──────────────────────────────────────────
case("_owns_installed_desktop_apps", platform="darwin", project_root="/repo/hermes-agent",
     io={"path.resolve": ["/repo/hermes-agent", "/repo/hermes-agent"],
         "get_default_hermes_root": "/repo"})
case("_owns_installed_desktop_apps", platform="darwin", project_root="/other/tree",
     io={"path.resolve": ["/other/tree", "/repo/hermes-agent"],
         "get_default_hermes_root": "/repo"})
case("_owns_installed_desktop_apps", platform="linux", io={})
case("_owns_installed_desktop_apps", platform="darwin", project_root="/repo/hermes-agent",
     io={"path.resolve": script(raise_=raise_spec("OSError", "x", errno=2))})
case("_owns_installed_desktop_apps", platform="darwin", project_root="/repo/hermes-agent",
     io={"path.resolve": "/repo/hermes-agent",
         "get_default_hermes_root": script(raise_=raise_spec("RuntimeError", "no home"))})

# ── _installed_desktop_apps ───────────────────────────────────────────────
OWN_IO = {"path.resolve": ["/repo/hermes-agent", "/repo/hermes-agent"],
          "get_default_hermes_root": "/repo"}
case("_installed_desktop_apps", platform="darwin", project_root="/repo/hermes-agent",
     io={**OWN_IO, "packaged_gui_app_paths": script(ret=[APP, INST]),
         "path.read_text": ['{"updateMechanism": "self"}', '{"updateMechanism": "self"}']})
case("_installed_desktop_apps", platform="darwin", project_root="/repo/hermes-agent",
     io={**OWN_IO, "packaged_gui_app_paths": script(ret=[APP, INST]),
         "path.read_text": ['{"updateMechanism": "bundled"}', '{"updateMechanism": "bundled"}'],
         "desktop_install_record": "/home/u/.hermes/desktop-install.json",
         "read_json_or_empty": {"apps": [INST]}, "path.exists": False})
case("_installed_desktop_apps", platform="darwin", project_root="/repo/hermes-agent",
     io={**OWN_IO, "packaged_gui_app_paths": script(ret=[APP, INST]),
         "path.read_text": ['{"updateMechanism": "bundled"}', '{"updateMechanism": "bundled"}'],
         "desktop_install_record": "/home/u/.hermes/desktop-install.json",
         "read_json_or_empty": {"apps": "not-a-list"}})
case("_installed_desktop_apps", platform="linux", io={})
case("_installed_desktop_apps", platform="darwin", project_root="/other",
     io={"path.resolve": ["/other", "/repo/hermes-agent"], "get_default_hermes_root": "/repo"})
case("_installed_desktop_apps", platform="darwin", project_root="/repo/hermes-agent",
     io={**OWN_IO, "packaged_gui_app_paths": script(ret=[INST]),
         "path.read_text": '{"updateMechanism": "bundled"}',
         "desktop_install_record": "/rec", "read_json_or_empty": {"apps": [INST]},
         "path.exists": True})

# ── _install_rebuilt_macos_bundles ────────────────────────────────────────
# Success: rebuilt hash present, installed is_file False (hash None != rebuilt).
case("_install_rebuilt_macos_bundles", rebuilt_app=APP, candidates=[INST], running=[],
     platform="darwin",
     io={"path.is_file": [True, False], "open": None, "file.read": [RB_HEX, ""],
         "shutil.which": CS, "subprocess.run": ok_cp(),
         "path.exists": True, "path.is_dir": True, "path.resolve": INST,
         "path.rename": None, "shutil.rmtree": None,
         "_macos_signing_downgrade_error": None})
case("_install_rebuilt_macos_bundles", rebuilt_app=APP, candidates=[], running=[],
     io={"path.is_file": True, "open": None, "file.read": [RB_HEX, ""],
         "shutil.which": None})
case("_install_rebuilt_macos_bundles", rebuilt_app=APP, candidates=[INST], running=[INST],
     platform="darwin",
     io={"path.is_file": [True, False], "open": None, "file.read": [RB_HEX, ""],
         "shutil.which": None, "path.exists": True, "path.is_dir": True,
         "path.resolve": INST, "shutil.rmtree": None})
case("_install_rebuilt_macos_bundles", rebuilt_app=APP, candidates=[INST], running=[],
     platform="darwin", io={"path.is_file": False})  # rebuilt hash None -> [], []
case("_install_rebuilt_macos_bundles", rebuilt_app=APP, candidates=[INST], running=[],
     platform="darwin",
     io={"path.is_file": True, "open": None, "file.read": [RB_HEX, "", RB_HEX, ""],
         "shutil.which": None, "path.exists": True, "path.is_dir": True,
         "path.resolve": INST, "shutil.rmtree": None})  # installed hash == rebuilt -> current -> skip
case("_install_rebuilt_macos_bundles", rebuilt_app=APP, candidates=[INST], running=[],
     platform="darwin",
     io={"path.is_file": [True, False], "open": None, "file.read": [RB_HEX, ""],
         "shutil.which": CS, "subprocess.run": ok_cp(),
         "path.exists": True, "path.is_dir": True, "path.resolve": INST,
         "shutil.rmtree": None,
         "_macos_signing_downgrade_error": "publisher Team ID mismatch",
         "path.rename": None})  # downgrade refuse
case("_install_rebuilt_macos_bundles", rebuilt_app=APP, candidates=[INST], running=[],
     platform="darwin",
     io={"path.is_file": [True, False], "open": None, "file.read": [RB_HEX, ""],
         "shutil.which": None, "path.exists": True, "path.is_dir": True,
         "path.resolve": INST, "shutil.rmtree": None,
         "subprocess.run": ok_cp(rc=1, stderr="ditto fail"),
         "path.rename": None})  # stage copy fails -> problem

# ── _install_rebuilt_desktop_app ──────────────────────────────────────────
IRB_IO = {"path.is_file": [True, False], "open": None, "file.read": [RB_HEX, ""],
          "shutil.which": CS, "subprocess.run": ok_cp(),
          "path.exists": True, "path.is_dir": True, "path.resolve": INST,
          "path.rename": None, "shutil.rmtree": None,
          "_macos_signing_downgrade_error": None}
case("_install_rebuilt_desktop_app", desktop_dir=DD, candidates=[INST], platform="darwin",
     io={"_desktop_packaged_executable": EXE,
         "psutil.process_iter": script(ret=[]), **IRB_IO})
case("_install_rebuilt_desktop_app", desktop_dir=DD, candidates=[INST], platform="linux", io={})
case("_install_rebuilt_desktop_app", desktop_dir=DD, candidates=[INST], platform="darwin",
     io={"_desktop_packaged_executable": None})
case("_install_rebuilt_desktop_app", desktop_dir=DD, candidates=[], platform="darwin",
     io={"_desktop_packaged_executable": EXE, "psutil.process_iter": script(ret=[]),
         "path.is_file": True, "open": None, "file.read": [RB_HEX, ""],
         "shutil.which": None})
case("_install_rebuilt_desktop_app", desktop_dir=DD, candidates=[INST], platform="darwin",
     io={"_desktop_packaged_executable": script(raise_=raise_spec("SystemExit", code=1))})

# ── _refresh_installed_desktop_apps ───────────────────────────────────────
REF_IO = {**OWN_IO, "packaged_gui_app_paths": script(ret=[INST]),
          "path.read_text": '{"updateMechanism": "self"}',
          "_desktop_packaged_executable": EXE,
          "psutil.process_iter": script(ret=[]),
          "path.is_file": [True, False], "open": None, "file.read": [RB_HEX, ""],
          "shutil.which": CS, "subprocess.run": ok_cp(),
          "path.is_dir": True, "path.rename": None, "shutil.rmtree": None,
          "path.resolve": INST, "_macos_signing_downgrade_error": None,
          "desktop_install_record": "/rec", "print": None, "logger.warning": None}
case("_refresh_installed_desktop_apps", desktop_dir=DD, platform="darwin",
     project_root="/repo/hermes-agent",
     io={**REF_IO, "path.exists": [False, False, True, True],
         "read_json_or_empty": {"apps": []}, "atomic_json_write": None})
case("_refresh_installed_desktop_apps", desktop_dir=DD, platform="linux", io={})
case("_refresh_installed_desktop_apps", desktop_dir=DD, platform="darwin",
     project_root="/other",
     io={"path.resolve": ["/other", "/repo/hermes-agent"], "get_default_hermes_root": "/repo"})
case("_refresh_installed_desktop_apps", desktop_dir=DD, platform="darwin",
     project_root="/repo/hermes-agent",
     io={**REF_IO, "path.exists": True, "read_json_or_empty": {"apps": [INST]}})
case("_refresh_installed_desktop_apps", desktop_dir=DD, platform="darwin",
     project_root="/repo/hermes-agent",
     io={**REF_IO, "path.exists": [False, False, True, True],
         "read_json_or_empty": {"apps": []},
         "atomic_json_write": script(raise_=raise_spec("OSError", "disk full", errno=28))})

# ── _installed_desktop_launch_target ──────────────────────────────────────
LT_BASE = {**REF_IO, "read_json_or_empty": {"apps": [INST]}}
case("_installed_desktop_launch_target", desktop_dir=DD, packaged_executable=EXE,
     platform="darwin", project_root="/repo/hermes-agent",
     io={**LT_BASE, "path.exists": True,
         "path.is_file": [True, False, True, True],
         "file.read": [RB_HEX, "", RB_HEX, "", RB_HEX, ""]})
case("_installed_desktop_launch_target", desktop_dir=DD, packaged_executable=EXE,
     platform="linux", io={})
case("_installed_desktop_launch_target", desktop_dir=DD, packaged_executable=EXE,
     platform="darwin", project_root="/repo/hermes-agent",
     io={**LT_BASE, "path.exists": True,
         "path.is_file": [True, False, True, True],
         "file.read": [RB_HEX, "", RB_HEX, "", b"other".hex(), ""]})  # mismatch -> fallback
case("_installed_desktop_launch_target", desktop_dir=DD, packaged_executable=EXE,
     platform="darwin", project_root="/repo/hermes-agent",
     io={**LT_BASE, "path.exists": True,
         "path.is_file": [True, False, False]})  # rebuilt hash None -> fallback
case("_installed_desktop_launch_target", desktop_dir=DD, packaged_executable=EXE,
     platform="darwin", project_root="/other",
     io={"path.resolve": ["/other", "/repo/hermes-agent"], "get_default_hermes_root": "/repo",
         "path.is_file": True, "open": None, "file.read": [RB_HEX, ""]})

# ── _promote_staged_desktop_app ───────────────────────────────────────────
case("_promote_staged_desktop_app", desktop_dir=DD, staging_dir=STAGING, platform="darwin",
     io={"_desktop_packaged_executable_in": SEXEC,
         "_desktop_macos_relaunchable_fixup": True,
         "_swap_staged_desktop_app": script(ret={"exe": EXE, "held_by": []}),
         "shutil.rmtree": None})
case("_promote_staged_desktop_app", desktop_dir=DD, staging_dir=STAGING, platform="darwin",
     io={"_desktop_packaged_executable_in": SEXEC,
         "_desktop_macos_relaunchable_fixup": False,
         "shutil.rmtree": None, "print": None})
case("_promote_staged_desktop_app", desktop_dir=DD, staging_dir=STAGING, platform="win32",
     io={"_desktop_packaged_executable_in": SEXEC,
         "_desktop_macos_relaunchable_fixup": True,
         "_desktop_exe_integrity_error": "architecture mismatch",
         "shutil.rmtree": None, "print": None})
case("_promote_staged_desktop_app", desktop_dir=DD, staging_dir=STAGING, platform="win32",
     integrity_check="custom",
     io={"_desktop_packaged_executable_in": SEXEC,
         "_desktop_macos_relaunchable_fixup": True,
         "integrity_check": "custom error", "shutil.rmtree": None, "print": None})
case("_promote_staged_desktop_app", desktop_dir=DD, staging_dir=STAGING, platform="darwin",
     io={"_desktop_packaged_executable_in": None,
         "_desktop_macos_relaunchable_fixup": True,
         "shutil.rmtree": None, "print": None})
case("_promote_staged_desktop_app", desktop_dir=DD, staging_dir=STAGING, platform="win32",
     io={"_desktop_packaged_executable_in": SEXEC,
         "_desktop_macos_relaunchable_fixup": True,
         "_desktop_exe_integrity_error": None,
         "_swap_staged_desktop_app": script(ret={"exe": None, "held_by": ["Hermes.exe"]}),
         "describe_block": "blocked by Hermes.exe", "holder_names": "Hermes.exe",
         "print": None, "shutil.rmtree": None})
case("_promote_staged_desktop_app", desktop_dir=DD, staging_dir=STAGING, platform="darwin",
     io={"_desktop_packaged_executable_in": SEXEC,
         "_desktop_macos_relaunchable_fixup": True,
         "_swap_staged_desktop_app": script(ret={"exe": None, "held_by": []}),
         "print": None, "shutil.rmtree": None})

# ── build_prepared_desktop ────────────────────────────────────────────────
BP_ENV = {"PATH": "/usr/bin"}
case("build_prepared_desktop", desktop_dir=DD, source_mode=True, pnpm="pnpm",
     env=BP_ENV, platform="linux",
     io={"run_in_custody": None, "_force_adhoc_macos_signing": False})
case("build_prepared_desktop", desktop_dir=DD, source_mode=False, pnpm="pnpm",
     env=BP_ENV, platform="darwin",
     io={"_force_adhoc_macos_signing": True, "print": None,
         "_desktop_staging_dir": STAGING,
         "_stop_desktop_processes_locking_build": script(ret=[11, 22]),
         "run_in_custody": [None, None], "publish_stage": None,
         "_desktop_packaged_executable_in": SEXEC,
         "_desktop_macos_relaunchable_fixup": True,
         "_swap_staged_desktop_app": script(ret={"exe": EXE, "held_by": []}),
         "shutil.rmtree": None})
case("build_prepared_desktop", desktop_dir=DD, source_mode=False, pnpm="pnpm",
     env=BP_ENV, platform="win32",
     io={"path.resolve": [REL, EXE], "os.getpid": 100,
         "psutil.Process": script(ret={"id": "me"}),
         "proc.parents": script(ret=[{"id": "a1", "pid": 50}]),
         "proc.exe": EXE, "print": None})
case("build_prepared_desktop", desktop_dir=DD, source_mode=False, pnpm="pnpm",
     env=BP_ENV, platform="win32",
     io={"path.resolve": REL, "os.getpid": 100,
         "psutil.Process": script(ret={"id": "me"}),
         "proc.parents": script(ret=[]),
         "pm.ensure": script(ret={"env": {"PATH": "/git;/usr/bin"}}),
         "_force_adhoc_macos_signing": False,
         "_desktop_staging_dir": STAGING,
         "_stop_desktop_processes_locking_build": script(ret=[]),
         "run_in_custody": script(raise_=cpe_spec(1, ["pnpm", "run", "build"],
                                                  output="@esbuild/ could not be found")),
         "_diagnose_esbuild_ignore_scripts": None, "print": None,
         "shutil.rmtree": None})
case("build_prepared_desktop", desktop_dir=DD, source_mode=True, pnpm="pnpm",
     env=BP_ENV, platform="linux", icons="/repo/icon.png",
     io={"run_in_custody": None, "_force_adhoc_macos_signing": False})
case("build_prepared_desktop", desktop_dir=DD, source_mode=False, pnpm="pnpm",
     env=BP_ENV, platform="darwin",
     io={"_force_adhoc_macos_signing": False,
         "_desktop_staging_dir": STAGING,
         "_stop_desktop_processes_locking_build": script(ret=[]),
         "run_in_custody": [None, None], "publish_stage": None,
         "_desktop_packaged_executable_in": SEXEC,
         "_desktop_macos_relaunchable_fixup": True,
         "_swap_staged_desktop_app": script(ret={"exe": EXE, "held_by": []}),
         "shutil.rmtree": script(raise_=raise_spec("RuntimeError", "discard boom"))})
case("build_prepared_desktop", desktop_dir=DD, source_mode=False, pnpm="pnpm",
     env=BP_ENV, platform="darwin",
     io={"_force_adhoc_macos_signing": False,
         "_desktop_staging_dir": STAGING,
         "_stop_desktop_processes_locking_build": script(ret=[]),
         "run_in_custody": script(raise_=raise_spec("SystemExit", code=1)),
         "shutil.rmtree": None})

# ── _desktop_macos_local_codesign ─────────────────────────────────────────
CONTENTS = APP + "/Contents"
FWDIR = CONTENTS + "/Frameworks"
FWB = FWDIR + "/Electron Framework.framework"
HELPER = FWDIR + "/Hermes Helper.app"
WALK1 = [
    {"root": CONTENTS + "/MacOS", "dirs": [],
     "files": ["app.node", "chrome_crashpad_handler", "Hermes"]},
    {"root": HELPER + "/Contents/MacOS", "dirs": [], "files": ["spawn-helper"]},
]
WALK2 = [{"root": FWB, "dirs": [], "files": []},
         {"root": HELPER, "dirs": [], "files": []}]
LC_IO = {"shutil.which": CS, "path.exists": True,
         "os.walk": [script(ret=WALK1), script(ret=WALK2)],
         "path.read_bytes": "00",
         "plistlib.loads": script(ret={"CFBundleIdentifier": "com.hermes.app"}),
         "subprocess.run": ok_cp()}
case("_desktop_macos_local_codesign", app=APP, desktop_dir=DD, identity="-", io=LC_IO)
case("_desktop_macos_local_codesign", app=APP, desktop_dir=DD, identity="Developer ID X", io=LC_IO)
case("_desktop_macos_local_codesign", app=APP, desktop_dir=DD, io={"shutil.which": None})
case("_desktop_macos_local_codesign", app=APP, desktop_dir=DD,
     io={"shutil.which": CS, "path.exists": [True, False]})
case("_desktop_macos_local_codesign", app=APP, desktop_dir=DD,
     io={"shutil.which": CS, "path.exists": [False]})
case("_desktop_macos_local_codesign", app=APP, desktop_dir=DD,
     io={**LC_IO, "subprocess.run": ok_cp(rc=1)})
case("_desktop_macos_local_codesign", app=APP, desktop_dir=DD,
     io={"shutil.which": CS,
         "path.exists": [True, True, script(raise_=raise_spec("SystemExit", code=1))]})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs -> {OUT}")
    if short:
        print("SHORT (<5):", short)
        raise SystemExit(1)
    print("per-func:", json.dumps(counts, sort_keys=True))


if __name__ == "__main__":
    main()