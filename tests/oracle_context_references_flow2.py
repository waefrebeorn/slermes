#!/usr/bin/env python3
"""Oracle for src/agent/context_references_flow2.c — agent/context_references.py
FLOW remainder (LIVE authority: C:/Users/eman5/hermes-agent/context_references.py).

The 10 ported defs are AST-extracted from the quarry (no heavy
agent.context_references import). Collaborators that other PoP sets own, plus
the foreign-module calls (mimetypes, filesystem stat/open, offline_file_access,
terminal-env bridge, cache-path translation, on-disk block builder) are
recording stubs fed the SAME scripted results the C harness feeds its universal
seam. Internal composition and state transforms run REAL on both sides. Compares
the ordered decision sequence + return + state window with order-sensitive
canon(). Per-case BaseException catch (SystemExit / KeyboardInterrupt /
GeneratorExit escape every `except Exception` / `(LiveConnectionError, OSError)`
and are compared as bare raise specs).

Input: stdin JSON lines from t_port_context_references_flow2:
    {"func":..., "in":{...,"seams":{...}}, "out":{"ret":...,"trace":[...],"state":{...}}}
Summary: ORACLE context_references_flow2: N cases, M mismatches. Fail-closed (2)
on missing defs or zero cases.

Script semantics (identical on both sides, FAIL-CLOSED): a MISSING script
returns a RuntimeError raise spec (presence test, NOT .get() is None); a
seams[name] ARRAY is a per-call queue (last repeats, empty -> None); a single
value is a constant; a scripted null is None; {"__raise__":cls,"msg":...}
scripts an exception (built by a REAL builtin class so `except` binds by
hierarchy identity).
"""
import ast
import builtins
import contextlib
import copy
import json
import os
import re
import sys
import types

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path = None  # placeholders replaced below
import pathlib
_REPO = pathlib.Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "cr2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "agent", "context_references.py")
SRC_TEXT = open(SRC, encoding="utf-8").read().replace("\r", "")
ANN_FLAG = __import__("__future__").annotations.compiler_flag

# func key -> live def name
PY = {
    "format_reference_value": "format_reference_value",
    "strip_trailing_punctuation": "_strip_trailing_punctuation",
    "strip_reference_wrappers": "_strip_reference_wrappers",
    "parse_file_reference_value": "_parse_file_reference_value",
    "get_providers": "get_context_reference_providers",
    "is_binary_file": "_is_binary_file",
    "agent_visible_path": "_agent_visible_path",
    "binary_reference_block": "_binary_reference_block",
    "file_metadata": "_file_metadata",
    "register_provider": "register_context_reference_provider",
}
ALL_FUNCS = set(PY)
TOP_DEFS = list(PY.values())

CUR = {"scripts": {}, "idx": {}, "raised": [], "trace": []}

# ── exception construction (real builtin classes => except binds by identity) ──
_UNICODE_ERRS = ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError")


class LiveConnectionError(Exception):
    pass


def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    if cls == "LiveConnectionError":
        return LiveConnectionError(msg)
    b = getattr(builtins, cls, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        if cls in _UNICODE_ERRS:
            e = b.__new__(b)
            Exception.__init__(e, msg)
            return e
        return b(msg)
    return type(cls, (Exception,), {})(msg)


def seam(name, args):
    args = norm({k: v for k, v in args.items() if v is not None})
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
    r = json.loads(json.dumps(r)) if r is not None else None
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = build_exc(r)
        CUR["raised"].append((e, r))
        raise e
    return r


# ── normalization / canonical comparison ──
def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, BaseException):
        return str(o)
    if isinstance(o, PathD):
        return norm(o._spec)
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


# ── doubles ──
class SeamFn:
    def __init__(self, name, argfn):
        self.name = name
        self.argfn = argfn

    def __call__(self, *a, **k):
        return seam(self.name, self.argfn(*a, **k))


class MimetypesDouble:
    @staticmethod
    def guess_type(name):
        return (seam("guess_type", {"name": name}), None)


