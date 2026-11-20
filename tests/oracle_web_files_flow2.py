#!/usr/bin/env python3
"""Oracle for web_files_flow2.c — hermes_cli/web_routers/files.py UNCLAIMED FLOW.

LIVE authority: C:/Users/eman5/hermes-agent/hermes_cli/web_routers/files.py
A CRLF-aware PoP grep over src/ + tests/ found NO claim on these five names (the
sibling helpers fs_list / fs_read_text / _fs_mime_type / _chat_image_extension /
_fsync helpers etc. ARE claimed and are NOT re-ported here). This oracle drives
the LIVE five defs, AST-extracted from the quarry (no heavy web_routers import),
with the same fail-closed seam scripts the C harness uses, and compares the
ordered seam trace + return value.

Ported (fixture `func` key -> live def):
  media_proxy_host_allowed     -> _media_proxy_host_allowed
  fs_looks_binary              -> _fs_looks_binary
  sanitize_chat_image_filename -> _sanitize_chat_image_filename
  decode_data_url              -> _decode_data_url
  decode_chat_image_upload     -> _decode_chat_image_upload   (composes the
      first decode + the CLAIMED _chat_image_extension, which is a recorder seam)

Input: stdin JSON lines from t_port_web_files_flow2:
  {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE web_files_flow2: N cases, M mismatches. Fail-closed (exit 2) when
extraction misses a def/constant or zero cases were compared.

Script semantics (identical on both sides, FAIL-CLOSED): a MISSING script returns
a RuntimeError raise spec {"__raise__":"RuntimeError","msg":"no script for seam:
<name>"}; a seams[name] ARRAY is a per-call queue (last value repeats, empty ->
None); a single value is a constant; a scripted null is None (NULL in C);
{"__raise__": cls, "msg": ...} scripts an exception. Byte caps are fixture-
supplied (live defaults 100 MiB / 25 MiB) so the 413 branches are reachable.

Exception contract: HTTPException(status_code, detail) str() is "<status>: <detail>"
(starlette). Scripted raises use REAL builtins (full hierarchy, BaseException trio
genuine); the C port folds a subclass into its ancestor via its exception table.
Per-case BaseException catch: SystemExit / KeyboardInterrupt / GeneratorExit and
ordinary Exceptions round-trip their spec bare; an unscripted exception is captured
as {"__raise__": type(e).__name__, "msg": str(e)}.

Fixture domain: ASCII text; integral JSON numbers read as int; bytes are base64
strings (data URLs) or arrays of int bytes (_fs_looks_binary); str.strip() uses the
ASCII White_Space set; Path.name uses Windows semantics.
"""
import ast
import base64
import binascii
import builtins
import json
import os
import pathlib
import re
import sys
import types

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("HOME", os.path.dirname(_REPO))
os.environ.setdefault("USERPROFILE", os.path.dirname(_REPO))
os.environ.setdefault("LOCALAPPDATA", os.path.join(os.path.dirname(_REPO), "AppData", "Local"))
os.environ.setdefault("HERMES_HOME", os.path.join(_REPO, ".testbin", "dbg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "web_routers", "files.py")
SRC_TEXT = open(SRC, encoding="utf-8", newline="").read()
ANN_FLAG = __import__("__future__").annotations.compiler_flag

FUNCS = {
    "media_proxy_host_allowed": "_media_proxy_host_allowed",
    "fs_looks_binary": "_fs_looks_binary",
    "sanitize_chat_image_filename": "_sanitize_chat_image_filename",
    "decode_data_url": "_decode_data_url",
    "decode_chat_image_upload": "_decode_chat_image_upload",
}
ALL_FUNCS = set(FUNCS)
MANAGE_MAX_DEFAULT = 100 * 1024 * 1024     # _MANAGED_FILE_MAX_BYTES
CHAT_MAX_DEFAULT = 25 * 1024 * 1024       # CHAT_IMAGE_MAX_BYTES / _CHAT_IMAGE_UPLOAD_MAX_BYTES

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": []}


# ── HTTPException (starlette __str__ == "<status>: <detail>") ──

class HTTPException(Exception):
    def __init__(self, status_code, detail):
        super().__init__("%s: %s" % (status_code, detail))
        self.status_code = status_code
        self.detail = detail


# ── exception construction (REAL builtins; BaseException trio must be genuine) ──

def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    b = getattr(builtins, cls, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        try:
            return b(msg)
        except Exception:
            # Unicode*Error builtins need a 5-arg __init__; bypass via __new__.
            try:
                e = b.__new__(b)
                Exception.__init__(e, msg)
                return e
            except Exception:
                # BaseExceptionGroup / ExceptionGroup need (msg, non-empty seq).
                # Their exact type is not observable here (scripted raises are
                # matched by identity), so a BaseException-shaped fallback keeps
                # the spec round-tripping instead of turning into a TypeError.
                return type(cls, (BaseException,), {})(msg)
    return type(cls, (Exception,), {})(msg)


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    scripts = CUR["scripts"]
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "no script for seam: %s" % name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    r = json.loads(json.dumps(r))  # deep copy: live mutation cannot corrupt script/trace
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = build_exc(r)
        CUR["raised"].append((e, r))
        raise e
    return r


# ── normalization / canonical comparison (ordered dict = semantic) ──

def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    o = _numnorm(o)
    if isinstance(o, dict):
        return ("obj", [(k, canon(v)) for k, v in o.items()])
    if isinstance(o, (list, tuple)):
        return ("arr", [canon(v) for v in o])
    if o is None:
        return ("null",)
    if isinstance(o, bool):
        return ("bool", o)
    if isinstance(o, (int, float)):
        return ("num", float(o))
    return ("str", o)


# ── quarry stub: the in-function import inside _decode_data_url ──

_WS = types.ModuleType("hermes_cli.web_server")
_WS._MANAGED_FILE_MAX_BYTES = MANAGE_MAX_DEFAULT
sys.modules.setdefault("hermes_cli", types.ModuleType("hermes_cli"))
sys.modules["hermes_cli.web_server"] = _WS


# ── extraction ──

def extract():
    mod = types.ModuleType("web_files_flow2_extracted")
    sys.modules["web_files_flow2_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "web_files_flow2_extracted",
        "HTTPException": HTTPException,
        "Path": pathlib.Path, "PurePath": pathlib.PurePath,
        "re": re, "base64": base64, "binascii": binascii,
        "json": json, "SimpleNamespace": types.SimpleNamespace,
        "CHAT_IMAGE_MAX_BYTES": CHAT_MAX_DEFAULT,
    })
    tree = ast.parse(SRC_TEXT)

    # Seed module-level constants the extracted defs read. Handle BOTH Assign
    # (node.targets) and AnnAssign (node.target) node types.
    wanted = {"_MEDIA_PROXY_ALLOWED_HOSTS", "_CHAT_IMAGE_ALLOWED_EXTENSIONS",
              "_CHAT_IMAGE_UPLOAD_MAX_BYTES"}
    for node in tree.body:
        targets = []
        if isinstance(node, ast.Assign):
            targets = [t.id for t in node.targets if isinstance(t, ast.Name)]
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            targets = [node.target.id]
        for name in targets:
            if name in wanted:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)

    missing = sorted(wanted - set(ns))
    if missing:
        print("ORACLE web_files_flow2: FATAL missing constants: %s" % missing, file=sys.stderr)
        sys.exit(2)

    by_name = {}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            by_name[node.name] = node
    for key, live in FUNCS.items():
        node = by_name.get(live)
        if node is None:
            print("ORACLE web_files_flow2: FATAL missing def: %s" % live, file=sys.stderr)
            sys.exit(2)
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=ANN_FLAG), ns)
        if not callable(ns.get(live)):
            print("ORACLE web_files_flow2: FATAL not callable: %s" % live, file=sys.stderr)
            sys.exit(2)

    # Composed sibling: the CLAIMED _chat_image_extension is a recorder seam, not
    # a re-ported def. It receives the decoded bytes; project them to base64 so
    # the seam args match the C harness byte-for-byte.
    def _chat_ext_rec(data):
        return seam("_chat_image_extension",
                    {"data_b64": base64.b64encode(bytes(data)).decode("ascii")})
    ns["_chat_image_extension"] = _chat_ext_rec
    return ns


