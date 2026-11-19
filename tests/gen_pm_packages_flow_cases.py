#!/usr/bin/env python3
"""Generate tests/fixtures/pm_packages_flow.jsonl for the pm/packages.py FLOW
port. Class-attribute defaults are AST-extracted from the LIVE quarry
(pm/packages.py + pm/package.py bases, pm/store.py target constants), merged
along each class's MRO; every case scripts the universal seam (in.io) with
the same queue semantics the C harness and the oracle share:

- a plain JSON value is returned verbatim on every call;
- a bare LIST is a per-call queue (one element consumed per call);
- {"__script__": true, "ret": v} / {"__script__": true, "raise": spec} force
  a single result / a real exception.

So a seam whose live return is a LIST is scripted one level deeper
([["a","b"]]), and a tuple-returning seam (try_install_libatomic) likewise
([[false,"remedy"]]). Fail-closed cases (missing script) carry
in.expect_fail_closed so the oracle self-test accepts the recorded
RuntimeError.
"""
import ast
import json
import os
import sys
import textwrap
from pathlib import Path

_REPO = Path(__file__).resolve().parent.parent
Q = Path(os.path.expanduser("~/hermes-agent"))
SRC = Q / "pm" / "packages.py"
PKG_SRC = Q / "pm" / "package.py"
STORE_SRC = Q / "pm" / "store.py"
OUT = _REPO / "tests" / "fixtures" / "pm_packages_flow.jsonl"


def seed_constants(src, ns):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec(compile(textwrap.dedent(ast.get_source_segment(src, node)), "<c>", "exec"), ns)
            except Exception:
                pass


def class_attrs(src, clsname, ns):
    out = {}
    for node in ast.parse(src).body:
        if isinstance(node, ast.ClassDef) and node.name == clsname:
            for sub in node.body:
                if isinstance(sub, (ast.Assign, ast.AnnAssign)):
                    tgts = sub.targets if isinstance(sub, ast.Assign) else [sub.target]
                    if not all(isinstance(t, ast.Name) for t in tgts):
                        continue
                    local = dict(ns)
                    try:
                        exec(compile(textwrap.dedent(ast.get_source_segment(src, sub)), "<a>", "exec"), local)
                    except Exception:
                        continue
                    for t in tgts:
                        out[t.id] = local[t.id]
    return out


def jsonable(v):
    if isinstance(v, (str, int, float, bool)) or v is None:
        return v
    if isinstance(v, (list, tuple)):
        return [jsonable(x) for x in v]
    if isinstance(v, dict):
        return {str(k): jsonable(x) for k, x in v.items()}
    return None  # frozensets etc. are never read by the ported methods


NS = {"os": os}
seed_constants(SRC.read_text(encoding="utf-8"), NS)
seed_constants(STORE_SRC.read_text(encoding="utf-8"), NS)
assert "ALL_TARGETS" in NS and "MUSL_TARGETS" in NS, "store constants failed to seed"
psrc = SRC.read_text(encoding="utf-8")
ksrc = PKG_SRC.read_text(encoding="utf-8")
ATTRS = {}
for cls in ("Package", "DebPackage"):
    ATTRS[cls] = class_attrs(ksrc, cls, NS)
for cls in ("BinaryPackage", "Dmgbuild", "_BionicDebArm", "Uv", "Python", "Nodejs",
            "TermuxDocker", "Npm", "Pnpm", "Git", "Gh", "Ffmpeg", "Ripgrep", "Just",
            "CuaDriver", "Chromium", "LlamaCpp", "LlamaCppCuda", "LlamaCppVulkan",
            "LlamaCppHip", "LlamaCppMetal", "LlamaCppCpu", "WhisperCppCpu"):
    ATTRS[cls] = class_attrs(psrc, cls, NS)

MRO = {
    "BinaryPackage": ["Package", "BinaryPackage"],
    "Dmgbuild": ["Package", "BinaryPackage", "Dmgbuild"],
    "_BionicDebArm": ["Package", "_BionicDebArm"],
    "Uv": ["Package", "BinaryPackage", "DebPackage", "_BionicDebArm", "Uv"],
    "Python": ["Package", "BinaryPackage", "DebPackage", "_BionicDebArm", "Python"],
    "Nodejs": ["Package", "BinaryPackage", "DebPackage", "_BionicDebArm", "Nodejs"],
    "TermuxDocker": ["Package", "TermuxDocker"],
    "Npm": ["Package", "BinaryPackage", "Npm"],
    "Pnpm": ["Package", "BinaryPackage", "Pnpm"],
    "Git": ["Package", "BinaryPackage", "Git"],
    "Gh": ["Package", "BinaryPackage", "Gh"],
    "Ffmpeg": ["Package", "BinaryPackage", "DebPackage", "_BionicDebArm", "Ffmpeg"],
    "Ripgrep": ["Package", "BinaryPackage", "Ripgrep"],
    "Just": ["Package", "BinaryPackage", "Just"],
    "CuaDriver": ["Package", "BinaryPackage", "CuaDriver"],
    "Chromium": ["Package", "Chromium"],
    "LlamaCpp": ["Package", "BinaryPackage", "LlamaCpp"],
    "LlamaCppCuda": ["Package", "BinaryPackage", "LlamaCpp", "LlamaCppCuda"],
    "LlamaCppCpu": ["Package", "BinaryPackage", "LlamaCpp", "LlamaCppCpu"],
    "WhisperCppCpu": ["Package", "BinaryPackage", "WhisperCppCpu"],
}
SELF = {}
for cls, chain in MRO.items():
    d = {}
    for c in chain:
        d.update(ATTRS.get(c, {}))
    SELF[cls] = {k: jsonable(v) for k, v in d.items()}

CASES = []


def S(cls, **over):
    d = dict(SELF[cls])
    d.update(over)
    return d


def C(func, self=None, io=None, state=None, fc=False, **params):
    args = dict(params)
    if self is not None:
        args["self"] = self
    inp = {"args": args}
    if io is not None:
        inp["io"] = io
    if state is not None:
        inp["state"] = state
    if fc:
        inp["expect_fail_closed"] = True
    CASES.append({"func": func, "in": inp})


def R(type, message="", **kw):
    return {"__script__": True, "raise": dict(type=type, message=message, **kw)}


