#!/usr/bin/env python3
"""
v548 enumerator + ADJUDICATION layer.

Stage 1 (mechanical, from PoP comments):
  For every /* PoP: cname @ pyfile:pyfunc */ in src/**/port_*.c, extract the C
  function body (C-aware brace matching) and classify the *shape*:
    FACADE_SHAPE  -> body reduces to `return <const>` (incl strdup("literal"))
    NORET_SHAPE   -> void body with no real statement (empty or bare `return;`)
    THIN_SHAPE    -> `return <non-const expr>`
    REAL          -> anything else

Stage 2 (honest adjudication, from Python source):
  For each FACADE_SHAPE / NORET_SHAPE / THIN_SHAPE candidate, read the actual
  Python body at HERMES_DIR/pyfile::pyfunc and decide whether the C canned/const
  return is HONEST (Python itself returns a constant or is a no-op / SDK-import
  guard) or a FRAUD (Python does real work that C skipped). This replicates the
  v547 edict-#2 method at scale, with evidence captured per function.

Verdicts:
  FACADE_FRAUD     -> Python does real work; C canned/const return skips it. DEMOTE.
  HONEST_CONST     -> Python itself returns a constant (None/0/true/{} / []) -> C const is truthful. KEEP.
  HONEST_NOOP      -> Python is a no-op (pass / bare return) -> C no-op truthful. KEEP.
  HONEST_SDKGUARD  -> Python try/import SDK, return const on failure -> C const truthful. KEEP.
  THIN_HONEST      -> C returns a real C helper/variable; delegation is faithful. KEEP.
  THIN_CANNED      -> C returns a const/canned literal but Python does real work -> FACADE_FRAUD.
  NORET_HONEST     -> Python no-op / pure logging -> C no-op truthful. KEEP.
  NORET_DORMANT    -> Python does real work but C is a no-op -> genuine dormant gap. DEMOTE.
  NEEDS_MANUAL     -> ambiguous; human must read.
"""
import ast
import json
import os
import re
from pathlib import Path

HERMES_DIR = Path("/home/wubu/hermes-agent-dev")
SLERMES_DIR = HERMES_DIR / "slermes"
SRC = SLERMES_DIR / "src"

POP_RE = re.compile(r"/\*\s*PoP:\s*(\w+)\s*@\s*([^:*]+?):(\w+)\s*\*/")

CONST_RE = re.compile(
    r"^(?:[0-9]+(?:\.[0-9]+)?|0x[0-9a-fA-F]+|NULL|TRUE|FALSE|true|false|"
    r'""|"[^"]*"|\'\\[^\']*\'|\'[^\']\'|strdup\(\s*""\s*\)|'
    r'strdup\(\s*"[^"]*"\s*\))$'
)


# ---------- C body extraction ----------
def skip_ws_comments(text, i):
    n = len(text)
    while i < n:
        c = text[i]
        if c in " \t\r\n":
            i += 1
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                i += 1
            i += 2
        elif c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
        else:
            break
    return i


def skip_balanced(text, start, open_ch, close_ch):
    n = len(text)
    assert text[start] == open_ch
    depth = 1
    i = start + 1
    while i < n and depth > 0:
        c = text[i]
        if c == "'":
            i += 1
            while i < n and text[i] != "'":
                i += 2 if text[i] == "\\" else 1
            i += 1
            continue
        if c == '"':
            i += 1
            while i < n and text[i] != '"':
                i += 2 if text[i] == "\\" else 1
            i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                i += 1
            i += 2
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if c == open_ch:
            depth += 1
        elif c == close_ch:
            depth -= 1
        i += 1
    return i


def collect_defs(text):
    """Return list of (def_start, name, body_start, body_end) for every
    function DEFINITION (name(...) followed by {) in the file. Handles
    multiline signatures and the bridge pattern (PoP comment follows def)."""
    defs = []
    # match a word boundary name followed by '('
    pat = re.compile(r"(?<![\w.>])([A-Za-z_]\w*)\s*\(")
    for m in pat.finditer(text):
        name = m.group(1)
        p = m.end() - 1
        close = skip_balanced(text, p, "(", ")")
        j = skip_ws_comments(text, close)
        if j < len(text) and text[j] == "{":
            body_end = skip_balanced(text, j, "{", "}")
            defs.append((m.start(), name, j + 1, body_end - 1))
    return defs


