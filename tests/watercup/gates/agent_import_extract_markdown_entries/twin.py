"""twin for agent_import_extract_markdown_entries.

Traces the LIVE hermes_cli/agent_import.py::extract_markdown_entries with the
same rung order as the C harness. Uses LIVE normalize_text for dedup and the
LIVE module's own regex semantics (identical patterns, re-executed here).
"""
import os
import sys
import re
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_agent_import",
    os.path.join(DEV, "hermes_cli", "agent_import.py"))
_ai = ilu.module_from_spec(_spec)
sys.modules["wc_live_agent_import"] = _ai
_spec.loader.exec_module(_ai)

_HEADING_RE = re.compile(r"^(#{1,6})\s+(.*\S)\s*$")
_BULLET_RE = re.compile(r"^\s*(?:[-*]|\d+\.)\s+(.*\S)\s*$")
_BANNED_RE = re.compile(r"\b(MEMORY|USER|SOUL|AGENTS|TOOLS|IDENTITY|CLAUDE)\.md\b", re.I)


def trace(text):
    c = Cup("extract_markdown_entries")
    entries = []
    headings = []
    para = []

    def context_prefix():
        filtered = [h for h in headings if h and not _BANNED_RE.search(h)]
        return " > ".join(filtered)

    def flush_paragraph():
        if not para:
            return
        block = " ".join(l.strip() for l in para).strip()
        para.clear()
        if not block:
            return
        prefix = context_prefix()
        entry = f"{prefix}: {block}" if prefix else block
        c.valstr(entry)
        entries.append(entry)

    in_code = False
    for raw_line in (text or "").splitlines():
        c.loop()
        line = raw_line.rstrip()
        stripped = line.strip()

        if stripped.startswith("```"):            # B1
            c.branch(True)
            flush_paragraph()
            in_code = not in_code
            continue
        c.branch(False)
        if in_code:                               # B2
            c.branch(True)
            continue
        c.branch(False)
        m = _HEADING_RE.match(stripped)
        c.branch(bool(m))                         # B3
        if m:
            flush_paragraph()
            level = len(m.group(1))
            value = m.group(2).strip()
            while len(headings) >= level:
                headings.pop()
            headings.append(value)
            continue
        bm = _BULLET_RE.match(line)
        c.branch(bool(bm))                        # B4
        if bm:
            flush_paragraph()
            content = bm.group(1).strip()
            prefix = context_prefix()
            entry = f"{prefix}: {content}" if prefix else content
            c.valstr(entry)
            entries.append(entry)
            continue
        if not stripped:                          # B5
            c.branch(True)
            flush_paragraph()
            continue
        c.branch(False)
        if stripped.startswith("|") and stripped.endswith("|"):  # B6
            c.branch(True)
            flush_paragraph()
            continue
        c.branch(False)
        para.append(stripped)

    flush_paragraph()

    seen = set()
    for entry in entries:
        c.loop()
        normalized = _ai.normalize_text(entry)
        dup = (not normalized) or (normalized in seen)
        c.branch(dup)
        if not dup:
            seen.add(normalized)
            c.valstr(entry.strip())
    return c


def run(inputs):
    cups = []
    for raw in inputs:
        text = raw.replace("\\n", "\n")
        cups.append(trace(text).finalize())
    return cups