def V(ret):
    return {"__script__": True, "ret": ret}


def Q1(v):
    """one-call queue whose single result is v (list/tuple returns)."""
    return [v]


# -- npm_env --
C("npm_env", cache_dir="/c", base_env={"PATH": "/usr/bin", "NPM_CONFIG_REGISTRY": "x",
                                      "node_options": "y", "NODE_PATH": "z", "A": "b"})
C("npm_env", cache_dir="/c", io={"os_environ": {"PATH": "/u", "npm_config_x": "1", "NODE_ENV": "dev", "K": "v"}})
C("npm_env", cache_dir="/c", base_env={})
C("npm_env", cache_dir=None, base_env={"A": "1"})
C("npm_env", cache_dir="/new", base_env={"npm_config_cache": "old", "B": "2"})
C("npm_env", cache_dir="/c", io={"os_environ": None})
C("npm_env", cache_dir="/c", base_env=["not", "a", "dict"])

# -- pnpm_env --
C("pnpm_env", base_env={"PNPM_CONFIG_STORE_DIR": "old", "PATH": "/u"},
  io={"get_default_hermes_root": "/h"})
C("pnpm_env", io={"os_environ": {"pnpm_config_cache_dir": "keep", "PNPM_CONFIG_CACHE_DIR": "twin"},
                  "get_default_hermes_root": "/h"})
C("pnpm_env", base_env={}, io={"get_default_hermes_root": "/h"})
C("pnpm_env", base_env={}, io={"get_default_hermes_root": None})
C("pnpm_env", io={"os_environ": None, "get_default_hermes_root": "/h"})
C("pnpm_env", base_env={}, fc=True)
C("pnpm_env", base_env={"PNPM_CONFIG_STATE_DIR": "a", "Pnpm_Config_State_Dir": "b"},
  io={"get_default_hermes_root": "/h"})

# -- uv_cache_path --
C("uv_cache_path", io={"get_default_hermes_root": "/h"})
C("uv_cache_path", io={"get_default_hermes_root": "/h/"})
C("uv_cache_path", io={"get_default_hermes_root": None})
C("uv_cache_path", fc=True)
C("uv_cache_path", io={"get_default_hermes_root": R("SystemExit", "bye")})
C("uv_cache_path", io={"get_default_hermes_root": "/"})

# -- _github_release_digests --
C("_github_release_digests", repo="casey/just", tag="1.0",
  io={"_get_json": {"assets": [{"name": "a.tar.gz", "digest": "sha256:abc"},
                               {"name": "b", "digest": ""}, {"name": "c"}]}}, state={})
C("_github_release_digests", repo="casey/just", tag="1.0",
  state={"release_digest_cache": {"casey/just": {"1.0": {"a": "abc"}}}})
C("_github_release_digests", repo="r", tag="t", io={"_get_json": R("ValueError", "bad json")}, state={})
C("_github_release_digests", repo="r", tag="t", io={"_get_json": R("SystemExit", "bye")}, state={})
C("_github_release_digests", repo="r", tag="t", io={"_get_json": None}, state={})
C("_github_release_digests", repo="r", tag="t",
  io={"_get_json": {"assets": [{"digest": "sha256:abc"}]}}, state={})
C("_github_release_digests", repo="r", tag="t", io={"_get_json": {"assets": None}}, state={})
C("_github_release_digests", repo="r", tag="t",
  io={"_get_json": {"assets": [{"name": "a", "digest": 5}]}}, state={})
C("_github_release_digests", repo="r", tag="t", fc=True, state={})
C("_github_release_digests", repo="r", tag="t",
  io={"_get_json": {"assets": [{"name": "a", "digest": "md5:xxx"}]}}, state={})
C("_github_release_digests", repo="r", tag="t", io={"_get_json": {"assets": []}}, state={})
C("_github_release_digests", repo="r", tag="t", io={"_get_json": {"assets": ["x"]}}, state={})

# -- BinaryPackage._rel --
C("BinaryPackage._rel", self=S("Uv"), target="win32-x64")
C("BinaryPackage._rel", self=S("Uv"), target="win32-arm64")
C("BinaryPackage._rel", self=S("Uv"), target="linux-x64")
C("BinaryPackage._rel", self=S("Uv"), target="darwin-arm64")
C("BinaryPackage._rel", self=S("Uv", binary_rel={"win32-x64": "", "win32": "uv.exe"}), target="win32-x64")
C("BinaryPackage._rel", self={}, target="linux-x64")
C("BinaryPackage._rel", self={"binary_rel": None}, target="linux-x64")
C("BinaryPackage._rel", self=S("Uv", binary_rel={"posix": "uv"}), target="win32-x64")

# -- BinaryPackage.stage --
C("BinaryPackage.stage", self=S("Uv"), staged="/s", version="0.12.3", target="linux-x64",
  io={"flatten_single_dir": None})
C("BinaryPackage.stage", self=S("Dmgbuild"), staged="/s", version="1+2", target="darwin-x64")
C("BinaryPackage.stage", self=S("Uv", flatten=0), staged="/s", version="v", target="linux-x64")
C("BinaryPackage.stage", self=S("Uv"), staged="/s", version="v", target="linux-x64",
  io={"flatten_single_dir": R("PermissionError", "denied", errno=13)})
C("BinaryPackage.stage", self=S("Uv"), staged="/s", version="v", target="linux-x64", fc=True)
C("BinaryPackage.stage", self={}, staged="/s", version="v", target="linux-x64")

# -- BinaryPackage.binary --
C("BinaryPackage.binary", self=S("Uv"), entry="/e", target="linux-x64")
C("BinaryPackage.binary", self=S("Uv"), entry="/e", target="win32-x64")
C("BinaryPackage.binary", self=S("Uv", binary_rel={}), entry="/e", target="linux-x64")
C("BinaryPackage.binary", self=S("Uv"), entry=None, target="linux-x64")
C("BinaryPackage.binary", self=S("Ffmpeg"), entry="/e", target="linux-x64")
C("BinaryPackage.binary", self=S("Uv"), entry="/e", target=None)