# ── drive ──

def drive(ns, func, inp):
    try:
        if func == "media_proxy_host_allowed":
            ret = bool(ns["_media_proxy_host_allowed"](inp.get("host") or ""))
        elif func == "fs_looks_binary":
            ret = bool(ns["_fs_looks_binary"](bytes(inp.get("data") or [])))
        elif func == "sanitize_chat_image_filename":
            ret = ns["_sanitize_chat_image_filename"](inp.get("filename"))
        elif func == "decode_data_url":
            _WS._MANAGED_FILE_MAX_BYTES = int(inp.get("manage_max_bytes", MANAGE_MAX_DEFAULT))
            data, mime = ns["_decode_data_url"](inp.get("data_url"))
            ret = {"data_b64": base64.b64encode(data).decode("ascii"), "mime": mime}
        elif func == "decode_chat_image_upload":
            _WS._MANAGED_FILE_MAX_BYTES = int(inp.get("manage_max_bytes", MANAGE_MAX_DEFAULT))
            ns["_CHAT_IMAGE_UPLOAD_MAX_BYTES"] = int(inp.get("chat_max_bytes", CHAT_MAX_DEFAULT))
            payload = types.SimpleNamespace(data_url=inp.get("data_url"))
            data, mime, ext = ns["_decode_chat_image_upload"](payload)
            ret = {"data_b64": base64.b64encode(data).decode("ascii"),
                   "mime": mime, "ext": ext}
        else:
            raise KeyError("unknown func: %s" % func)
    except BaseException as e:  # SystemExit included — fail-closed per case
        ret = None
        for exc, spec in CUR["raised"]:
            if e is exc:
                ret = spec  # scripted raise round-trips bare
                break
        if ret is None:
            ret = {"__raise__": type(e).__name__, "msg": str(e)}
    return ret


def main():
    ns = extract()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["raised"] = []
        if func not in ALL_FUNCS:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        ret = drive(ns, func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE web_files_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE web_files_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
