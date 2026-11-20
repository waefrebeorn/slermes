#!/usr/bin/env python3
"""Oracle for the Matrix-adapter mention / classification / config flow port
(src/gateway/platforms/matrix_adapter_flow5.py).

Drives the LIVE plugins/platforms/matrix/adapter.py UNCLAIMED helpers with
SCRIPTED seams: every module collaborator / self-sibling / in-function import
is a recording stub fed the SAME scripts the C harness feeds its universal
seam. Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"}
in "trace") plus the final return ("ret") and the state echo ("state").

The live defs are AST-extracted from the adapter module body and the
MatrixAdapter class body (decorators excluded by lineno, segments dedented)
and called with plain Python args / a SimpleNamespace self double. The heavy
quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit/CancelledError included). Scripted raises round-trip
their spec verbatim; unscripted live raises project {__raise__, message:str(exc)}.
Summary: 'ORACLE matrix_adapter_flow5: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- Plain JSON numbers are ints; integral floats normalize to int (canon).
- Attribute carriers (config, client, identity) are {"__attr__": {...}};
  the oracle builds SimpleNamespace doubles and projects them back on seam args.
- Sets return as sorted arrays (Python set -> canon sorted list).
- logger calls are silent.
"""
import ast
import asyncio
import builtins
import copy
import inspect
import json
import os
import re
import sys
import textwrap
import types
import typing
from pathlib import Path
from contextlib import suppress
from types import SimpleNamespace
import __future__ as _fut

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "matrix", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

# name -> kind ("free" = module/static fn, "instance" = method on self double,
# "afree" / "ainstance" = coroutine variants). Used for extraction + PoP check.
KINDS = {
    "_run_media_tool": "free",
    "_apply_yaml_config": "free",
    "_is_connected": "free",
    "_recovery_key_output_path": "free",
    "_content_mentions_bot": "instance",
    "_is_bot_mentioned": "instance",
    "_has_outbound_room_mention": "instance",
    "_extract_outbound_mentions": "instance",
    "_inject_outbound_mention_links": "instance",
    "_voice_may_park": "instance",
    "_strip_mention": "instance",
    "_classify_inbound_media": "free",
    "get_chat_info": "ainstance",
    "_query_own_device_keys": "afree",
}

# EXACTLY the C ma5_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "subprocess.run",
    "_apply_yaml_bridge",
    "get_env_value",
    "_get_scoped_secret",
    "expanduser",
    "_is_bot_mentioned",
    "_user_localpart",
    "_protect_outbound_mention_regions",
    "is_voice_event",
    "_thread_root",
    "_content_mentions_bot",
    "has_voice_marker",
    "_resolve_room_identity",
    "query_keys",
]

# seams that live on the self double (accessed as self.<name>)
SELF_SEAMS = [
    "_is_bot_mentioned", "_user_localpart", "_protect_outbound_mention_regions",
    "_content_mentions_bot", "_resolve_room_identity",
]


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


class _PathDouble:
    def __init__(self, p, rec):
        self._p = str(p); self._rec = rec; self._expanded = self._p

    def expanduser(self):
        self._expanded = self._rec(self._p)
        return self

    def __str__(self):
        return self._expanded


def _PathFactory(rec):
    return lambda p: _PathDouble(p, rec)


def extract():
    mod = new_module("ma5_oracle_adapter")
    ns = mod.__dict__
    ns.update({
        "os": os, "json": json, "re": re, "suppress": suppress,
        "typing": typing, "Any": typing.Any, "Optional": typing.Optional,
        "Dict": typing.Dict, "Set": typing.Set, "List": typing.List,
        "Tuple": typing.Tuple,
        "MessageType": SimpleNamespace(
            TEXT="text", LOCATION="location", PHOTO="photo", VIDEO="video",
            AUDIO="audio", VOICE="voice", DOCUMENT="document", STICKER="sticker",
            COMMAND="command"),
    })
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # module-level Assign constants (_OUTBOUND_MENTION_RE, _MAX_CONTENT_BYTES,
    # _YAML_BRIDGE, ...) are seeded so extracted bodies can read them.
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, A_SRC)
            except Exception:
                pass
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "MatrixAdapter")
    found = set()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            n2 = copy.copy(node)
            n2.decorator_list = []
            exec_node(src, n2, ns, A_SRC)
            found.add(node.name)
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            n2 = copy.copy(node)
            n2.decorator_list = []
            exec_node(src, n2, ns, A_SRC)
            found.add(node.name)
    missing = [k for k in KINDS if k not in ns]
    if missing:
        print(f"FATAL oracle_matrix_adapter_flow5: missing: {missing}")
        sys.exit(2)
    return ns, found


