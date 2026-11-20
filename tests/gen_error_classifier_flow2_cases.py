#!/usr/bin/env python3
"""Generate tests/fixtures/error_classifier_flow2.jsonl for the FLOW2 port of
the NINE unclaimed leaves of agent/error_classifier.py:

  _is_byte_size_limit_message, is_reasoning_level_rejection,
  is_reasoning_required_rejection, is_reasoning_field_rejection,
  _by_message, _account_gates_model, _has_large_inline_image,
  _oversized_message_content_rejection, _classify_image_tool_422

Regex primitives (the compiled pattern objects) are treated as recordable
collaborators, exactly like flow1's claimed leaves: the generator computes each
pattern's answer with the REAL quarry regex and writes it as a script in in.io;
both the C port and the live-oracle drive read the same script. Verdict
constants and the claimed siblings (_first_match / _has_usage_limit_transient_signal
/ _classify_402) are scripted the same way.
"""
import ast
import enum
import itertools
import json
import re
import textwrap
from pathlib import Path

HERE = Path(__file__).resolve()
REPO = HERE.parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "agent" / "error_classifier.py"

KINDS = [
    "_is_byte_size_limit_message",
    "is_reasoning_level_rejection",
    "is_reasoning_required_rejection",
    "is_reasoning_field_rejection",
    "_by_message",
    "_account_gates_model",
    "_has_large_inline_image",
    "_oversized_message_content_rejection",
    "_classify_image_tool_422",
]

# ---- seed the real quarry constants (AST, no import side effects) ----
_ANN = getattr(__import__("__future__").annotations, "compiler_flag", 0)


def _seg(src, node, ns):
    code = textwrap.dedent(ast.get_source_segment(src, node))
    exec(compile(code, str(SRC), "exec", flags=_ANN, dont_inherit=True), ns)


def load_constants():
    src = SRC.read_text(encoding="utf-8")
    tree = ast.parse(src)
    ns = {
        "__name__": "ec2_consts", "enum": enum, "re": re,
        "itertools": itertools, "json": json,
        "Any": object, "Optional": object, "Callable": object,
        "Dict": dict, "Iterator": object, "Sequence": object,
        "dataclass": lambda c: c, "field": lambda **k: None,
    }
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "FailoverReason":
            _seg(src, node, ns)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_v":
            _seg(src, node, ns)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            if isinstance(node, ast.AnnAssign) and node.value is None:
                continue
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            def flat(t):
                if isinstance(t, ast.Name):
                    return [t.id]
                if isinstance(t, (ast.Tuple, ast.List)):
                    ids = []
                    for e in t.elts:
                        ids += flat(e)
                    return ids
                return None
            names = []
            for t in tgts:
                got = flat(t)
                if got is None:
                    names = None
                    break
                names += got
            if not names:
                continue
            if any(n.startswith("logger") for n in names):
                continue
            try:
                _seg(src, node, ns)
            except Exception:
                pass
    need = ["_BYTE_SIZE_OVERFLOW", "_TOKEN_COUNT", "_BYTE_SIZE_SUBJECT",
            "_REQUEST_WORDS", "_REASONING_FIELD_TOKEN", "_REASONING_PARAM_REJECTION",
            "_REASONING_REQUIRED_MARKERS", "_REASONING_LEVEL_REJECTION_MARKERS",
            "UNSUPPORTED_PARAM_MARKERS", "_ACCOUNT_GATING_TAIL_RE",
            "_ACCOUNT_GATED_MODEL_RE", "_WALL_WORDS", "_LONG_QUOTA_WINDOW_PATTERNS",
            "_USAGE_LIMIT_PATTERNS", "_IMAGE_TOOL_RULES",
            "_V_PAYLOAD_TOO_LARGE", "_V_RATE_LIMIT", "_V_BILLING",
            "_V_MODEL_ENTITLEMENT", "_V_IMAGE_TOO_LARGE", "_V_FORMAT_ERROR"]
    missing = [n for n in need if n not in ns]
    if missing:
        raise RuntimeError("constant seed missing: %s" % missing)
    return ns


C = load_constants()