# -- BinaryPackage._probe_env --
C("BinaryPackage._probe_env", self=S("Uv", deps=[]), io={"os_environ": {"A": "1"}})
C("BinaryPackage._probe_env", self=S("Uv", deps=None), io={"os_environ": {"A": "1"}})
C("BinaryPackage._probe_env", self=S("Npm"), io={"env_for": {"PATH": "/n"}})
C("BinaryPackage._probe_env", self=S("Uv", deps=["node", "python"]), io={"env_for": {"PATH": "/n"}})
C("BinaryPackage._probe_env", self=S("Uv", deps=[]), io={"os_environ": None})
C("BinaryPackage._probe_env", self=S("Npm"), io={"env_for": R("ValueError", "no deps")})
C("BinaryPackage._probe_env", self=S("Uv", deps="node"), io={"env_for": {"PATH": "/n"}})
C("BinaryPackage._probe_env", self={})

# -- BinaryPackage.verify --
VSELF = S("Uv", deps=[])
VIO = {"_binary_reason": "", "current_target": "linux-x64", "native": "/e/uv",
       "os_environ": {"PATH": "/u"}, "subprocess_run": {"returncode": 0, "stdout": "uv 0.12.3", "stderr": ""}}
C("BinaryPackage.verify", self=VSELF, entry="/e", target="linux-x64", io=dict(VIO))
C("BinaryPackage.verify", self=S("Uv", binary_rel={}), entry="/e", target="linux-x64")
C("BinaryPackage.verify", self=VSELF, entry="/e", target="linux-x64",
  io={"_binary_reason": "uv is not a linux-x64 binary"})
C("BinaryPackage.verify", self=S("Uv", probe_version=False, deps=[]), entry="/e", target="linux-x64",
  io={"_binary_reason": ""})
C("BinaryPackage.verify", self=VSELF, entry="/e", target="linux-x64",
  io=dict(VIO, current_target="win32-x64"))
C("BinaryPackage.verify", self=VSELF, entry="/e", target="linux-x64",
  io=dict(VIO, native=[R("FileNotFoundError", "no such file", errno=2), "/e/uv"]))
C("BinaryPackage.verify", self=VSELF, entry="/e", target="linux-x64",
  io=dict(VIO, subprocess_run=R("TimeoutExpired", cmd=["/e/uv", "--version"], timeout=60)))
C("BinaryPackage.verify", self=VSELF, entry="/e", target="linux-x64",
  io=dict(VIO, subprocess_run={"returncode": 1, "stdout": "", "stderr": "boom"},
          _probe_reason="uv exited 1: boom"))
C("BinaryPackage.verify", self=S("Uv", probe_cwd=True, deps=[]), entry="/e", target="linux-x64",
  io=dict(VIO, native=["/e/uv", "/e"]))
C("BinaryPackage.verify", self=S("Npm"), entry="/e", target="linux-x64",
  io={"_binary_reason": "", "current_target": "linux-x64", "native": "/e/bin/npm",
      "env_for": {"PATH": "/n"}, "subprocess_run": {"returncode": 0}})
C("BinaryPackage.verify", self=VSELF, entry="/e", target="linux-x64",
  io=dict(VIO, subprocess_run=R("SystemExit", "bye")))
C("BinaryPackage.verify", self=VSELF, entry="/e", target="linux-x64",
  io=dict(VIO, subprocess_run=None))
C("BinaryPackage.verify", self=VSELF, entry="/e", target="linux-x64",
  io={"_binary_reason": "", "native": "/e/uv", "os_environ": {}}, fc=True)
C("BinaryPackage.verify", self=VSELF, entry="/e", target="linux-x64",
  io=dict(VIO, subprocess_run={"returncode": False}))
C("BinaryPackage.verify", self=VSELF, entry="/e", target="linux-x64",
  io=dict(VIO, subprocess_run={"stdout": "x"}))

# -- Dmgbuild --
C("Dmgbuild.fetch_url", self=S("Dmgbuild"), version="1.2.3+4", target="darwin-arm64")
C("Dmgbuild.fetch_url", self=S("Dmgbuild"), version="1.2.3+4", target="darwin-x64")
C("Dmgbuild.fetch_url", self=S("Dmgbuild"), version="1.2.3", target="darwin-x64")
C("Dmgbuild.fetch_url", self=S("Dmgbuild"), version="1+2", target="linux-x64")
C("Dmgbuild.fetch_url", self=S("Dmgbuild"), version=None, target="darwin-x64")
C("Dmgbuild.fetch_url", self=S("Dmgbuild"), version="1+2", target=None)
C("Dmgbuild.verify", self=S("Dmgbuild"), entry="/e", target="darwin-x64",
  io={"super_verify": "bad"})
C("Dmgbuild.verify", self=S("Dmgbuild"), entry="/e", target="darwin-x64",
  io={"super_verify": "", "_binary_reason": ""})
C("Dmgbuild.verify", self=S("Dmgbuild"), entry="/e", target="darwin-x64",
  io={"super_verify": "", "_binary_reason": "python3 missing"})
C("Dmgbuild.verify", self=S("Dmgbuild"), entry="/e", target="darwin-x64",
  io={"super_verify": R("OSError", "disk", errno=5)})
C("Dmgbuild.verify", self=S("Dmgbuild"), entry=None, target="darwin-x64",
  io={"super_verify": ""})
C("Dmgbuild.verify", self=S("Dmgbuild"), entry="/e", target="darwin-x64", fc=True)

# -- _BionicDebArm.verify --
C("_BionicDebArm.verify", self=S("Uv"), entry="/e", target="linux-arm64-bionic",
  io={"deb_verify": ""})
C("_BionicDebArm.verify", self=S("Uv"), entry="/e", target="linux-x64", io={"bp_verify": ""})
C("_BionicDebArm.verify", self=S("Uv"), entry="/e", target="linux-arm64-bionic",
  io={"deb_verify": R("RuntimeError", "deb bad")})
C("_BionicDebArm.verify", self=S("Uv"), entry="/e", target="win32-x64", io={"bp_verify": "reason"})
C("_BionicDebArm.verify", self=S("Uv"), entry="/e", target="linux-x64", fc=True)

