#!/usr/bin/env python3
"""Unified v548 adjudication across ALL candidate shapes (facade / no-return / thin).

Strict, calibrated rules (see v547 edict + empirical calibration):
  FACADE_SHAPE (C returns const):
    HONEST_KEEP  python is no-op/abstract/NotImplemented, OR python itself returns
                 a constant, OR python returns/imports a third-party SDK object
                 (no C repr -> NULL/0 truthful).
    FACADE_FRAUD python does real local work but C hardcoded a const (skipped it).
  NORET_SHAPE (C void, no real stmt):
    HONEST_KEEP  python is a no-op / pure-log-only with no side effects.
    NORET_DORMANT python does real work (IO/assign/loop) but C is a no-op.
  THIN_SHAPE (C returns an expr):
    HONEST_KEEP  thin wrapper returning an SDK object/const.
    THIN_HONEST  thin delegation with no SDK/IO (faithful).
    THIN_REVIEW  python does real work; verify C wrapper actually covers it.
Writes tests/.v548_all_verdicts.json. This is a DECISION AID; per the edict every
FACADE_FRAUD / NORET_DORMANT / THIN_REVIEW must still be human-confirmed before
any code change. Does NOT modify any .c file.
"""
import ast, json, re
from pathlib import Path
from collections import Counter

HERMES = Path("/home/wubu/hermes-agent-dev")
d = json.load(open(HERMES / "slermes/tests/.v548_adjudicated.json"))

SDK_MODULES = (
    "boto3","botocore","anthropic","fal_client","fal","openai","edge_tts","piper",
    "sounddevice","kittentts","mistral","elevenlabs","mcp","playwright","pydub",
    "yt_dlp","duckduckgo","ddgs","whisper","torch","transformers","requests",
    "httpx","aiohttp","urllib3","selenium","browser_use","firecrawl","claude_code",
    "google","azure","cv2",
)

def resolve_py(s):
    p = HERMES / s
    if p.exists(): return p
    hits = list(HERMES.rglob(s.split("/")[-1]))
    return hits[0] if hits else None

def locate(tree, pyfunc):
    if "." in pyfunc:
        cls, meth = pyfunc.split(".", 1)
        for nd in ast.walk(tree):
            if isinstance(nd, ast.ClassDef) and nd.name == cls:
                for b in nd.body:
                    if isinstance(b, (ast.FunctionDef, ast.AsyncFunctionDef)) and b.name == meth:
                        return b
    for nd in tree.body:
        if isinstance(nd, (ast.FunctionDef, ast.AsyncFunctionDef)) and nd.name == pyfunc:
            return nd
    for nd in ast.walk(tree):
        if isinstance(nd, (ast.FunctionDef, ast.AsyncFunctionDef)) and nd.name == pyfunc:
            return nd
    return None

def imports_sdk(node):
    for imp in ast.walk(node):
        if isinstance(imp, ast.Import):
            if any(m.name.split(".")[0] in SDK_MODULES for m in imp.names): return True
        elif isinstance(imp, ast.ImportFrom) and imp.module:
            if imp.module.split(".")[0] in SDK_MODULES: return True
    return False

def returns_sdk_object(node):
    real = [s for s in node.body if not (isinstance(s, ast.Expr) and isinstance(getattr(s, "value", None), ast.Constant))]
    for s in real:
        if isinstance(s, ast.Return) and s.value is not None and isinstance(s.value, ast.Name):
            rv = s.value.id
            for imp in ast.walk(node):
                if isinstance(imp, ast.Import):
                    for m in imp.names:
                        if (m.asname or m.name.split(".")[0]) == rv and m.name.split(".")[0] in SDK_MODULES: return True
                elif isinstance(imp, ast.ImportFrom) and imp.module:
                    if imp.module.split(".")[0] in SDK_MODULES:
                        for m in imp.names:
                            if (m.asname or m.name) == rv: return True
    return False

def body_returns_const(node):
    real = [s for s in node.body if not (isinstance(s, ast.Expr) and isinstance(getattr(s, "value", None), ast.Constant))]
    if len(real) == 1 and isinstance(real[0], ast.Return) and real[0].value is not None:
        v = real[0].value
        if isinstance(v, ast.Constant): return True
        if isinstance(v, (ast.List, ast.Dict, ast.Set, ast.Tuple)):
            def lit(n):
                if isinstance(n, ast.Constant): return True
                if isinstance(n, (ast.List, ast.Tuple, ast.Set)): return all(lit(e) for e in n.elts)
                if isinstance(n, ast.Dict): return all((k is None or lit(k)) for k in n.keys) and all(lit(x) for x in n.values)
                return False
            return lit(v)
    return False