class FileD:
    """Sniff file: a single read returns all scripted bytes."""

    def __init__(self, data):
        self._data = data
        self._done = False

    def read(self, n=-1):
        if self._done:
            return b""
        self._done = True
        return self._data

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class FileDList:
    """Line-count file: read() pops the next scripted chunk, then b''."""

    def __init__(self, chunks):
        self._chunks = list(chunks)
        self._i = 0

    def read(self, n=-1):
        if self._i >= len(self._chunks):
            return b""
        v = self._chunks[self._i]
        self._i += 1
        return v

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class PathD:
    def __init__(self, s, name=None, read_mode="sniff"):
        self._spec = str(s)
        self.name = name if name is not None else self._spec.rsplit("/", 1)[-1]
        self._read_mode = read_mode

    def __str__(self):
        return self._spec

    def __repr__(self):
        return self._spec

    def stat(self):
        r = seam("stat", {"path": self._spec})
        return types.SimpleNamespace(st_size=r.get("size", 0) if isinstance(r, dict) else 0)

    def open(self, mode):
        if self._read_mode == "sniff":
            data = seam("sniff", {"path": self._spec}) or []
            return FileD(bytes(int(x) for x in data))
        chunks = seam("read", {"path": self._spec}) or ""
        if isinstance(chunks, str):
            data = [chunks.encode("utf-8")]
        elif isinstance(chunks, list):
            data = [str(c).encode("utf-8") for c in chunks]
        else:
            data = []
        return FileDList(data)


@contextlib.contextmanager
def offline_file_access(path, what=None):
    seam("cm_enter", {"path": str(path), "what": what})
    yield


class ContextReferenceProvider:
    prefix = ""


class CRProvider(ContextReferenceProvider):
    def __init__(self, prefix):
        self.prefix = prefix


def install_agent_visible():
    m_tt = types.ModuleType("tools.terminal_tool")
    m_tt._ensure_terminal_env_bridged = lambda: seam("ensure_bridged", {})
    sys.modules["tools.terminal_tool"] = m_tt
    m_cf = types.ModuleType("tools.credential_files")
    m_cf.to_agent_visible_cache_path = lambda p: seam("to_cache", {"path": str(p)})
    sys.modules["tools.credential_files"] = m_cf
    parent = sys.modules.setdefault("tools", types.ModuleType("tools"))
    parent.terminal_tool = m_tt
    parent.credential_files = m_cf


# ── extraction ──
NEEDS_QUOTING_RE = re.compile(r"""[\s()\[\]{}<>"'`]""")
FILE_VALUE_RE = re.compile(
    r'^(?:(?P<quote>`|"|\')(?P<qpath>.+?)(?P=quote)|(?P<path>.+?))'
    r'(?::(?P<start>\d+)(?:-(?P<end>\d+))?)?$'
)
TEXT_EXTENSIONS = (".py", ".md", ".txt", ".json", ".yaml", ".yml", ".toml", ".js", ".ts")


def extract():
    tree = ast.parse(SRC_TEXT)
    nodes = {}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_DEFS:
            nodes[node.name] = node
    missing = [n for n in TOP_DEFS if n not in nodes]
    if missing:
        print("ORACLE context_references_flow2: FATAL AST node missing: %s" % missing,
              file=sys.stderr)
        sys.exit(2)
    for name in TOP_DEFS:
        nd = copy.copy(nodes[name])
        nd.decorator_list = []
        exec(compile(ast.Module(body=[nd], type_ignores=[]), SRC, "exec", flags=ANN_FLAG), NS)
    missing = [n for n in TOP_DEFS if n not in NS]
    if missing:
        print("ORACLE context_references_flow2: FATAL missing defs: %s" % sorted(missing),
              file=sys.stderr)
        sys.exit(2)
    for name in TOP_DEFS:
        REAL[name] = NS[name]
    return NS


NS = {"__name__": "context_references_flow2_extracted", "__file__": SRC}
REAL = {}