# -- Uv --
C("Uv.fetch_url", self=S("Uv"), version="0.12.3", target="linux-arm64-bionic")
C("Uv.fetch_url", self=S("Uv"), version="0.12.3", target="win32-x64")
C("Uv.fetch_url", self=S("Uv"), version="0.12.3", target="linux-x64")
C("Uv.fetch_url", self=S("Uv"), version="0.12.3", target="linux-x64-musl")
C("Uv.fetch_url", self=S("Uv"), version="0.12.3", target="darwin-arm64")
C("Uv.fetch_url", self=S("Uv"), version="0.12.3", target="plan9-x64")
C("Uv.fetch_url", self=S("Uv"), version=None, target="linux-x64")
C("Uv.latest_versions", self=S("Uv"), target="linux-x64",
  io={"github_release_tags": Q1(["0.12.3", "0.12.2"])})
C("Uv.latest_versions", self=S("Uv"), target="linux-x64", io={"github_release_tags": Q1([])})
C("Uv.latest_versions", self=S("Uv"), target="linux-x64",
  io={"github_release_tags": R("ValueError", "api")})
C("Uv.latest_versions", self=S("Uv"), target="linux-x64",
  io={"github_release_tags": R("SystemExit", "bye")})
C("Uv.latest_versions", self=S("Uv"), target="linux-x64", fc=True)

# -- Python --
C("Python.fetch_url", self=S("Python"), version="3.14.6-1", target="linux-arm64-bionic")
C("Python.fetch_url", self=S("Python"), version="3.14.7+20260901", target="linux-x64")
C("Python.fetch_url", self=S("Python"), version="3.14.7", target="linux-x64")
C("Python.fetch_url", self=S("Python"), version="3.14.7+", target="linux-x64")
C("Python.fetch_url", self=S("Python"), version="3.14.7+20260901", target="win32-arm64")
C("Python.fetch_url", self=S("Python"), version="3.14.7+20260901", target="plan9-x64")
C("Python.fetch_url", self={k: v for k, v in S("Python").items() if k != "_BIONIC_URL"},
  version="v", target="linux-arm64-bionic")
C("Python.latest_versions", self=S("Python"), target="linux-arm64-bionic", locked="3.14.7+x")
C("Python.latest_versions", self=S("Python"), target="linux-x64", locked=None)
C("Python.latest_versions", self=S("Python"), target="linux-x64", locked="3.14.7")
C("Python.latest_versions", self=S("Python"), target="linux-x64", locked="3.14.7+20260901",
  io={"pbs_versions": Q1(["3.14.8+20261001"])})
C("Python.latest_versions", self=S("Python"), target="linux-x64", locked="3.14.7+x",
  io={"pbs_versions": R("ValueError", "api")})
C("Python.latest_versions", self=S("Python"), target="linux-x64", locked=5)
C("Python.latest_versions", self=S("Python"), target="plan9-x64", locked="3.14.7+x")
C("Python.latest_versions", self=S("Python"), target="linux-x64", locked="")
C("Python.verify", self=S("Python"), entry="/e", target="win32-x64", io={"super_verify": "reason"})
C("Python.verify", self=S("Python"), entry="/e", target="linux-x64", io={"super_verify": ""})
C("Python.verify", self=S("Python"), entry="/e", target="win32-x64",
  io={"super_verify": "", "read_bytes": ["abc", "abc"]})
C("Python.verify", self=S("Python"), entry="/e", target="win32-arm64",
  io={"super_verify": "", "read_bytes": ["abc", "abd"]})
C("Python.verify", self=S("Python"), entry="/e", target="win32-x64",
  io={"super_verify": "", "read_bytes": R("FileNotFoundError", "x", errno=2)})
C("Python.verify", self=S("Python"), entry="/e", target="win32-x64",
  io={"super_verify": "", "read_bytes": ["abc", R("FileNotFoundError", "x", errno=2)]})
C("Python.verify", self=S("Python"), entry="/e", target="win32-x64",
  io={"super_verify": "", "read_bytes": R("SystemExit", "bye")})
C("Python.verify", self=S("Python"), entry=None, target="win32-x64", io={"super_verify": ""})

# -- Nodejs --
C("Nodejs.fetch_url", self=S("Nodejs"), version="26.7.0", target="linux-arm64-bionic")
C("Nodejs.fetch_url", self=S("Nodejs"), version="26.7.0", target="win32-x64")
C("Nodejs.fetch_url", self=S("Nodejs"), version="26.7.0", target="linux-x64-musl")
C("Nodejs.fetch_url", self=S("Nodejs"), version="26.7.0", target="darwin-arm64")
C("Nodejs.fetch_url", self=S("Nodejs"), version="26.7.0", target="plan9-x64")
C("Nodejs.fetch_url", self=S("Nodejs"), version=None, target="linux-x64")
C("Nodejs.latest_versions", self=S("Nodejs"), target="linux-x64",
  io={"node_latest_versions": Q1(["26.7.0", "26.6.0"])})
C("Nodejs.latest_versions", self=S("Nodejs"), target="linux-x64", io={"node_latest_versions": Q1([])})
C("Nodejs.latest_versions", self=S("Nodejs"), target="linux-x64",
  io={"node_latest_versions": R("ValueError", "api")})
C("Nodejs.latest_versions", self=S("Nodejs"), target="linux-x64",
  io={"node_latest_versions": R("KeyboardInterrupt", "")})
C("Nodejs.latest_versions", self=S("Nodejs"), target="linux-x64", fc=True)
REASON = "node: error while loading shared libraries: libatomic.so.1"
C("Nodejs.repair_staged_verification", self=S("Nodejs"), entry="/e", target="win32-x64",
  reason=REASON, io={"current_target": "win32-x64"})
C("Nodejs.repair_staged_verification", self=S("Nodejs"), entry="/e", target="linux-x64",
  reason="something else", io={"current_target": "linux-x64"})
C("Nodejs.repair_staged_verification", self=S("Nodejs"), entry="/e", target="linux-x64",
  reason=REASON, io={"current_target": "linux-x64", "try_install_libatomic": Q1([False, "install libatomic"])})
C("Nodejs.repair_staged_verification", self=S("Nodejs"), entry="/e", target="linux-x64",
  reason=REASON, io={"current_target": "linux-x64", "try_install_libatomic": Q1([True, ""]),
                     "self_verify": ""})
