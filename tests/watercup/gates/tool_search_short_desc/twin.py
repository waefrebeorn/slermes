"""twin for tool_search_short_desc — LIVE tools/tool_search.py::_short_desc."""
import os, sys, re
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_SENTENCE_END_RE = re.compile(r"[.!?\n]")


def _load_live():
    try:
        spec = ilu.spec_from_file_location(
            "wc_live_ts", os.path.join(DEV, "tools", "tool_search.py"))
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_live_ts"] = mod
        spec.loader.exec_module(mod)
        return mod._short_desc
    except Exception:
        return None


_live_fn = _load_live()


def _fallback(description, max_chars=60):
    text = " ".join((description or "").split())
    if not text:
        return ""
    m = _SENTENCE_END_RE.search(text)
    if m:
        text = text[:m.start() + (1 if text[m.start()] == "." else 0)]
    if len(text) <= max_chars:
        return text
    clipped = text[:max_chars]
    if " " in clipped:
        clipped = clipped.rsplit(" ", 1)[0]
    return clipped.rstrip(",;: ") + "\u2026"


def trace(raw):
    raw = raw.replace("\\t", "\t").replace("\\n", "\n")
    first, _, rest = raw.partition("\t")
    try:
        max_chars = int(first)
    except ValueError:
        max_chars, description = 60, raw
    else:
        description = rest

    c = Cup("_short_desc")
    text = " ".join((description or "").split())
    empty = not text
    m = _SENTENCE_END_RE.search(text)
    if m:
        text = text[:m.start() + (1 if text[m.start()] == "." else 0)]
    over = len(text) > max_chars and not empty
    clipped = text[:max_chars]
    c.branch(empty)                                # B1
    c.branch(m is not None)                        # B2
    c.branch(over)                                 # B3
    if over:
        c.branch(" " in clipped)                   # B4

    result = _live_fn(description, max_chars) if _live_fn else _fallback(description, max_chars)
    c.valstr(result)
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