# ---- helpers ----
def vjson(v):
    """Project a live value to JSON (enum -> .value)."""
    if isinstance(v, enum.Enum):
        return v.value
    if isinstance(v, dict):
        return {k: vjson(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [vjson(x) for x in v]
    return v


def tok_match(pat, s):
    m = pat.search(s)
    return None if m is None else {"start": m.start(), "end": m.end()}


def sc_list(value):
    """A list-valued seam RESULT must be wrapped: a bare array is a per-call
    queue (and an empty array is an exhausted queue -> fail-closed)."""
    return {"__script__": True, "ret": value}


def byte_scripts(msg):
    return {
        "byte_size_overflow": bool(C["_BYTE_SIZE_OVERFLOW"].search(msg)),
        "token_count": bool(C["_TOKEN_COUNT"].search(msg)),
        "byte_size_subject": bool(C["_BYTE_SIZE_SUBJECT"].search(msg)),
        "request_words": sc_list([w for w in re.split(r"[^a-z0-9_]+", msg)
                                  if w in C["_REQUEST_WORDS"]]),
    }


def reasoning_token_script(msg):
    return tok_match(C["_REASONING_FIELD_TOKEN"], msg)


def verdicts():
    return {
        "V_PAYLOAD_TOO_LARGE": vjson(C["_V_PAYLOAD_TOO_LARGE"]),
        "V_RATE_LIMIT": vjson(C["_V_RATE_LIMIT"]),
        "V_BILLING": vjson(C["_V_BILLING"]),
        "V_MODEL_ENTITLEMENT": vjson(C["_V_MODEL_ENTITLEMENT"]),
        "V_IMAGE_TOO_LARGE": vjson(C["_V_IMAGE_TOO_LARGE"]),
        "V_FORMAT_ERROR": vjson(C["_V_FORMAT_ERROR"]),
        "image_shrink_target_bytes": 4 * 1024 * 1024,
    }


CASES = []


def add(func, msg=None, body=None, content=None, io=None, note=""):
    inp = {}
    if msg is not None:
        inp["msg"] = msg
    if body is not None:
        inp["body"] = body
    if content is not None:
        inp["content"] = content
    full_io = dict(verdicts())
    full_io.update(io or {})
    inp["io"] = full_io
    CASES.append({"func": func, "in": inp, "_note": note})


# ---------------- _is_byte_size_limit_message ----------------
for msg in [
    "request size exceeds the maximum allowed",
    "Request body too large for gpt-4.1 model. Max size: 8000 tokens.",
    "tools[0].description content length exceeds maximum allowed",
    "content length exceeds limit; invalid_request_error payload bytes too big",
    "response payload too large, cannot shrink the request",
    "payload too large",
    "requestsize exceeds limit and content-length is 40000",
    "just a plain validation failure",
    "",
]:
    io = byte_scripts(msg)
    add("_is_byte_size_limit_message", msg=msg, io=io)

# ---------------- reasoning trio ----------------
REASON_MSGS = [
    "'reasoning_effort' must be one of: 'minimal', 'low', 'medium'",
    "invalid value for reasoning_effort: none",
    "Reasoning is mandatory for this endpoint and cannot be disabled",
    "reasoning_effort cannot be turned off for this model",
    "Unrecognized request argument supplied: reasoning_effort",
    "unsupported reasoning_effort 'none'; use minimal|low|medium|high",
    "'param': 'reasoning.effort' is invalid",
    "invalid_reasoning_effort in request body",
    "reasoning_effort is not supported with reasoning models",
    "A 400 unrelated to any wire control",
]
for msg in REASON_MSGS:
    low = msg.lower()
    io = {
        "reasoning_field_token": reasoning_token_script(low),
        "reasoning_param_rejection": bool(C["_REASONING_PARAM_REJECTION"].search(low)),
    }
    for fn in ("is_reasoning_level_rejection",
               "is_reasoning_required_rejection",
               "is_reasoning_field_rejection"):
        add(fn, msg=msg, io=dict(io))

# ---------------- _account_gates_model ----------------
ACCOUNT_MSGS = [
    "The model gpt-6-astra is not supported for this account",
    "The model gpt-6-astra is not supported when using this account, quota exceeded",
    "model pro-1 is not supported for this account but free tier is available",
    "this model is not supported for this account",
    "feature is not supported for this account",
    "The model abc-123 is not supported for this account and billing is required",
    "no gating phrase here at all",
]
for msg in ACCOUNT_MSGS:
    tails = [{"start": m.start()} for m in C["_ACCOUNT_GATING_TAIL_RE"].finditer(msg)][:4]
    gated = []
    for t in tails:
        s = t["start"]
        gated.append(bool(C["_ACCOUNT_GATED_MODEL_RE"].search(msg, max(0, s - 160), s)))
    wall = any(p in msg for p in C["_WALL_WORDS"])
    add("_account_gates_model", msg=msg, io={
        "account_gating_tail": sc_list(tails),
        "account_gated_model": gated,
        "wall_words": wall,
    })

# ---------------- _by_message ----------------
BY_MSG = [
    ("request entity too large", {}),
    ("Request body too large for gpt-4.1 model. Max 8000 tokens.", {}),
    ("usage limit reached, try again in 30 seconds", {}),
    ("usage limit reached, resets at midnight", {}),
    ("The model gpt-6-astra is not supported for this account", {}),
    ("daily limit exceeded", {}),
    ("service is overloaded", {}),
    ("just some other failure", {}),
]
V_OVER = vjson(C["_V_RATE_LIMIT"])
for msg, body in BY_MSG:
    low = msg.lower()
    io = byte_scripts(low)
    io.update({
        "long_quota_window": any(p in low for p in C["_LONG_QUOTA_WINDOW_PATTERNS"]),
        "usage_limit": any(p in low for p in C["_USAGE_LIMIT_PATTERNS"]),
        "first_match": None,
        "has_usage_limit_transient_signal": bool(any(
            s in low for s in ("try again", "retry", "resets at", "reset in", "window", "per minute"))),
        "classify_402": vjson(C["_V_BILLING"]),
        "account_gating_tail": sc_list([{"start": m.start()} for m in C["_ACCOUNT_GATING_TAIL_RE"].finditer(msg)][:4]),
        "account_gated_model": [bool(C["_ACCOUNT_GATED_MODEL_RE"].search(
            msg, max(0, m.start() - 160), m.start()))
            for m in list(C["_ACCOUNT_GATING_TAIL_RE"].finditer(msg))[:4]],
        "wall_words": any(p in msg for p in C["_WALL_WORDS"]),
    })
    add("_by_message", msg=msg, body=body, io=io)

# ---------------- _has_large_inline_image ----------------
BIG = "data:image/png;base64," + "A" * (4 * 1024 * 1024 + 16)
SMALL = "data:image/png;base64," + "A" * 32
for content in [
    [{"image_url": {"url": BIG}}],
    [{"image_url": {"url": SMALL}}],
    [{"image_url": BIG}],
    [{"type": "text", "text": "hello"}],
    [],
    "not-a-list",
    [{"image_url": {"url": "https://example.com/x.png"}}],
    [None, {"image_url": {"url": BIG}}],
]:
    add("_has_large_inline_image", content=content, io={})

# ---------------- _oversized_message_content_rejection ----------------
def det(loc, typ="string_type", inp=None):
    d = {"type": typ, "loc": loc}
    if inp is not None:
        d["input"] = inp
    return d

OV_CASES = [
    {"detail": [det(["body", "messages", 0, "content", "str"], inp=[{"image_url": {"url": BIG}}])]},
    {"detail": [det(["body", "messages", 0, "content", "str"], inp=[{"image_url": {"url": SMALL}}])]},
    {"detail": [det(["body", "messages", 0, "tool", "content", "str"], inp=[{"image_url": {"url": BIG}}])]},
    {"detail": [det(["body", "messages", 0, "content", "str"], inp=[{"type": "text"}])]},
    {"detail": [det(["body", "messages", 0, "content", "str"], typ="string_type")]},
    {"detail": [det(["body", "messages", 0, "content", "str"], typ="value_error")]},
    {"detail": [det(["content", "str"], inp=[{"image_url": {"url": BIG}}])]},
    {"detail": []},
    {},
]
for body in OV_CASES:
    add("_oversized_message_content_rejection", body=body, io={})

# ---------------- _classify_image_tool_422 ----------------
for msg, body in [
    ("invalid content", OV_CASES[0]),
    ("invalid content", OV_CASES[1]),
    ("image tool content rejected", {}),
    ("some 422 validation failure", {}),
    ("", {}),
]:
    # first_match('image_tool'): a scripted verdict, or null -> V_FORMAT_ERROR
    add("_classify_image_tool_422", msg=msg, body=body,
        io={"first_match": vjson(C["_V_IMAGE_CORRUPT"]) if "image tool" in msg else None})

# ---- write fixtures ----
out = REPO / "tests" / "fixtures" / "error_classifier_flow2.jsonl"
out.parent.mkdir(parents=True, exist_ok=True)
with out.open("w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        note = c.pop("_note")
        f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")

counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print("wrote %d cases to %s" % (len(CASES), out))
for k in KINDS:
    print("  %-40s %d" % (k, counts.get(k, 0)))