def is_abstract_noop(node):
    real = [s for s in node.body if not (isinstance(s, ast.Expr) and isinstance(getattr(s, "value", None), ast.Constant))]
    if not real: return True
    for dec in getattr(node, "decorator_list", []):
        if (isinstance(dec, ast.Name) and dec.id == "abstractmethod") or (isinstance(dec, ast.Attribute) and dec.attr == "abstractmethod"):
            return True
    if len(real) == 1 and isinstance(real[0], ast.Raise):
        exc = real[0].exc
        if isinstance(exc, ast.Call) and "NotImplemented" in ast.unparse(exc.func): return True
        if isinstance(exc, ast.Name) and "NotImplemented" in exc.id: return True
    return False

def py_does_real_work(node):
    calls = [n for n in ast.walk(node) if isinstance(n, ast.Call)]
    io = any(re.search(r"\b(requests|httpx|urllib|aiohttp|subprocess|os\.(system|popen|environ)|open|socket|"
                       r"client|browser|\.get|\.post|fetch|github|api|sdk|boto|mcp|playwright|click|"
                       r"read|write|load|download|save|run|spawn|connect)\b",
                       ast.unparse(n.func) if hasattr(n, "func") else "", re.I) for n in calls)
    loop = any(isinstance(n, (ast.For, ast.While, ast.AsyncFor, ast.ListComp, ast.DictComp, ast.SetComp)) for n in ast.walk(node))
    assign = any(isinstance(n, (ast.Assign, ast.AugAssign)) for n in ast.walk(node))
    return io or loop or assign

verdicts = []
for c in d:
    if "v547 retained" in c["verdict"]:
        verdicts.append({**c, "final": "HONEST_KEEP", "evidence": "v547 per-function adjudicated honest-limitation facade"})
        continue
    if c["shape"] not in ("FACADE_SHAPE", "NORET_SHAPE", "THIN_SHAPE"):
        continue
    p = resolve_py(c["py"].split(":")[0])
    if not p:
        verdicts.append({**c, "final": "NEEDS_MANUAL", "evidence": "python file absent from host"}); continue
    try:
        tree = ast.parse(p.read_text()); node = locate(tree, c["py"].split(":")[1])
    except Exception as e:
        verdicts.append({**c, "final": "NEEDS_MANUAL", "evidence": f"parse/locate error {e}"}); continue
    if node is None:
        verdicts.append({**c, "final": "NEEDS_MANUAL", "evidence": "method not located in file"}); continue

    if c["shape"] == "FACADE_SHAPE":
        if is_abstract_noop(node):
            f, ev = "HONEST_KEEP", "Python no-op/abstract/NotImplemented -> C const/NA truthful"
        elif body_returns_const(node):
            f, ev = "HONEST_KEEP", "Python itself returns a constant -> C const truthful"
        elif returns_sdk_object(node) or (imports_sdk(node) and any(isinstance(s, ast.Return) and isinstance(s.value, ast.Constant) and s.value.value in (None, 0, False, True, "") for s in node.body if isinstance(s, ast.Return))):
            f, ev = "HONEST_KEEP", "Python returns/imports third-party SDK (no C repr) -> NULL/0 truthful"
        else:
            f, ev = "FACADE_FRAUD", "Python does real work; C hardcoded const skips it"
    elif c["shape"] == "NORET_SHAPE":
        if is_abstract_noop(node) or (not py_does_real_work(node) and not any(isinstance(s, (ast.Assign, ast.AugAssign, ast.For, ast.While)) for s in ast.walk(node))):
            f, ev = "HONEST_KEEP", "Python no-op/pure-log only -> C no-op truthful"
        else:
            f, ev = "NORET_DORMANT", "Python does real work; C is a no-op"
    else:  # THIN_SHAPE
        if returns_sdk_object(node) or (imports_sdk(node) and body_returns_const(node)):
            f, ev = "HONEST_KEEP", "thin wrapper returning SDK object/const -> truthful"
        elif py_does_real_work(node) and not imports_sdk(node):
            f, ev = "THIN_REVIEW", "Python does real work; verify C wrapper covers it"
        else:
            f, ev = "THIN_HONEST", "thin delegation, no SDK/IO -> honest"

    verdicts.append({**c, "final": f, "evidence": ev})

cnt = Counter(v["final"] for v in verdicts)
json.dump(verdicts, open(HERMES / "slermes/tests/.v548_all_verdicts.json", "w"), indent=1)
print("UNIFIED VERDICTS:", dict(cnt))
for k in ("HONEST_KEEP", "FACADE_FRAUD", "NORET_DORMANT", "THIN_HONEST", "THIN_REVIEW", "NEEDS_MANUAL"):
    print(f"  {k:14s}: {cnt.get(k,0)}")