C("Nodejs.repair_staged_verification", self=S("Nodejs"), entry="/e", target="linux-arm64",
  reason=REASON, io={"current_target": "linux-arm64", "try_install_libatomic": Q1([True, ""]),
                     "self_verify": "still cannot load libatomic.so.1"})
C("Nodejs.repair_staged_verification", self=S("Nodejs"), entry="/e", target="linux-x64",
  reason=REASON, io={"current_target": "linux-x64", "try_install_libatomic": Q1([1, 2, 3])})
C("Nodejs.repair_staged_verification", self=S("Nodejs"), entry="/e", target="linux-x64",
  reason=REASON, io={"current_target": "linux-x64", "try_install_libatomic": None})
C("Nodejs.repair_staged_verification", self=S("Nodejs"), entry="/e", target="linux-x64",
  reason=REASON, io={"current_target": "linux-x64", "try_install_libatomic": Q1([True, ""]),
                     "self_verify": R("OSError", "x", errno=5)})
C("Nodejs.repair_staged_verification", self=S("Nodejs"), entry="/e", target="linux-x64",
  reason=None, io={"current_target": "linux-x64"})
C("Nodejs.repair_staged_verification", self=S("Nodejs"), entry="/e", target="linux-x64",
  reason=REASON, fc=True)

# -- TermuxDocker --
C("TermuxDocker.missing_reason", self=S("TermuxDocker"), target="linux-arm64-bionic")
C("TermuxDocker.missing_reason", self=S("TermuxDocker"), target="win32-x64")
C("TermuxDocker.missing_reason", self=S("TermuxDocker"), target="linux-x64")
C("TermuxDocker.missing_reason", self=S("TermuxDocker"), target="darwin-arm64")
C("TermuxDocker.missing_reason", self=S("TermuxDocker"), target=None)
C("TermuxDocker.fetch_url", self=S("TermuxDocker"), version="sha256:abc", target="linux-arm64-bionic")
C("TermuxDocker.fetch_url", self=S("TermuxDocker"), version="sha256:def", target="linux-x64")
C("TermuxDocker.fetch_url", self=S("TermuxDocker"), version=None, target="linux-arm64-bionic")
C("TermuxDocker.fetch_url", self=S("TermuxDocker"), version="", target="linux-arm64-bionic")
C("TermuxDocker.fetch_url", self=S("TermuxDocker"), version="sha256:0123456789", target="darwin-x64")

# -- Npm / Pnpm --
C("Npm.latest_versions", self=S("Npm"), target="linux-x64", io={"npm_dist_tags": {"latest": "11.0.0"}})
C("Npm.latest_versions", self=S("Npm"), target="linux-x64", io={"npm_dist_tags": {"latest": ""}})
C("Npm.latest_versions", self=S("Npm"), target="linux-x64", io={"npm_dist_tags": {"latest": None}})
C("Npm.latest_versions", self=S("Npm"), target="linux-x64", io={"npm_dist_tags": {}})
C("Npm.latest_versions", self=S("Npm"), target="linux-x64", io={"npm_dist_tags": None})
C("Npm.latest_versions", self=S("Npm"), target="linux-x64", io={"npm_dist_tags": R("ValueError", "api")})
C("Npm.latest_versions", self=S("Npm"), target="linux-x64", fc=True)
C("Pnpm.fetch_urls", self=S("Pnpm"), version="12.0.0", target="linux-arm64-bionic")
C("Pnpm.fetch_urls", self=S("Pnpm"), version="12.0.0", target="win32-x64")
C("Pnpm.fetch_urls", self=S("Pnpm"), version="12.0.0", target="linux-x64")
C("Pnpm.fetch_urls", self=S("Pnpm", exe_platforms=None), version="12.0.0", target="linux-x64")
C("Pnpm.fetch_urls", self={}, version="12.0.0", target="linux-x64")
C("Pnpm.fetch_urls", self=S("Pnpm"), version=None, target="linux-x64")
C("Pnpm.latest_versions", self=S("Pnpm"), target="linux-x64", io={"npm_dist_tags": {"latest": "12.0.0"}})
C("Pnpm.latest_versions", self=S("Pnpm"), target="linux-x64", io={"npm_dist_tags": {"latest": ""}})
C("Pnpm.latest_versions", self=S("Pnpm"), target="linux-x64", io={"npm_dist_tags": {}})
C("Pnpm.latest_versions", self=S("Pnpm"), target="linux-x64", io={"npm_dist_tags": None})
C("Pnpm.latest_versions", self=S("Pnpm"), target="linux-x64", fc=True)

# -- Git --
C("Git.fetch_url", self=S("Git"), version="2.56.0+1", target="win32-x64")
C("Git.fetch_url", self=S("Git"), version="2.56.0+3", target="win32-x64")
C("Git.fetch_url", self=S("Git"), version="2.56.0+2", target="win32-arm64")
C("Git.fetch_url", self=S("Git"), version="2.56.0", target="win32-x64")
C("Git.fetch_url", self=S("Git"), version="2.56.0+1+2", target="win32-x64")
C("Git.fetch_url", self=S("Git"), version=None, target="win32-x64")
C("Git.fetch_url", self=S("Git"), version="2.56.0+1", target=None)
C("Git.latest_versions", self=S("Git"), target="win32-x64",
  io={"github_release_tags": Q1(["2.56.0.windows.1", "2.55.0.windows.2", "nightly"])})
C("Git.latest_versions", self=S("Git"), target="win32-x64", io={"github_release_tags": Q1([])})
C("Git.latest_versions", self=S("Git"), target="win32-x64", io={"github_release_tags": None})
C("Git.latest_versions", self=S("Git"), target="win32-x64", io={"github_release_tags": "abc"})
C("Git.latest_versions", self=S("Git"), target="win32-x64",
  io={"github_release_tags": R("ValueError", "api")})
C("Git.latest_versions", self=S("Git"), target="win32-x64", fc=True)
C("Git.env", self=S("Git"), entry="/e", target="win32-x64")
C("Git.env", self=S("Git"), entry="/e/", target="win32-x64")
C("Git.env", self=S("Git"), entry=None, target="win32-x64")
C("Git.env", self=S("Git"), entry="/", target="win32-x64")
C("Git.env", self=S("Git"), entry="/a/b", target="win32-arm64")