def install_stubs(inp, func):
    # Restore every extracted real def before installing per-case stubs: a stub
    # left from a prior case must never shadow the real function (is_binary_file
    # drives the real def, while file_metadata seames it).
    for name in TOP_DEFS:
        NS[name] = REAL[name]
    st = inp.get("state") or {}
    NS["_NEEDS_QUOTING"] = NEEDS_QUOTING_RE
    NS["TRAILING_PUNCTUATION"] = ",.;!?"
    NS["_OPENERS"] = {")": "(", "]": "[", "}": "{"}
    NS["_FILE_VALUE_PATTERN"] = FILE_VALUE_RE
    NS["mimetypes"] = MimetypesDouble()
    NS["_TEXT_EXTENSIONS"] = TEXT_EXTENSIONS
    NS["_LINE_COUNT_MAX_BYTES"] = 4 * 1024 * 1024
    NS["offline_file_access"] = offline_file_access
    NS["LiveConnectionError"] = LiveConnectionError
    if func == "file_metadata":
        NS["_is_binary_file"] = SeamFn("is_binary", lambda p: {"path": str(p)})
    NS["_on_disk_reference_block"] = SeamFn(
        "on_disk",
        lambda ref, path, descriptor, reason, guidance: {
            "raw": ref.raw, "descriptor": descriptor, "reason": reason, "guidance": guidance,
        },
    )
    NS["ContextReferenceProvider"] = ContextReferenceProvider
    NS["BUILTIN_PREFIXES"] = frozenset({"diff", "staged", "file", "folder", "git", "url"})
    NS["_context_reference_lock"] = contextlib.nullcontext()
    NS["_context_reference_providers"] = (copy.deepcopy(st.get("providers", {})) if func == "register_provider"
                                          else copy.deepcopy(inp.get("providers", {})))
    NS["_context_reference_owners"] = copy.deepcopy(st.get("owners", {}))


def do_register(inp):
    prov_map = NS["_context_reference_providers"]
    own_map = NS["_context_reference_owners"]
    prefix_raw = inp.get("prefix", "")
    owner = inp.get("owner")
    if inp.get("is_provider", True):
        provider = CRProvider(prefix_raw)
    else:
        provider = types.SimpleNamespace(prefix=prefix_raw)
    pub = NS["register_context_reference_provider"](
        provider, owner=owner, reclaim=inp.get("reclaim", False)
    )
    ret = None
    if callable(pub) and inp.get("publish", False):
        p = provider.prefix.lower().strip()
        if inp.get("publish_changed", False):
            oldp, oldo = prov_map.get(p), own_map.get(p)
            prov_map[p] = CRProvider("replacement")
            own_map[p] = "someone-else"
            ret = pub()
            prov_map[p], own_map[p] = oldp, oldo
        else:
            ret = pub()
    return ret


def drive(func, inp):
    try:
        if func == "format_reference_value":
            ret = NS["format_reference_value"](inp.get("value", ""))
        elif func == "strip_trailing_punctuation":
            ret = NS["_strip_trailing_punctuation"](inp.get("value", ""))
        elif func == "strip_reference_wrappers":
            ret = NS["_strip_reference_wrappers"](inp.get("value", ""))
        elif func == "parse_file_reference_value":
            ret = NS["_parse_file_reference_value"](inp.get("value", ""))
        elif func == "get_providers":
            ret = NS["get_context_reference_providers"]()
        elif func == "is_binary_file":
            p = PathD(inp.get("path", ""), name=inp.get("name", ""))
            ret = NS["_is_binary_file"](p)
        elif func == "agent_visible_path":
            install_agent_visible()
            ret = NS["_agent_visible_path"](str(inp.get("path", "")))
        elif func == "binary_reference_block":
            ref = types.SimpleNamespace(raw=inp.get("raw", ""))
            p = PathD(inp.get("path", ""), name=inp.get("name", ""))
            ret = NS["_binary_reference_block"](ref, p)
        elif func == "file_metadata":
            p = PathD(inp.get("path", ""), read_mode="read")
            ret = NS["_file_metadata"](p)
        elif func == "register_provider":
            ret = do_register(inp)
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:
        ret = None
        for exc, spec in CUR["raised"]:
            if e is exc:
                ret = spec
                break
        if ret is None:
            ret = {"__raise__": type(e).__name__, "msg": str(e)}
    return ret


def emit_state(func, inp):
    if func != "register_provider":
        return {}
    prov = NS["_context_reference_providers"]
    own = NS["_context_reference_owners"]

    def clean_map(m):
        out = {}
        for k, v in m.items():
            if isinstance(v, ContextReferenceProvider):
                out[k] = {}
            elif v is None:
                out[k] = None
            else:
                out[k] = norm(v)
        return out

    return {"providers": clean_map(prov), "owners": clean_map(own)}


def main():
    extract()
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
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["raised"] = []
        CUR["trace"] = []
        if func not in ALL_FUNCS:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        install_stubs(inp, func)
        ret = drive(func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"], "state": norm(emit_state(func, inp))}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE context_references_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE context_references_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