NS, _FOUND = extract()


def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items()}
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, BaseException):
            return {"__raise__": type(x).__name__, "message": str(x)}
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def proj(v):
    """Project live Python doubles back to fixture JSON (attr carriers kept)."""
    if isinstance(v, types.SimpleNamespace):
        return {"__attr__": {k: proj(vv) for k, vv in vars(v).items()}}
    if isinstance(v, dict):
        return {k: proj(vv) for k, vv in v.items()}
    if isinstance(v, (list, tuple)):
        return [proj(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(proj(x) for x in v)
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def attr_double(raw):
    if not isinstance(raw, dict):
        return raw
    inner = raw.get("__attr__", raw)
    ns = SimpleNamespace()
    for k, v in inner.items():
        setattr(ns, k, v)
    return ns


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


_MISSING = object()
_CUR = None


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []

    def _record_raise(self, name, args, exc, spec=None):
        if spec is None:
            spec = proj_exc(exc)
        self.scripted.append((exc, spec))
        self.trace.append({"name": name, "args": args, "ret": spec})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                sp = used["raise"]
                spec = {"__raise__": sp.get("type", "RuntimeError"),
                        "message": sp.get("message", "")}
                self._record_raise(name, args, make_exc(sp), spec)
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret

    def spec_for(self, exc):
        for e, spec in self.scripted:
            if e is exc:
                return spec
        return None


def rec(name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


def arec(name, args_fn=None, build=None):
    async def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


def build_seams():
    st = {}
    st["subprocess.run"] = rec("subprocess.run",
        lambda cmd, capture_output=None, text=False, timeout=None, stdin=None, check=False: {
            "cmd": proj(cmd), "timeout": timeout, "text": bool(text)})
    st["_apply_yaml_bridge"] = rec("_apply_yaml_bridge",
        lambda matrix_cfg, bridge: {"matrix_cfg": proj(matrix_cfg), "bridge": "YAML_BRIDGE"})
    st["get_env_value"] = rec("get_env_value", lambda name: {"name": name})
    st["_get_scoped_secret"] = rec("_get_scoped_secret",
        lambda name, default="": {"name": name, "default": default})
    st["expanduser"] = rec("expanduser", lambda path: {"path": path})
    st["_is_bot_mentioned"] = rec("_is_bot_mentioned",
        lambda body, formatted_body=None, user_ids=None: {
            "body": body, "formatted_body": proj(formatted_body), "user_ids": proj(user_ids)})
    st["_user_localpart"] = rec("_user_localpart", lambda: {})
    st["_protect_outbound_mention_regions"] = rec(
        "_protect_outbound_mention_regions", lambda text: {"text": text},
        build=lambda raw: (raw[0], raw[1]) if isinstance(raw, list) and len(raw) >= 2 else ("", []))
    st["is_voice_event"] = rec("is_voice_event", lambda content: {"content": proj(content)})
    st["_thread_root"] = rec("_thread_root", lambda relates_to: {"relates_to": proj(relates_to)})
    st["_content_mentions_bot"] = rec("_content_mentions_bot",
        lambda body, content: {"body": body, "content": proj(content)})
    st["has_voice_marker"] = rec("has_voice_marker", lambda content: {"content": proj(content)})
    st["_resolve_room_identity"] = arec("_resolve_room_identity",
        lambda room_id: {"room_id": room_id}, build=attr_double)
    st["query_keys"] = arec("query_keys",
        lambda payload: {"payload": proj(payload)}, build=attr_double)
    return st


def install_module_seams(stubs):
    for n in ("_apply_yaml_bridge", "_get_scoped_secret", "is_voice_event",
              "_thread_root", "has_voice_marker"):
        NS[n] = stubs[n]
    NS["subprocess"] = SimpleNamespace(run=stubs["subprocess.run"], DEVNULL=-3)
    NS["Path"] = _PathFactory(stubs["expanduser"])
    if "hermes_cli" not in sys.modules:
        parent = types.ModuleType("hermes_cli")
        parent.__path__ = []
        sys.modules["hermes_cli"] = parent
    gm = types.ModuleType("hermes_cli.gateway")
    gm.get_env_value = stubs["get_env_value"]
    sys.modules["hermes_cli.gateway"] = gm


def make_self(state, stubs):
    attrs = {n: staticmethod(stubs[n]) for n in SELF_SEAMS}
    D = type("MatrixAdapter", (), attrs)
    inst = D()
    for k, v in (state or {}).items():
        setattr(inst, k, v)
    return inst


def make_client(spec, stubs):
    spec = spec or {}
    inner = spec.get("__attr__", spec) if isinstance(spec, dict) else {}
    D = type("MatrixClient", (), {"query_keys": staticmethod(stubs["query_keys"])})
    c = D()
    for k, v in inner.items():
        if k != "query_keys":
            setattr(c, k, v)
    return c


def cfg_double(spec):
    if not isinstance(spec, dict):
        return spec
    inner = spec.get("__attr__", spec)
    return SimpleNamespace(**inner)


def maybe_run(x):
    if inspect.isawaitable(x):
        return asyncio.run(x)
    return x


def drive(func, inp):
    global _CUR
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    stubs = build_seams()
    install_module_seams(stubs)
    inst = make_self(state, stubs)
    client = make_client(args.get("client"), stubs)
    g = args.get
    try:
        if func == "_run_media_tool":
            ret = NS[func](g("cmd"), timeout=g("timeout"), text=g("text", False))
        elif func == "_apply_yaml_config":
            ret = NS[func](g("yaml_cfg"), g("matrix_cfg"))
        elif func == "_is_connected":
            ret = NS[func](cfg_double(g("config")))
        elif func == "_recovery_key_output_path":
            ret = NS[func]()
        elif func == "_content_mentions_bot":
            ret = NS[func](inst, g("body"), g("content"))
        elif func == "_is_bot_mentioned":
            ret = NS[func](inst, g("body"), g("formatted_body"), g("user_ids"))
        elif func == "_has_outbound_room_mention":
            ret = NS[func](inst, g("text"))
        elif func == "_extract_outbound_mentions":
            ret = NS[func](inst, g("text"))
        elif func == "_inject_outbound_mention_links":
            ret = NS[func](inst, g("text"))
        elif func == "_voice_may_park":
            ret = NS[func](inst, g("room_id"), g("body"), g("content"), g("relates_to"),
                           g("mention_claimed", False))
        elif func == "_strip_mention":
            ret = NS[func](inst, g("body"))
        elif func == "_classify_inbound_media":
            ret = NS[func](g("msgtype"), g("event_mimetype"), g("source_content"))
        elif func == "get_chat_info":
            ret = maybe_run(NS[func](inst, g("chat_id")))
        elif func == "_query_own_device_keys":
            ret = maybe_run(NS[func](client))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        spec = ctx.spec_for(exc)
        ret = spec if spec is not None else proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(vars(inst))}


# ── seam-name set cross-check (C / oracle / fixtures / PoP) ──
def _c_seams():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "matrix_adapter_flow5.c").read_text(encoding="utf-8")
    return set(re.findall(r'ma5_call\s*\(\s*ctx\s*,\s*"([^"]+)"', csrc))


def cross_check():
    registry = set(SEAM_NAMES)
    cseams = _c_seams()
    if cseams != registry:
        print(f"FATAL oracle_matrix_adapter_flow5: seam set drift C vs registry: "
              f"extra={sorted(cseams - registry)} missing={sorted(registry - cseams)}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'\brec\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'\barec\(\s*"([^"]+)"', osrc))
    if used != registry:
        print(f"FATAL oracle_matrix_adapter_flow5: seam set drift oracle vs registry: "
              f"extra={sorted(used - registry)} missing={sorted(registry - used)}")
        sys.exit(2)
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "matrix_adapter_flow5.c").read_text(encoding="utf-8")
    pops = set(re.findall(r'PoP:\s*ma5_\w+\s*@\s*plugins/platforms/matrix/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_matrix_adapter_flow5: PoP/KINDS drift: c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow5.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in fix.open(encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func", ""))
        if fix_keys != registry:
            print(f"FATAL oracle_matrix_adapter_flow5: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(registry - fix_keys)} extra={sorted(fix_keys - registry)}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_matrix_adapter_flow5: fixture func set != KINDS: "
                  f"missing={sorted(set(KINDS) - fix_funcs)} extra={sorted(fix_funcs - set(KINDS))}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 60:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE matrix_adapter_flow5: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow5.jsonl"
    n = bad = 0
    counts = {}
    for line in fix.open(encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        if not case.get("fc") and isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in list(KINDS) + SELF_SEAMS:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST BINDING {func} case {n}: {msg}")
    print(f"SELFTEST matrix_adapter_flow5: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    low = [f for f, c in counts.items() if c < 6]
    if low:
        print("WARNING funcs with <6 cases:", low)
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