# -- Gh --
C("Gh.fetch_url", self=S("Gh"), version="2.60.0", target="win32-x64")
C("Gh.fetch_url", self=S("Gh"), version="2.60.0", target="linux-x64")
C("Gh.fetch_url", self=S("Gh"), version="2.60.0", target="linux-arm64-musl")
C("Gh.fetch_url", self=S("Gh"), version="2.60.0", target="darwin-arm64")
C("Gh.fetch_url", self=S("Gh"), version="2.60.0", target="plan9-x64")
C("Gh.fetch_url", self=S("Gh"), version="2.60.0", target="linux-x64-extra")
C("Gh.fetch_url", self=S("Gh"), version="2.60.0", target="linux-ppc64")
C("Gh.fetch_url", self=S("Gh"), version="2.60.0", target=None)
C("Gh.latest_versions", self=S("Gh"), target="linux-x64",
  io={"github_release_tags": Q1(["2.60.0", "2.59.0"])})
C("Gh.latest_versions", self=S("Gh"), target="linux-x64", io={"github_release_tags": Q1([])})
C("Gh.latest_versions", self=S("Gh"), target="linux-x64",
  io={"github_release_tags": R("ValueError", "api")})
C("Gh.latest_versions", self=S("Gh"), target="linux-x64",
  io={"github_release_tags": R("GeneratorExit", "")})
C("Gh.latest_versions", self=S("Gh"), target="linux-x64", fc=True)

# -- Ffmpeg --
C("Ffmpeg.fetch_url", self=S("Ffmpeg"), version="9.0.1", target="linux-arm64-bionic")
C("Ffmpeg.fetch_url", self=S("Ffmpeg"), version="9.0", target="linux-x64-musl",
  io={"missing_reason": "BtbN Linux builds link glibc dynamically"})
C("Ffmpeg.fetch_url", self=S("Ffmpeg"), version="9.0", target="win32-x64",
  io={"btbn_index": {"win32-x64": {"9.0": ["n9.0-1", "ffmpeg-n9.0-win64.zip"]}}})
C("Ffmpeg.fetch_url", self=S("Ffmpeg"), version="9.0", target="linux-x64",
  io={"btbn_index": {"linux-x64": {"9.0": ["n9.0-1", "ffmpeg-n9.0-linux64.tar.xz"]}}})
C("Ffmpeg.fetch_url", self=S("Ffmpeg"), version="9.0", target="win32-x64", io={"btbn_index": {}})
C("Ffmpeg.fetch_url", self=S("Ffmpeg"), version="9.0", target="darwin-x64",
  io={"martin_riedl_index": {"darwin-x64": {"9.0": "1730000000"}}})
C("Ffmpeg.fetch_url", self=S("Ffmpeg"), version="9.0", target="darwin-arm64",
  io={"martin_riedl_index": {"darwin-arm64": {"9.0": "1730000000"}}})
C("Ffmpeg.fetch_url", self=S("Ffmpeg"), version="9.0", target="darwin-x64",
  io={"martin_riedl_index": {}})
C("Ffmpeg.fetch_url", self=S("Ffmpeg"), version="9.0", target="win32-x64", io={"btbn_index": None})
C("Ffmpeg.fetch_url", self=S("Ffmpeg"), version="9.0", target="win32-x64",
  io={"btbn_index": {"win32-x64": {"9.0": [1, 2, 3]}}})
C("Ffmpeg.fetch_url", self=S("Ffmpeg"), version="9.0", target="linux-arm64-musl",
  io={"missing_reason": R("SystemExit", "bye")})
C("Ffmpeg.fetch_url", self=S("Ffmpeg"), version="9.0", target="win32-x64",
  io={"btbn_index": {"win32-x64": {"9.0": None}}})
C("Ffmpeg.fetch_url", self=S("Ffmpeg"), version="9.0", target="linux")
C("Ffmpeg.latest_versions", self=S("Ffmpeg"), target="linux-x64-musl")
C("Ffmpeg.latest_versions", self=S("Ffmpeg"), target="win32-x64",
  io={"btbn_versions": Q1(["9.0", "8.9"])})
C("Ffmpeg.latest_versions", self=S("Ffmpeg"), target="linux-arm64",
  io={"btbn_versions": Q1(["9.0"])})
C("Ffmpeg.latest_versions", self=S("Ffmpeg"), target="darwin-x64",
  io={"martin_riedl_versions": Q1(["9.0.1"])})
C("Ffmpeg.latest_versions", self=S("Ffmpeg"), target="win32-x64",
  io={"btbn_versions": R("ValueError", "api")})
C("Ffmpeg.latest_versions", self=S("Ffmpeg"), target="darwin-arm64",
  io={"martin_riedl_versions": R("ValueError", "api")})
C("Ffmpeg.latest_versions", self=S("Ffmpeg"), target="linux-arm64-bionic",
  io={"martin_riedl_versions": Q1([])})

# -- Ripgrep / Just --
C("Ripgrep.fetch_url", self=S("Ripgrep"), version="14.1.1", target="linux-arm64-bionic")
C("Ripgrep.fetch_url", self=S("Ripgrep"), version="14.1.1", target="win32-x64")
C("Ripgrep.fetch_url", self=S("Ripgrep"), version="14.1.1", target="linux-x64")
C("Ripgrep.fetch_url", self=S("Ripgrep"), version="14.1.1", target="darwin-arm64")
C("Ripgrep.fetch_url", self=S("Ripgrep"), version="14.1.1", target="plan9-x64")
C("Ripgrep.fetch_url", self=S("Ripgrep"), version=None, target="linux-x64")
C("Ripgrep.latest_versions", self=S("Ripgrep"), target="linux-x64",
  io={"github_release_tags": Q1(["14.1.1"])})
C("Ripgrep.latest_versions", self=S("Ripgrep"), target="linux-x64", io={"github_release_tags": Q1([])})
C("Ripgrep.latest_versions", self=S("Ripgrep"), target="linux-x64",
  io={"github_release_tags": R("ValueError", "api")})
C("Ripgrep.latest_versions", self=S("Ripgrep"), target="linux-x64",
  io={"github_release_tags": R("SystemExit", "bye")})
