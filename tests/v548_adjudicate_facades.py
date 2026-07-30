#!/usr/bin/env python3
"""Definitive per-function adjudication of the v548 facade-shaped bucket.

Uses the calibrated classifier (strict SDK rule) PLUS a few hard rules derived
from manual reading:
  - Python returns a CONSTANT literal/{}[]/None -> HONEST (const truthful).
  - Python returns/imports a third-party SDK object or class (boto3, anthropic,
    openai, fal, piper, kittentts, sounddevice, edge_tts, mcp, ...) -> HONEST
    (no C repr; NULL/0 truthful). This is v547's "honest-limitation facade".
  - Python is a no-op / abstractmethod / bare raise -> HONEST (NA).
  - Python does LOCAL computable work (string parse, dict get, numeric floor,
    conjunct logic, config read via hermes_cli.config) and C hardcoded a const
    -> FACADE_FRAUD (C skipped real work).
Writes tests/.v548_facade_verdicts.json with one record per function and a
machine-checkable 'verdict' field: HONEST_KEEP | FACADE_FRAUD | NEEDS_MANUAL.
"""
import ast, json, re
from pathlib import Path

HERMES = Path("/home/wubu/hermes-agent-dev")
rows = json.load(open(HERMES / "slermes/tests/.v548_facade_full.json"))

SDK_MODULES = (
    "boto3","botocore","anthropic","fal_client","fal","openai","edge_tts","piper",
    "sounddevice","kittentts","mistral","elevenlabs","mcp","playwright","pydub",
    "yt_dlp","duckduckgo","ddgs","whisper","torch","transformers","requests",
    "httpx","aiohttp","urllib3","selenium","browser_use","firecrawl","claude_code",
    "google","azure","cv2",
)

def body_returns_const(node):
    real=[s for s in node.body if not (isinstance(s,ast.Expr) and isinstance(getattr(s,"value",None),ast.Constant))]
    if len(real)==1 and isinstance(real[0],ast.Return) and real[0].value is not None:
        v=real[0].value
        if isinstance(v,ast.Constant): return True
        if isinstance(v,(ast.List,ast.Dict,ast.Set,ast.Tuple)):
            def lit(n):
                if isinstance(n,ast.Constant): return True
                if isinstance(n,(ast.List,ast.Tuple,ast.Set)): return all(lit(e) for e in n.elts)
                if isinstance(n,ast.Dict): return all((k is None or lit(k)) for k in n.keys) and all(lit(x) for x in n.values)
                return False
            return lit(v)
    return False

def imports_sdk(node):
    for imp in ast.walk(node):
        if isinstance(imp,ast.Import):
            if any(m.name.split(".")[0] in SDK_MODULES for m in imp.names): return True
        elif isinstance(imp,ast.ImportFrom) and imp.module:
            if imp.module.split(".")[0] in SDK_MODULES: return True
    return False

def returns_sdk_object(node):
    """Python returns an imported SDK class/object (or raises ImportError)."""
    real=[s for s in node.body if not (isinstance(s,ast.Expr) and isinstance(getattr(s,"value",None),ast.Constant))]
    for s in real:
        if isinstance(s,ast.Return) and s.value is not None:
            rv=s.value
            # bare raise inside try/except ImportError -> SDK getter
            if isinstance(rv,ast.Name):
                # is it an SDK alias? check imports
                for imp in ast.walk(node):
                    if isinstance(imp,ast.Import):
                        for m in imp.names:
                            if (m.asname or m.name.split(".")[0])==rv.id and m.name.split(".")[0] in SDK_MODULES:
                                return True
                    elif isinstance(imp,ast.ImportFrom) and imp.module:
                        if imp.module.split(".")[0] in SDK_MODULES:
                            for m in imp.names:
                                if (m.asname or m.name)==rv.id: return True
    return False

def is_noop_or_abstract(node):
    """Strict: honest only if body is genuinely empty of real work:
       - no statements except docstring (pass), OR
       - single 'raise NotImplementedError/NotImplemented' (abstract NA), OR
       - @abc.abstractmethod present.
       A raise buried among real logic is NOT honesty."""
    real=[s for s in node.body if not (isinstance(s,ast.Expr) and isinstance(getattr(s,"value",None),ast.Constant))]
    if not real:
        return True
    # abstractmethod decorator
    for dec in getattr(node,"decorator_list",[]):
        if isinstance(dec,ast.Name) and dec.id=="abstractmethod":
            return True
        if isinstance(dec,ast.Attribute) and dec.attr=="abstractmethod":
            return True
    # exactly one statement that is a bare raise of NotImplementedError
    if len(real)==1 and isinstance(real[0],ast.Raise):
        exc=real[0].exc
        if isinstance(exc,ast.Call):
            fn=ast.unparse(exc.func) if hasattr(exc,"func") else ""
            if "NotImplemented" in fn: return True
        elif isinstance(exc,ast.Name) and "NotImplemented" in exc.id:
            return True
    return False

def adjudicate(r):
    py=r["pybody"]
    if py in ("[FILE ABSENT]","[METHOD NOT LOCATED]","[ERR ]"):
        return "NEEDS_MANUAL", "python source unlocatable from this host"
    try:
        node=ast.parse(py)
    except Exception as e:
        return "NEEDS_MANUAL", f"parse error {e}"
    fn=node.body[0]
    # rule order mirrors edict: is the C const return truthful?
    if is_noop_or_abstract(fn):
        return "HONEST_KEEP", "Python is no-op/abstract/raises -> C const/NA truthful"
    if body_returns_const(fn):
        return "HONEST_KEEP", "Python itself returns a constant -> C const truthful"
    if returns_sdk_object(fn) or (imports_sdk(fn) and is_sdk_guard(fn)):
        return "HONEST_KEEP", "Python returns/imports third-party SDK object (no C repr) -> NULL/0 truthful"
    # otherwise: Python does real work C skipped -> fraud
    return "FACADE_FRAUD", "Python does real local work; C hardcoded const skips it"

def is_sdk_guard(node):
    """SDK import present AND a return of None/const (absent-at-runtime guard)."""
    real=[s for s in node.body if not (isinstance(s,ast.Expr) and isinstance(getattr(s,"value",None),ast.Constant))]
    for s in real:
        if isinstance(s,ast.Return) and s.value is not None and isinstance(s.value,ast.Constant):
            if s.value.value in (None,0,False,True,""):
                return True
    return False

out=[]
counts={"HONEST_KEEP":0,"FACADE_FRAUD":0,"NEEDS_MANUAL":0}
for r in rows:
    v,ev=adjudicate(r)
    counts[v]+=1
    out.append({"cname":r["cname"],"py":r["py"],"creturn":r["creturn"],
                "heuristic_was":r["heur"],"verdict":v,"evidence":ev})

json.dump(out, open(HERMES/"slermes/tests/.v548_facade_verdicts.json","w"), indent=1)
print("Facade adjudication (definitive):", counts)
print("\nHONEST_KEEP:")
for r in out:
    if r["verdict"]=="HONEST_KEEP": print(f"  {r['cname']}  ({r['py']})  C={r['creturn']!r}  [{r['evidence']}]")
print("\nNEEDS_MANUAL:")
for r in out:
    if r["verdict"]=="NEEDS_MANUAL": print(f"  {r['cname']}  ({r['py']})")
print(f"\nFACADE_FRAUD count: {counts['FACADE_FRAUD']} (full list in .v548_facade_verdicts.json)")
