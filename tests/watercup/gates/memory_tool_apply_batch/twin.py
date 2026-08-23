"""twin for memory_tool_apply_batch.

Traces LIVE tools/memory_tool.py::MemoryStore.apply_batch with the same rung
order as the C harness (traced copy of src/tools/memory_tool.c::
memory_store_apply_batch). The store here is seeded from the scenario string
exactly as the harness seeds its in-memory store; the threat scan calls the
LIVE _scan_memory_content (strict scope).
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)
sys.path.insert(0, os.path.join(SLERMES, "tests", "watercup"))

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_memory_tool",
    os.path.join(DEV, "tools", "memory_tool.py"))
_bk = ilu.module_from_spec(_spec)
sys.modules["wc_live_memory_tool"] = _bk
_spec.loader.exec_module(_bk)

DELIM = _bk.ENTRY_DELIMITER


def parse(scenario):
    parts = scenario.split("||")
    hdr = parts[0].split("|")
    target = hdr[0]
    limit = int(hdr[1]) if len(hdr) > 1 and hdr[1] else 2200
    entries = hdr[2].split(";;") if len(hdr) > 2 and hdr[2] else []
    ops = []
    for p in parts[1:]:
        f = p.split("~")
        ops.append({
            "action": f[0] if f and f[0] else None,
            "content": f[1] if len(f) > 1 else "",
            "old_text": f[2] if len(f) > 2 else "",
        })
    return target, limit, list(entries), ops


def trace(scenario):
    c = Cup("apply_batch")
    target, limit, working, ops = parse(scenario)
    committed = False

    def finish():
        if committed:
            v = ";".join(working)
        else:
            v = "ERR"
        c.valstr(target)
        c.valstr(v)
        return c.finalize()

    if not ops:
        c.branch(True)                                   # B0
        return finish()
    c.branch(False)

    # scan pre-pass (live strict-scope scanner)
    for op in ops:
        c.loop()                                         # L
        act = op.get("action")
        content = op.get("content")
        if act in {"add", "replace"} and content:        # B1
            c.branch(True)
            scan_error = _bk._scan_memory_content(content)
            c.branch(bool(scan_error))                   # B2
            if scan_error:
                return finish()
        else:
            c.branch(False)

    # apply loop
    for op in ops:
        c.loop()                                         # L
        act = op.get("action")
        content = (op.get("content") or "")
        old_text = (op.get("old_text") or "")
        if not act:                                      # B3
            c.branch(True)
            return finish()
        c.branch(False)
        if act == "add":                                 # B4
            c.branch(True)
            if not content:                              # B5
                c.branch(True)
                return finish()
            c.branch(False)
            dup = content in working
            c.branch(dup)                                # B6
            if not dup:
                working.append(content)
        elif act == "replace":                           # B7
            c.branch(False)
            c.branch(True)
            if not old_text:                             # B8
                c.branch(True)
                return finish()
            c.branch(False)
            if not content:                              # B9
                c.branch(True)
                return finish()
            c.branch(False)
            matches = []
            for e in working:
                hit = old_text in e
                c.loop()                                 # L
                c.branch(hit)                            # B10
                if hit:
                    matches.append(e)
            c.branch(len(matches) == 0)                  # B11
            if not matches:
                return finish()
            c.branch(False)
            seen_first = None
            distinct = False
            for m in matches:
                if seen_first is None:
                    seen_first = m
                elif m != seen_first:
                    distinct = True
            c.branch(distinct)                           # B12
            if distinct:
                return finish()
            working[working.index(matches[0])] = content
        elif act == "remove":                            # B13
            c.branch(False)
            c.branch(False)
            c.branch(True)
            if not old_text:                             # B14
                c.branch(True)
                return finish()
            c.branch(False)
            matches = []
            for e in working:
                hit = old_text in e
                c.loop()                                 # L
                c.branch(hit)                            # B15
                if hit:
                    matches.append(e)
            c.branch(len(matches) == 0)                  # B16
            if not matches:
                return finish()
            c.branch(False)
            seen_first = None
            distinct = False
            for m in matches:
                if seen_first is None:
                    seen_first = m
                elif m != seen_first:
                    distinct = True
            c.branch(distinct)                           # B17
            if distinct:
                return finish()
            working.pop(working.index(matches[0]))
        else:                                            # unknown action
            c.branch(False)
            c.branch(False)
            c.branch(False)
            return finish()

    new_total = len(DELIM.join(working)) if working else 0
    over = new_total > limit
    c.branch(over)                                       # B18
    if over:
        return finish()                                  # consolidation failure
    committed = True
    return finish()


def run(inputs):
    return [trace(s) for s in inputs]