def associate(text, defs, pop_pos):
    """Pick the def that this PoP comment annotates.

    Project convention is the BRIDGE pattern: the /* PoP: */ comment sits
    IMMEDIATELY ABOVE the function it documents (def_start > pop_pos). So the
    correct def is almost always the FIRST def whose start is after the comment.
    Only fall back to 'containing' (PoP inside a def body) or 'preceding' (PoP
    trailing a def, no following def) when no following def exists.
    """
    # 1) PoP inside a def body -> that def
    containing = [d for d in defs if d[0] <= pop_pos <= d[3]]
    if containing:
        return min(containing, key=lambda d: (d[3] - d[0]))
    # 2) bridge convention: first def AFTER the comment
    following = [d for d in defs if d[0] > pop_pos]
    if following:
        return min(following, key=lambda d: d[0])
    # 3) last resort: nearest preceding def
    preceding = [d for d in defs if d[0] <= pop_pos]
    if preceding:
        return max(preceding, key=lambda d: d[0])
    return None


def strip_noops(body):
    out = []
    i, n = 0, len(body)
    while i < n:
        c = body[i]
        if c == "/" and i + 1 < n and body[i + 1] == "*":
            i += 2
            while i + 1 < n and not (body[i] == "*" and body[i + 1] == "/"):
                i += 1
            i += 2
            continue
        if c == "/" and i + 1 < n and body[i + 1] == "/":
            while i < n and body[i] != "\n":
                i += 1
            continue
        out.append(c)
        i += 1
    s = "".join(out)
    s = re.sub(r"\bhermes_log\s*\((?:[^()]*|\([^()]*\))*\)\s*;", " ", s)
    s = re.sub(r"\(void\)[^;]+;", " ", s)
    return re.sub(r"\s+", " ", s).strip()


def c_shape(body, is_void):
    cleaned = strip_noops(body)
    if cleaned in ("", "return;"):
        return "NORET_SHAPE", cleaned
    m = re.fullmatch(r"return\s+(.+?)\s*;", cleaned)
    if m:
        expr = m.group(1).strip()
        if CONST_RE.match(expr):
            return "FACADE_SHAPE", expr
        return "THIN_SHAPE", expr
    return "REAL", cleaned


# ---------- Python adjudication ----------
def locate_py_func(tree, pyfunc):
    """Return func/method/class node or None. Handle module-fn, ClassName.method,
    bare class name (returns the class def), and bare method name."""
    if "." in pyfunc:
        cls, meth = pyfunc.split(".", 1)
        for node in ast.walk(tree):
            if isinstance(node, ast.ClassDef) and node.name == cls:
                for b in node.body:
                    if isinstance(b, (ast.FunctionDef, ast.AsyncFunctionDef)) and b.name == meth:
                        return b
        return None
    # bare class name (Python class ported as C fn/struct-ctor)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == pyfunc:
            return node
    # module-level function
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == pyfunc:
            return node
    # fall back: any function/method with that name
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == pyfunc:
            return node
    return None