C("Ripgrep.latest_versions", self=S("Ripgrep"), target="linux-x64", fc=True)
C("Just.fetch_url", self=S("Just"), version="1.40.0", target="linux-arm64-bionic")
C("Just.fetch_url", self=S("Just"), version="1.40.0", target="win32-x64")
C("Just.fetch_url", self=S("Just"), version="1.40.0", target="linux-x64")
C("Just.fetch_url", self=S("Just"), version="1.40.0", target="darwin-arm64")
C("Just.fetch_url", self=S("Just"), version="1.40.0", target="plan9-x64")
C("Just.known_sha256", self=S("Just"), version="1.40.0",
  url="https://github.com/casey/just/releases/download/1.40.0/just-1.40.0-x.tar.gz",
  io={"github_release_digests": {"just-1.40.0-x.tar.gz": "abc"}})
C("Just.known_sha256", self=S("Just"), version="1.40.0", url="just.tar.gz",
  io={"github_release_digests": {"just.tar.gz": "abc"}})
C("Just.known_sha256", self=S("Just"), version="1.40.0", url="x/just.tar.gz",
  io={"github_release_digests": None})
C("Just.known_sha256", self=S("Just"), version="1.40.0", url="x/other.tar.gz",
  io={"github_release_digests": {"just.tar.gz": "abc"}})
C("Just.known_sha256", self=S("Just"), version="1.40.0", url="x/a",
  io={"github_release_digests": R("ValueError", "api")})
C("Just.known_sha256", self=S("Just"), version="1.40.0", url=None,
  io={"github_release_digests": {}})
C("Just.known_sha256", self=S("Just"), version="1.40.0", url="x/a", fc=True)
C("Just.latest_versions", self=S("Just"), target="linux-x64",
  io={"github_release_tags": Q1(["1.40.0"])})
C("Just.latest_versions", self=S("Just"), target="linux-x64", io={"github_release_tags": Q1([])})
C("Just.latest_versions", self=S("Just"), target="linux-x64",
  io={"github_release_tags": R("ValueError", "api")})
C("Just.latest_versions", self=S("Just"), target="linux-x64",
  io={"github_release_tags": R("KeyboardInterrupt", "")})
C("Just.latest_versions", self=S("Just"), target="linux-x64", fc=True)

# -- CuaDriver --
C("CuaDriver.fetch_url", self=S("CuaDriver"), version="0.5.0", target="darwin-arm64")
C("CuaDriver.fetch_url", self=S("CuaDriver"), version="0.5.0", target="win32-x64")
C("CuaDriver.fetch_url", self=S("CuaDriver"), version="0.5.0", target="linux-x64")
C("CuaDriver.fetch_url", self=S("CuaDriver"), version="0.5.0", target="win32-arm64")
C("CuaDriver.fetch_url", self=S("CuaDriver"), version="0.5.0", target="plan9-x64")
C("CuaDriver.fetch_url", self=S("CuaDriver"), version=None, target="linux-x64")
C("CuaDriver.latest_versions", self=S("CuaDriver"), target="linux-x64",
  io={"github_release_tags": Q1(["0.5.0", "0.4.0"])})
C("CuaDriver.latest_versions", self=S("CuaDriver"), target="linux-x64",
  io={"github_release_tags": Q1([])})
C("CuaDriver.latest_versions", self=S("CuaDriver"), target="linux-x64",
  io={"github_release_tags": R("ValueError", "api")})
C("CuaDriver.latest_versions", self=S("CuaDriver"), target="linux-x64",
  io={"github_release_tags": R("SystemExit", "bye")})
C("CuaDriver.latest_versions", self=S("CuaDriver"), target="linux-x64", fc=True)

# -- Chromium --
C("Chromium.store_entry", self=S("Chromium"), version="1148+131.0.6778.85", target="linux-x64")
C("Chromium.store_entry", self=S("Chromium"), version="1148", target="linux-x64")
C("Chromium.store_entry", self=S("Chromium", name="chrome-test"), version="1+2", target="linux-x64")
C("Chromium.store_entry", self=S("Chromium"), version=None, target="linux-x64")
C("Chromium.store_entry", self={}, version="1+2", target="linux-x64")
C("Chromium.fetch_url", self=S("Chromium"), version="1148+131.0.6778.85", target="win32-x64")
C("Chromium.fetch_url", self=S("Chromium"), version="1148+131.0.6778.85", target="win32-arm64")
C("Chromium.fetch_url", self=S("Chromium"), version="1148+131.0.6778.85", target="darwin-arm64")
C("Chromium.fetch_url", self=S("Chromium"), version="1148+131.0.6778.85", target="linux-x64")
C("Chromium.fetch_url", self=S("Chromium"), version="1148+131.0.6778.85", target="plan9-x64")
C("Chromium.fetch_url", self={k: v for k, v in S("Chromium").items() if k != "_CFT"},
  version="1+2", target="linux-x64")
C("Chromium.fetch_url", self=S("Chromium"), version=None, target="linux-x64")

# -- LlamaCpp --
C("LlamaCpp._asset_names", self=S("LlamaCppCpu"), version="12170", target="win32-x64")
C("LlamaCpp._asset_names", self=S("LlamaCppCpu"), version="12170", target="linux-x64")
C("LlamaCpp._asset_names", self=S("LlamaCppCpu"), version="12170", target="darwin-arm64")
C("LlamaCpp._asset_names", self=S("LlamaCppCpu"), version="12170", target="plan9-x64")
C("LlamaCpp._asset_names", self={}, version="12170", target="linux-x64")
C("LlamaCpp._asset_names", self=S("LlamaCppCpu"), version=None, target="linux-x64")
C("LlamaCpp.fetch_urls", self=S("LlamaCppCpu"), version="12170", target="linux-x64")
C("LlamaCpp.fetch_urls", self=S("LlamaCppCpu"), version="12170", target="win32-x64")
C("LlamaCpp.fetch_urls", self=S("LlamaCppCpu"), version="12170", target="win32-arm64")
C("LlamaCpp.fetch_urls", self=S("LlamaCppCpu"), version="12170", target="darwin-arm64")
C("LlamaCpp.fetch_urls", self=S("LlamaCppCpu", assets={"linux-arm64-bionic": "ubuntu-arm64"}),
  version="12170", target="linux-arm64-bionic")
