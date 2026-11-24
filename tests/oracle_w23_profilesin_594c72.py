#!/usr/bin/env python3
"""Differential oracle for hermes_cli/profiles_initialization.py (gate
w23_profilesin_594c72).

Every entry point runs on the live module; the seams the module reaches for --
the pid, the filesystem it is handed, the keychain, the profiles module's
seeders, the default SOUL text and the config mode fixer -- are scripted:

  * a temp cwd, so the fixture's relative paths are the only paths in play;
  * ``hermes_cli.profiles`` / ``hermes_cli.default_soul`` /
    ``hermes_cli.config_backend`` are stub modules that RECORD what they are
    asked to do, so the port's action list can be compared call for call;
  * ``_PLACEHOLDER_ENV`` and ``NO_BUNDLED_SKILLS_MARKER`` are read out of the
    live quarry source with ``ast`` (constant assignments), so a rename upstream
    fails this gate instead of passing on a stale copy;
  * ``pi.os`` is a namespace supplying the pid and recording chmod.
"""
import ast
import importlib.util
import json
import os
import shutil
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "profiles_initialization.py"
GATE = "w23_profilesin_594c72"
DEFAULT_SOUL_SENTINEL = "<DEFAULT_SOUL_MD>"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("profiles_init_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["profiles_init_live"] = live
_spec.loader.exec_module(live)

# A package shell with a real search path: our stub submodules win, everything
# else (home_data_layout) is loaded from the quarry.
if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [str(QUARRY / "hermes_cli")]
    sys.modules["hermes_cli"] = pkg


def quarry_constants():
    """_PLACEHOLDER_ENV / NO_BUNDLED_SKILLS_MARKER as the quarry defines them."""
    tree = ast.parse((QUARRY / "hermes_cli" / "profiles.py").read_text(encoding="utf-8"))
    found = {}
    for node in tree.body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1 and \
                isinstance(node.targets[0], ast.Name):
            try:
                found[node.targets[0].id] = ast.literal_eval(node.value)
            except (ValueError, SyntaxError):
                continue
    return found["_PLACEHOLDER_ENV"], found["NO_BUNDLED_SKILLS_MARKER"]


PLACEHOLDER_ENV, MARKER_NAME = quarry_constants()

WANTED = {"staging", "discard", "prepare_import", "finish_layout"}

WORK = tempfile.mkdtemp(prefix="w23pi_")
os.chdir(WORK)


def install(fake_modules):
    saved = {}
    for name, module in fake_modules.items():
        saved[name] = sys.modules.get(name, "MISSING")
        sys.modules[name] = module
    return saved


def restore(saved):
    for name, previous in saved.items():
        if previous == "MISSING":
            sys.modules.pop(name, None)
        else:
            sys.modules[name] = previous


class RecorderOS:
    """pi.os with the pid injected and chmod observed."""

    def __init__(self, pid):
        self.name = "nt"
        self._pid = pid
        self.chmods = []
        self._real = os

    def getpid(self):
        return self._pid

    def chmod(self, path, mode):
        self.chmods.append((str(path), int(mode)))
        return self._real.chmod(path, mode)


def case_staging(req):
    target = Path(req.get("dir"))
    kind = req.get("leftover", "none")
    staging = target.parent / (".%s.staging-%d" % (target.name, req.get("pid", 0)))
    staging.parent.mkdir(parents=True, exist_ok=True)
    if staging.is_dir():
        shutil.rmtree(staging, ignore_errors=True)
    elif staging.exists():
        staging.unlink()
    existed = False
    if kind == "file":
        staging.write_text("leftover", encoding="utf-8")
        existed = True
    elif kind == "dir":
        staging.mkdir()
        (staging / "partial.txt").write_text("x", encoding="utf-8")
        existed = True

    saved_os = live.os
    live.os = RecorderOS(req.get("pid", 0))
    try:
        result = live.clone_staging_dir(target)
    finally:
        live.os = saved_os
    return {"path": str(result),
            "removed": bool(existed and not staging.exists())}


def case_discard(req):
    ref = req.get("ref")
    deleted = []
    module = types.ModuleType("agent.vault_keychain")
    module.delete_key = lambda value: deleted.append(value)
    saved = install({"agent.vault_keychain": module})
    try:
        live.discard_unpublished_vault_key(ref)
    finally:
        restore(saved)
    return {"deleted": bool(deleted), "ref": deleted[0] if deleted else None}


def case_prepare_import(req):
    target = Path(req.get("dir"))
    if target.is_dir():
        shutil.rmtree(target, ignore_errors=True)
    target.mkdir(parents=True)
    for child in req.get("children") or []:
        path = target / child["name"]
        if child.get("kind") == "dir":
            path.mkdir()
        else:
            path.write_text("x", encoding="utf-8")
    env_kind = req.get("env", "absent")
    if env_kind == "file":
        (target / ".env").write_text("A=1\n", encoding="utf-8")
    elif env_kind == "dir":
        (target / ".env").mkdir()
    if req.get("config") == "file":
        (target / "config.yaml").write_text("k: v\n", encoding="utf-8")

    before = sorted(entry.name for entry in target.iterdir())
    drops = []
    backend = types.ModuleType("hermes_cli.config_backend")
    backend.drop_other_access = lambda path: drops.append(str(path))
    saved = install({"hermes_cli.config_backend": backend})
    saved_os = live.os
    recorder = RecorderOS(0)
    live.os = recorder
    try:
        live.prepare_imported_tree(target)
    finally:
        live.os = saved_os
        restore(saved)
    after = sorted(entry.name for entry in target.iterdir())
    return {"kept": after,
            "removed": sorted(set(before) - set(after)),
            "config_drop": drops[0] if drops else None,
            "env_chmod": recorder.chmods[0][1] if recorder.chmods else None}


def case_finish_layout(req):
    target = Path(req.get("dir"))
    if target.is_dir():
        shutil.rmtree(target, ignore_errors=True)
    target.mkdir(parents=True)
    actions = []

    def seed_kind(path):
        name = Path(path).name
        if name == ".env":
            return "seed_env"
        if name == MARKER_NAME:
            return "seed_marker"
        if name == "SOUL.md":
            return "seed_soul"
        return "seed_other"

    def seed_file_if_missing(path, text, mode=None):
        action = {"kind": seed_kind(path), "path": str(path), "text": text}
        if mode is not None:
            action["mode"] = int(mode)
        actions.append(action)

    profiles = types.ModuleType("hermes_cli.profiles")
    profiles._PLACEHOLDER_ENV = PLACEHOLDER_ENV
    profiles.NO_BUNDLED_SKILLS_MARKER = MARKER_NAME
    profiles._seed_file_if_missing = seed_file_if_missing
    profiles._migrate_profile_config_if_outdated = \
        lambda path: actions.append({"kind": "migrate", "path": str(path)})

    def write_profile_meta(path, **kwargs):
        actions.append({"kind": "meta", "path": str(path),
                        "description": kwargs.get("description"),
                        "auto": kwargs.get("description_auto")})

    profiles.write_profile_meta = write_profile_meta

    soul_mod = types.ModuleType("hermes_cli.default_soul")
    soul_mod.DEFAULT_SOUL_MD = DEFAULT_SOUL_SENTINEL
    backend = types.ModuleType("hermes_cli.config_backend")
    backend.drop_other_access = lambda path: actions.append(
        {"kind": "drop_access", "path": str(path)})

    fakes = {"hermes_cli.profiles": profiles,
             "hermes_cli.default_soul": None if req.get("default_soul_error") else soul_mod,
             "hermes_cli.config_backend": backend}
    saved = install(fakes)
    try:
        live.finish_profile_layout(
            target,
            no_skills=bool(req.get("no_skills")),
            clone_all=bool(req.get("clone_all")),
            description=req.get("description"),
            soul=req.get("soul"),
        )
    finally:
        restore(saved)

    soul_path = target / "SOUL.md"
    if soul_path.exists():
        actions.insert(0, {"kind": "write_soul", "path": str(soul_path),
                           "text": soul_path.read_text(encoding="utf-8")})
    return {"actions": actions}


HANDLERS = {
    "staging": case_staging,
    "discard": case_discard,
    "prepare_import": case_prepare_import,
    "finish_layout": case_finish_layout,
}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = HANDLERS[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    shutil.rmtree(WORK, ignore_errors=True)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