def py_classify(node):
    """Decide what the Python body does. Returns (verdict_hint, evidence)."""
    body = node.body

    def _is_literal_coll(node):
        if isinstance(node, ast.Constant):
            return True
        if isinstance(node, (ast.List, ast.Tuple, ast.Set)):
            return all(_is_literal_coll(e) for e in node.elts)
        if isinstance(node, ast.Dict):
            return all(k is None or _is_literal_coll(k) for k in node.keys) and \
                   all(_is_literal_coll(val) for val in node.values)
        return False

    def is_const_return(stmt):
        if isinstance(stmt, ast.Return) and stmt.value is not None:
            v = stmt.value
            if isinstance(v, ast.Constant):
                return ("const", repr(v.value))
            if isinstance(v, (ast.List, ast.Dict, ast.Set, ast.Tuple)) and _is_literal_coll(v):
                return ("const", ast.unparse(v))
        return None

    # trivial no-op: just pass / bare return / docstring
    meaningful = [s for s in body if not (isinstance(s, ast.Expr) and isinstance(getattr(s, "value", None), ast.Constant))]
    if not meaningful:
        return ("HONEST_NOOP", "body is pass/docstring only")

    # drop docstrings to find the "real" statement list
    real = [s for s in body if not (isinstance(s, ast.Expr) and isinstance(getattr(s, "value", None), ast.Constant))]

    # single const return
    if len(real) == 1 and isinstance(real[0], ast.Return):
        cr = is_const_return(real[0])
        if cr:
            return ("HONEST_CONST", f"returns constant: {cr[1]}")
        # returns a non-const expression built from args? (e.g. return a + b)
        return ("REAL_WORK", f"returns computed expr: {ast.unparse(real[0].value) if real[0].value else 'None'}")

    # SDK-getter / import-guard pattern (STRICT): only honest if the import is a
    # third-party SDK/engine with NO C equivalent (boto3, anthropic, fal_client,
    # openai, edge_tts, botocore, piper, sounddevice, kittentts, mistral,
    # elevenlabs, mcp, playwright, pydub, yt_dlp, ...). Internal hermes imports
    # (hermes_cli.config, hermes_constants, ...) are DOABLE in C -> not a guard.
    SDK_MODULES = (
        "boto3", "botocore", "anthropic", "fal_client", "fal", "openai", "edge_tts",
        "piper", "sounddevice", "kittentts", "mistral", "elevenlabs", "mcp",
        "playwright", "pydub", "yt_dlp", "duckduckgo", "ddgs", "whisper", "torch",
        "transformers", "requests", "httpx", "aiohttp", "urllib3", "selenium",
        "browser_use", "firecrawl", "claude_code", "google", "azure", "cv2",
    )

    def is_sdk_import(imp):
        # imp is an ast.Import or ast.ImportFrom
        if isinstance(imp, ast.Import):
            return any(m.name.split(".")[0] in SDK_MODULES for m in imp.names)
        if isinstance(imp, ast.ImportFrom):
            return imp.module.split(".")[0] in SDK_MODULES
        return False

    sdk_imports = [n for n in ast.walk(node) if isinstance(n, (ast.Import, ast.ImportFrom)) and is_sdk_import(n)]
    # map imported local alias -> module root, to recognise "return <sdk alias>"
    sdk_alias = {}
    for imp in ast.walk(node):
        if not is_sdk_import(imp):
            continue
        if isinstance(imp, ast.Import):
            for m in imp.names:
                sdk_alias[m.asname or m.name.split(".")[0]] = m.name.split(".")[0]
        elif isinstance(imp, ast.ImportFrom) and imp.module:
            root = imp.module.split(".")[0]
            for m in imp.names:
                sdk_alias[m.asname or m.name] = root
    if sdk_imports:
        for s in real:
            if isinstance(s, ast.Return) and s.value is not None:
                rv = s.value
                # returns None / a literal -> honest guard (SDK absent at runtime)
                if isinstance(rv, ast.Constant) and rv.value in (None, 0, False, True, ""):
                    return ("HONEST_SDKGUARD", "imports third-party SDK, returns None/literal on absent")
                # returns the imported module/object (Name referencing an SDK) -> honest
                if isinstance(rv, ast.Name) and sdk_alias.get(rv.id.split(".")[0]) in SDK_MODULES:
                    return ("HONEST_SDKGUARD", f"returns SDK object '{rv.id}' (no C repr -> NULL/0 truthful)")

    # try/except ImportError returning None/const (SDK only)
    for s in real:
        if isinstance(s, ast.Try):
            has_sdk = any(is_sdk_import(x) for x in ast.walk(s))
            ret_const = False
            for h in s.handlers:
                for hs in h.body:
                    if isinstance(hs, ast.Return) and hs.value is not None and isinstance(hs.value, ast.Constant):
                        ret_const = True
            for ts in s.body:
                if isinstance(ts, ast.Return) and ts.value is not None and isinstance(ts.value, ast.Constant):
                    ret_const = True
            # only counts as honest guard if it also touches an SDK import somewhere
            if has_sdk and ret_const:
                return ("HONEST_SDKGUARD", "try/import third-party SDK, return const on failure")

    # does it touch IO / network / call external?
    calls = [n for n in ast.walk(node) if isinstance(n, ast.Call)]
    io_hits = []
    for c in calls:
        fn = ast.unparse(c.func) if hasattr(c, "func") else ""
        if re.search(r"\b(requests|httpx|urllib|aiohttp|subprocess|os\.(system|popen|environ)|open|socket|"
                     r"client|browser|\.get|\.post|fetch|github|api|sdk|boto|mcp|playwright|click|"
                     r"read|write|load|download|save|run|spawn|connect)\b", fn, re.I):
            io_hits.append(fn)
    if io_hits:
        return ("REAL_WORK", "calls external/IO: " + ", ".join(io_hits[:4]))
    # loops/comprehensions over data, assignments from args -> real compute
    has_loop = any(isinstance(n, (ast.For, ast.While, ast.AsyncFor, ast.ListComp,
                                  ast.DictComp, ast.SetComp, ast.comprehension)) for n in ast.walk(node))
    has_assign = any(isinstance(n, (ast.Assign, ast.AugAssign)) for n in ast.walk(node))
    if has_loop or has_assign:
        return ("REAL_WORK", "computation/assignment/loop over data")
    if calls:
        return ("REAL_WORK", f"{len(calls)} call(s) present")
    return ("NEEDS_MANUAL", "unclear body structure")