C("LlamaCpp.fetch_urls", self=S("LlamaCppCpu", _LIBGOMP=None), version="12170", target="linux-x64")
C("LlamaCpp.fetch_urls", self=S("LlamaCppCpu"), version="12170", target="linux-arm64")
C("LlamaCpp.fetch_url", self=S("LlamaCppCpu"), version="12170", target="win32-x64")
C("LlamaCpp.fetch_url", self=S("LlamaCppCpu"), version="12170", target="linux-x64")
C("LlamaCpp.fetch_url", self=S("LlamaCppCpu"), version="12170", target="darwin-arm64")
C("LlamaCpp.fetch_url", self=S("LlamaCppCpu"), version="12170", target="plan9-x64")
C("LlamaCpp.fetch_url", self=S("LlamaCppCpu"), version=None, target="linux-x64")
C("LlamaCpp.latest_versions", self=S("LlamaCppCpu"), target="linux-x64",
  io={"llama_app_latest": "12170", "llama_app_bucket_versions": Q1(["12167", "12160"])})
C("LlamaCpp.latest_versions", self=S("LlamaCppCpu"), target="linux-x64",
  io={"llama_app_latest": None, "llama_app_bucket_versions": Q1(["12167"])})
C("LlamaCpp.latest_versions", self=S("LlamaCppCpu"), target="linux-x64",
  io={"llama_app_latest": None, "llama_app_bucket_versions": Q1([]),
      "github_release_tags": Q1(["12100"])})
C("LlamaCpp.latest_versions", self=S("LlamaCppCpu"), target="linux-x64",
  io={"llama_app_latest": "", "llama_app_bucket_versions": Q1(["x"])})
C("LlamaCpp.latest_versions", self=S("LlamaCppCpu"), target="linux-x64",
  io={"llama_app_latest": "1", "llama_app_bucket_versions": None})
C("LlamaCpp.latest_versions", self=S("LlamaCppCpu"), target="linux-x64",
  io={"llama_app_latest": R("ValueError", "api")})
C("LlamaCpp.latest_versions", self=S("LlamaCppCpu"), target="linux-x64",
  io={"llama_app_latest": None, "llama_app_bucket_versions": R("ValueError", "api")})
C("LlamaCpp.latest_versions", self=S("LlamaCppCpu"), target="linux-x64",
  io={"llama_app_latest": None, "llama_app_bucket_versions": Q1([]),
      "github_release_tags": R("ValueError", "api")})
C("LlamaCpp.latest_versions", self=S("LlamaCppCpu"), target="linux-x64", fc=True)
C("LlamaCpp.known_sha256", self=S("LlamaCppCpu"), version="12170",
  url="https://github.com/ggml-org/llama.cpp/releases/download/b12170/llama-b12170-bin-win-cpu-x64.zip",
  io={"github_release_digests": {"llama-b12170-bin-win-cpu-x64.zip": "abc"}})
C("LlamaCpp.known_sha256", self=S("LlamaCppCpu"), version="12170", url="x/other.zip",
  io={"github_release_digests": {"a.zip": "abc"}})
C("LlamaCpp.known_sha256", self=S("LlamaCppCpu"), version="12170", url="x/a.zip",
  io={"github_release_digests": None})
C("LlamaCpp.known_sha256", self=S("LlamaCppCpu"), version="12170", url="x/a.zip",
  io={"github_release_digests": R("ValueError", "api")})
C("LlamaCpp.known_sha256", self=S("LlamaCppCpu"), version="12170", url=None,
  io={"github_release_digests": {}})
C("LlamaCpp.known_sha256", self=S("LlamaCppCpu"), version=None, url="x/a.zip",
  io={"github_release_digests": {}})

# -- LlamaCppCuda --
C("LlamaCppCuda._asset_names", self=S("LlamaCppCuda"), version="12170", target="win32-x64")
C("LlamaCppCuda._asset_names", self=S("LlamaCppCuda"), version="12167", target="win32-x64")
C("LlamaCppCuda._asset_names", self=S("LlamaCppCuda"), version="12170", target="linux-x64")
C("LlamaCppCuda._asset_names", self=S("LlamaCppCuda"), version="12167", target="linux-arm64")
C("LlamaCppCuda._asset_names", self=S("LlamaCppCuda"), version="abc", target="win32-x64")
C("LlamaCppCuda._asset_names", self=S("LlamaCppCuda"), version="12_170", target="win32-x64")
C("LlamaCppCuda._asset_names", self=S("LlamaCppCuda"), version="12170", target="plan9-x64")
C("LlamaCppCuda._asset_names", self=S("LlamaCppCuda"), version=None, target="win32-x64")
C("LlamaCppCuda._asset_names", self=S("LlamaCppCuda"), version=" 12170 ", target="win32-x64")
C("LlamaCppCuda._asset_names", self=S("LlamaCppCuda"), version="12169", target="win32-x64")

# -- WhisperCppCpu --
WURL = "https://github.com/ggml-org/whisper.cpp/releases/download/1.7.6/whisper-bin-win-cpu-arm64.zip"
C("WhisperCppCpu.fetch_urls", self=S("WhisperCppCpu"), version="1.7.6", target="win32-arm64",
  io={"base_fetch_url": WURL})
C("WhisperCppCpu.fetch_urls", self=S("WhisperCppCpu"), version="1.7.6", target="win32-x64",
  io={"base_fetch_url": WURL})
C("WhisperCppCpu.fetch_urls", self=S("WhisperCppCpu"), version="1.7.6", target="linux-x64",
  io={"base_fetch_url": WURL})
C("WhisperCppCpu.fetch_urls", self=S("WhisperCppCpu"), version="1.7.6", target="win32-arm64",
  io={"base_fetch_url": R("RuntimeError", "no url")})
C("WhisperCppCpu.fetch_urls", self=S("WhisperCppCpu"), version="1.7.6", target="win32-arm64", fc=True)
C("WhisperCppCpu.fetch_urls", self=S("WhisperCppCpu"), version=None, target="win32-arm64",
  io={"base_fetch_url": WURL})


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = [f for f, n in counts.items() if n < 5]
    if thin:
        print(f"FATAL: funcs with <5 cases: {thin}")
        sys.exit(2)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"gen_pm_packages_flow_cases: {len(CASES)} cases, {len(counts)} funcs -> {OUT}")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")


if __name__ == "__main__":
    main()
