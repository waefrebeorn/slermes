#!/usr/bin/env python3
"""Differential oracle for tools/computer_use/cua_backend_daemon.py (gate w19_cuabackend_0e9d63).

Extracted from the live AST: ``_resolve_cua_driver_app_path``,
``_validate_cua_driver_app_signature``, ``_embedded_daemon_spawn_command`` and the
``_EmbeddedCuaDaemon`` class body (its constructor policy is what is exercised).  The
process/filesystem edges are injected: ``shutil.which``, the ``_cb()`` facade
(``_run_quiet`` / ``_computer_use_cfg``), ``os.path`` answers, ``sys.platform``,
``uuid4`` and ``_driver``.  The start/stop/proxy plumbing talks to a real daemon and is
not exercised.
"""
import ast, json, posixpath, sys, types, __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "tools" / "computer_use" / "cua_backend_daemon.py"
GATE = "w19_cuabackend_0e9d63"
_ANN = __future__.annotations.compiler_flag
CUR = [{}]          # the active fixture row (the injected filesystem/process answers)
__import__("logging").disable(__import__("logging").CRITICAL)


def proc(returncode, stderr):
    return types.SimpleNamespace(returncode=returncode, stderr=stderr, stdout="")


def make_shutil():
    return types.SimpleNamespace(which=lambda name: "/usr/bin/codesign" if CUR[0].get("have_codesign", True) else None)


def make_os():
    def isfile(p):
        r = CUR[0]
        ab = r.get("manifest_abspath")
        if ab and p == ab:
            return bool(r.get("manifest_exists", False))
        if p.endswith("cua-driver"):
            return bool(r.get("isfile", False))
        return False
    path = types.SimpleNamespace(
        realpath=lambda p: CUR[0].get("realpath", p),
        isfile=isfile,
        join=posixpath.join,
        expanduser=lambda p: p,
        abspath=lambda p: CUR[0].get("manifest_abspath") or p,
        exists=lambda p: False,
    )
    return types.SimpleNamespace(path=path, access=lambda p, m: bool(CUR[0].get("x_ok", False)),
                                 join=posixpath.join, F_OK=0, X_OK=1, SEP="/")


def make_cb():
    def run_quiet(args, **kwargs):
        return proc(int(CUR[0].get("returncode", 0)), CUR[0].get("stderr", ""))
    cfg = lambda: {"allow_unsigned_driver": bool(CUR[0].get("allow_unsigned", False))}
    return types.SimpleNamespace(_run_quiet=run_quiet, _computer_use_cfg=cfg,
                                 _manifest_is_mode_independent=lambda p: bool(CUR[0].get("mode_independent", False)),
                                 cua_driver_child_env=lambda: {})


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "cua_daemon_live",
         "subprocess": __import__("subprocess"), "shutil": make_shutil(), "os": make_os(),
         "sys": types.SimpleNamespace(platform=CUR[0].get("platform", "linux")),
         "uuid": types.SimpleNamespace(uuid4=lambda: types.SimpleNamespace(hex="0123456789abcdef0123456789abcdef")),
         "tempfile": types.SimpleNamespace(gettempdir=lambda: "/tmp"),
         "logger": __import__("logging").getLogger("oracle.cua"), "threading": None, "time": None,
         "contextlib": __import__("contextlib"), "deque": (lambda maxlen=None: []),
         "Any": object, "Dict": object,
         "List": object, "Optional": object, "Tuple": object,
         "CUA_DRIVER_BUNDLE_ID": "com.trycua.driver",
         "_CUA_DRIVER_TEAM_IDS": ("4YEC26S9KF", "YCK386LBJ7"),
         "_QUIET_ERRORS": (OSError, __import__("subprocess").SubprocessError),
         "_driver": types.SimpleNamespace(_CUA_DRIVER_ARGS=["--mcp"],
                                          _mcp_args_with_overlay_flag=lambda a, driver_cmd=None: a),
         "_cb": make_cb,
         "release_process_group_once_empty": lambda *a, **k: None,
         "spawn_owned": lambda *a, **k: None}
    wanted = {"_resolve_cua_driver_app_path", "_validate_cua_driver_app_signature",
              "_embedded_daemon_spawn_command", "_EmbeddedCuaDaemon"}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()
Daemon = G["_EmbeddedCuaDaemon"]


def refresh(r):
    CUR[0] = r
    G["shutil"].which = make_shutil().which
    G["os"] = make_os()
    G["sys"] = types.SimpleNamespace(platform=r.get("platform", "linux"))
    G["tempfile"] = types.SimpleNamespace(gettempdir=lambda: r.get("tmpdir") or "/tmp")
    G["uuid"] = types.SimpleNamespace(uuid4=lambda: types.SimpleNamespace(
        hex=(r.get("token") or "") + "0" * 32))
    G["_cb"] = make_cb


def run_case(r):
    refresh(r)
    f = r.get("func")
    if f == "resolve_app_path":
        return G["_resolve_cua_driver_app_path"](r.get("driver_cmd") or "")
    if f == "validate_signature":
        try:
            G["_validate_cua_driver_app_signature"](r.get("app_path") or "")
            return {"ok": True, "error": None}
        except RuntimeError as exc:
            return {"ok": False, "error": str(exc)}
    if f == "spawn_command":
        try:
            return {"ok": True,
                    "command": G["_embedded_daemon_spawn_command"](
                        r.get("driver_cmd") or "", list(r.get("serve_args") or []),
                        platform=r.get("platform") or "", app_path=r.get("app_path")),
                    "error": None}
        except RuntimeError as exc:
            return {"ok": False, "command": None, "error": str(exc)}
    if f == "daemon_init":
        try:
            d = Daemon(r.get("driver_cmd") or "", r.get("permission_mode") or "",
                       r.get("capability_manifest"))
            return {"ok": True, "error": None,
                    "permission_mode": d.permission_mode,
                    "capability_manifest": d.capability_manifest,
                    "manifest_applies": bool(d.manifest_applies),
                    "socket_path": d.socket_path}
        except ValueError as exc:
            return {"ok": False, "error": str(exc), "permission_mode": None,
                    "capability_manifest": None, "manifest_applies": False, "socket_path": None}
    return "<unknown>"


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    f = r.get("func")
    if f not in {"resolve_app_path", "validate_signature", "spawn_command", "daemon_init"}:
        continue
    cases += 1
    try:
        py = run_case(r)
    except BaseException as exc:
        mism += 1
        print("MISMATCH %s oracle raised %r args=%s" % (f, exc, r), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)

print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
sys.exit(1 if mism else 0)