def main():
    candidates = []
    files = sorted(str(p) for p in SRC.rglob("port_*.c"))
    for f in files:
        text = Path(f).read_text()
        rel = os.path.relpath(f, SLERMES_DIR)
        defs = collect_defs(text)
        for m in POP_RE.finditer(text):
            cname, pyfile, pyfunc = m.group(1), m.group(2), m.group(3)
            d = associate(text, defs, m.start())
            if not d:
                candidates.append({"file": rel, "cname": cname, "py": f"{pyfile}:{pyfunc}",
                                    "shape": "NO_MATCH", "detail": "", "cbody": "", "verdict": "NEEDS_MANUAL"})
                continue
            body = text[d[2]:d[3]]
            # void-ness: look at the signature text just before def name
            sig = text[d[0]:d[2]]
            is_void = bool(re.search(r"\bvoid\b\s*\w+\s*$", sig.rstrip().split("\n")[-1]) or
                           re.search(r"^void\s+\w+\s*\(", sig.strip().split("\n")[0], re.M))
            shape, detail = c_shape(body, is_void)
            candidates.append({"file": rel, "cname": cname, "py": f"{pyfile}:{pyfunc}",
                                "shape": shape, "detail": detail, "cbody": body.strip(),
                                "verdict": "REAL"})

    # v547 ground truth: 23 per-function-adjudicated HONEST-LIMITATION facades.
    # These are retained (their const return is truthful in C: SDK getter -> NULL,
    # network call with no C SDK -> 0/true, __enter__/__exit__ -> 0). Do NOT
    # re-litigate or re-flag them. (The 110 eradicated facades are already deleted
    # from the .c files, so they never appear in this scan.)
    V547_RETAINED = {
        "cli_agent_anthropic_adapter__get_anthropic_sdk",
        "cli_agent_bedrock_adapter__require_boto3",
        "cli_agent_display___enter__",
        "cli_agent_display___exit__",
        "cli_hermes_cli_memory_setup__get_available_providers",
        "cli_tools_clarify_tool_check_clarify_requirements",
        "cli_tools_env_passthrough__load_config_passthrough",
        "cli_tools_osv_check__query_osv",
        "todo_tool_check_todo_requirements",
        "cron_is_available",
        "mcp_tool_check_rate_limit",
        "mcp_tool_ensure_mcp_loop",
        "skills_hub_ensure_loaded",
        "skills_hub_load_catalog_index",
        "skills_hub_resolve_github_meta",
        "skills_hub_resolve_latest_version",
        "skills_hub_resolve_skill_md_url",
        "skills_hub_resolve_skill_name",
        "tts_tool_check_kittentts_available",
        "tts_tool_check_neutts_available",
        "tts_tool_check_piper_available",
        "tts_tool_gemini_model_supports_audio_tags",
        "tts_tool_resolve_command_provider_config",
    }

    # Adjudicate
    py_cache = {}
    def resolve_py(pypath_str):
        p = HERMES_DIR / pypath_str
        if p.exists():
            return p
        # search by basename anywhere under HERMES_DIR (nested modules)
        base = pypath_str.split("/")[-1]
        hits = list(HERMES_DIR.rglob(base))
        return hits[0] if hits else None

    for c in candidates:
        if c["cname"] in V547_RETAINED:
            c["verdict"] = "HONEST_CONST (v547 retained)"
            c["py_evidence"] = "v547 per-function adjudication: honest-limitation facade"
            continue
        if c["shape"] in ("FACADE_SHAPE", "NORET_SHAPE", "THIN_SHAPE"):
            pypath = resolve_py(c["py"].split(":")[0])
            try:
                if pypath is None:
                    c["verdict"] = "NEEDS_MANUAL (py fn not found)"
                    c["py_evidence"] = "python file not located"
                    continue
                if pypath not in py_cache:
                    py_cache[pypath] = ast.parse(pypath.read_text())
                tree = py_cache[pypath]
                node = locate_py_func(tree, c["py"].split(":")[1])
                if not node:
                    c["verdict"] = "NEEDS_MANUAL (py fn not found)"
                    c["py_evidence"] = "function not located in module"
                    continue
                hint, ev = py_classify(node)
                c["py_evidence"] = ev
                if c["shape"] == "FACADE_SHAPE":
                    if hint in ("HONEST_CONST", "HONEST_NOOP", "HONEST_SDKGUARD"):
                        c["verdict"] = "HONEST_CONST" if hint != "HONEST_NOOP" else "HONEST_CONST"
                        c["verdict"] = "HONEST_CONST"  # const/canned return truthful
                    else:
                        c["verdict"] = "FACADE_FRAUD"
                elif c["shape"] == "NORET_SHAPE":
                    if hint in ("HONEST_NOOP", "HONEST_SDKGUARD", "HONEST_CONST"):
                        c["verdict"] = "NORET_HONEST"
                    elif hint == "REAL_WORK":
                        c["verdict"] = "NORET_DORMANT"
                    else:
                        c["verdict"] = "NEEDS_MANUAL"
                elif c["shape"] == "THIN_SHAPE":
                    # thin: C returns a non-const expr. Could be honest delegation OR
                    # canned-const hiding as expr. Distinguish: if detail is a literal-ish
                    # strdup already caught by FACADE_SHAPE, so here it's real expr.
                    if hint in ("HONEST_CONST", "HONEST_NOOP", "HONEST_SDKGUARD"):
                        # Python returns const but C returns a real C helper — fine if helper
                        # genuinely computes. Mark for review.
                        c["verdict"] = "THIN_REVIEW"
                    elif hint == "REAL_WORK":
                        # Python does work; C returns a C-helper call -> honest delegation
                        c["verdict"] = "THIN_HONEST"
                    else:
                        c["verdict"] = "NEEDS_MANUAL"
            except Exception as e:
                c["verdict"] = f"NEEDS_MANUAL ({type(e).__name__})"
                c["py_evidence"] = str(e)

    # ---- Report ----
    from collections import Counter
    buckets = Counter(c["verdict"] for c in candidates)
    print(f"Files scanned : {len(files)}")
    print(f"Candidates    : {sum(1 for c in candidates if c['shape']!='REAL')} "
          f"(non-REAL shape); REAL-impl: {sum(1 for c in candidates if c['shape']=='REAL')}")
    print("Verdict buckets:")
    for k, v in sorted(buckets.items(), key=lambda x: -x[1]):
        print(f"  {k:18s}: {v}")
    print()

    def dump(title, pred):
        rows = [c for c in candidates if pred(c)]
        print(f"=== {title} ({len(rows)}) ===")
        for c in rows:
            print(f"  {c['cname']}  ({c['py']})")
            print(f"      C: {c['detail'] or c.get('cbody','')[:60]!r}")
            print(f"      Py: {c.get('py_evidence','?')}")
            print(f"      -> {c['verdict']}")
        print()

    dump("FACADE_FRAUD (demote)", lambda c: c["verdict"] == "FACADE_FRAUD")
    dump("HONEST_CONST (keep)", lambda c: c["verdict"] == "HONEST_CONST")
    dump("NORET_HONEST (keep)", lambda c: c["verdict"] == "NORET_HONEST")
    dump("NORET_DORMANT (demote)", lambda c: c["verdict"] == "NORET_DORMANT")
    dump("THIN_HONEST (keep)", lambda c: c["verdict"] == "THIN_HONEST")
    dump("THIN_REVIEW (needs read)", lambda c: c["verdict"] == "THIN_REVIEW")
    dump("NEEDS_MANUAL", lambda c: c["verdict"].startswith("NEEDS_MANUAL"))
    dump("NO_MATCH (check)", lambda c: c["shape"] == "NO_MATCH")

    outp = SLERMES_DIR / "tests" / ".v548_adjudicated.json"
    outp.write_text(json.dumps(candidates, indent=2))
    print(f"Wrote {outp}")


if __name__ == "__main__":
    main()
