"""twin for signal_format_markdown_to_signal.

Traces gateway/platforms/signal_format.py::markdown_to_signal with the SAME
rung order as the C harness (see harness.c header for the ladder). The twin
mirrors the C port's scan structure (char loops, PCRE2-equivalent regexes
compiled with DOTALL like the port does) and then HONESTY-CHECKS its final
(text, styles) against the LIVE function's actual output — a drift raises,
so a stale twin can never silently pass. See gate.json notes for the
documented C/Py divergences the inputs steer around.
"""
import os, sys, re, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_signal_format", os.path.join(DEV, "gateway", "platforms", "signal_format.py"))
_sf = ilu.module_from_spec(_spec)
sys.modules["wc_live_signal_format"] = _sf
_spec.loader.exec_module(_sf)

# From here on the twin mirrors the C port BYTE-FOR-BYTE: the rewritten
# bullet "\u2022" is 3 UTF-8 bytes in C, so all positions/scan lengths below
# are byte offsets over the UTF-8 encoding (regexes compiled on bytes).
HEADING_RE = re.compile(rb"^#{1,6}\s+", re.MULTILINE)
# C port compiles every inline pattern with PCRE2_DOTALL — mirror that here.
PATTERNS = [
    (re.compile(rb"\*\*(.+?)\*\*", re.DOTALL), "BOLD"),
    (re.compile(rb"__(.+?)__", re.DOTALL), "BOLD"),
    (re.compile(rb"~~(.+?)~~", re.DOTALL), "STRIKETHROUGH"),
    (re.compile(rb"`(.+?)`", re.DOTALL), "MONOSPACE"),
    (re.compile(rb"(?<!\*)\*(?!\*| )(.+?)(?<!\*)\*(?!\*)", re.DOTALL), "ITALIC"),
    (re.compile(rb"(?<!\w)_(?!_)(.+?)(?<!_)_(?!\w)", re.DOTALL), "ITALIC"),
]


def _utf16_len(s):
    return len(s.encode("utf-16-le")) // 2


def trace(text, c):
    # Phase 1: collapse 3+ newlines -> 2, strip.
    out = []
    i, n = 0, len(text)
    while i < n:
        c.loop()
        if text[i] == "\n":                                   # B1
            c.branch(True)
            k = i
            nc = 0
            while k < n and text[k] == "\n":
                nc += 1
                k += 1
            reps = 2 if nc >= 3 else nc
            out.append("\n" * reps)
            i = k
        else:
            c.branch(False)
            out.append(text[i])
            i += 1
    t = "".join(out).strip()

    # Phase 2: bullet normalization (C char-scan semantics).
    out = []
    i, n = 0, len(t)
    in_code = False
    while i < n:
        c.loop()
        if not in_code and t[i:i + 3] == "```":               # B2
            c.branch(True)
            out.append("```")
            i += 3
            in_code = True
            continue
        c.branch(False)
        if in_code:                                           # B3
            c.branch(True)
            out.append(t[i])
            if t[i] == "`" and t[i + 1:i + 3] == "``":
                out.append("``")
                i += 3
                in_code = False
                continue
            i += 1
            continue
        c.branch(False)
        j = 0 if i == 0 else i + 1
        is_linestart = (t[i] == "\n" or i == 0)
        jj = j
        indent = 0
        while jj < n and t[jj] in " \t" and indent < 3:
            jj += 1
            indent += 1
        is_bullet = (jj < n and t[jj] in "-*+" and jj + 1 < n and t[jj + 1] == " ")
        if is_linestart and is_bullet:                        # B4
            c.branch(True)
            if i != 0:
                out.append("\n")
            out.append(" " * indent + "\u2022 ")
            i = jj + 2
            continue
        c.branch(False)
        out.append(t[i])
        i += 1
    t = "".join(out).encode("utf-8")   # C scans bytes from here on

    styles = []  # [start, len, style]

    # Phase 3: fenced code blocks -> inner (MONOSPACE).
    cbs = []
    blen = len(t)
    i = 0
    while i + 2 < blen:
        c.loop()
        if t[i] == 0x60 and t[i + 1] == 0x60 and t[i + 2] == 0x60:   # B5
            c.branch(True)
            j = i + 3
            while j < blen and (chr(t[j]).isalnum() or chr(t[j]) in "_+-"):
                j += 1
            if j < blen and t[j] == 0x0A:
                j += 1
            g1s = j
            k = g1s
            found = False
            while k + 2 < blen:
                if t[k:k + 3] == b"```":
                    found = True
                    break
                k += 1
            if not found:                                         # B6
                c.branch(False)
                break
            c.branch(True)
            g1e = k
            while g1e > g1s and t[g1e - 1] == 0x0A:
                g1e -= 1
            cbs.append((i, k + 3, g1s, g1e))
            i = k + 3
            continue
        c.branch(False)
        i += 1
    for start, end, g1s, g1e in reversed(cbs):
        inner = t[g1s:g1e]
        styles.append([start, len(inner), "MONOSPACE"])
        t = t[:start] + inner + t[end:]

    # Phase 4: headings ^#{1,6}\s+ -> BOLD.
    new_text_parts = []
    last_end = 0
    for m in HEADING_RE.finditer(t):
        c.loop()
        hs, he = m.start(), m.end()
        new_text_parts.append(t[last_end:hs])
        eol = t.find(b"\n", he)
        if eol == -1:                                             # B7
            c.branch(False)
            eol = len(t)
        else:
            c.branch(True)
        heading_text = t[he:eol]
        start = sum(len(p) for p in new_text_parts)
        new_text_parts.append(heading_text)
        styles.append([start, len(heading_text), "BOLD"])
        last_end = eol
    new_text_parts.append(t[last_end:])
    t = b"".join(new_text_parts)

    # Phase 5: inline patterns with overlap avoidance.
    all_matches = []
    occupied = []
    for pat, style_name in PATTERNS:                              # LOOP patterns
        c.loop()
        for m in pat.finditer(t):                                 # LOOP matches
            c.loop()
            ms, me = m.start(), m.end()
            g1s, g1e = m.start(1), m.end(1)
            overlaps = any(ms < oe and me > os_ for os_, oe in occupied)
            if not overlaps:                                      # B8
                c.branch(False)
                all_matches.append((ms, me, g1s, g1e, style_name))
                occupied.append((ms, me))
            else:
                c.branch(True)
    all_matches.sort(key=lambda x: x[0])   # stable, mirrors C sort by ms

    # Phase 6: removals + adjust prior styles.
    removals = []
    for ms, me, g1s, g1e, _ in all_matches:
        c.loop()
        if g1s > ms:                                              # B9
            c.branch(True)
            removals.append((ms, g1s - ms))
        else:
            c.branch(False)
        if me > g1e:                                              # B10
            c.branch(True)
            removals.append((g1e, me - g1e))
        else:
            c.branch(False)
    removals.sort(key=lambda x: x[0])
    adj = []
    for start0, length0, style_name in styles:                    # LOOP prior
        c.loop()
        start, length = start0, length0
        shift = 0
        for rpos, rlen in removals:
            if rpos < start:
                shift += rlen
            else:
                break
        start -= shift
        end = start + length
        end_shift = 0
        for rpos, rlen in removals:
            if rpos < end:
                end_shift += rlen
            else:
                break
        end -= end_shift
        adj.append([start, end - start, style_name])

    # Phase 7: rebuild text + inline styles.
    result = []
    last_end = 0
    inline = []
    for ms, me, g1s, g1e, style_name in all_matches:
        c.loop()
        result.append(t[last_end:ms])
        pos = sum(len(p) for p in result)
        inner = t[g1s:g1e]
        result.append(inner)
        inline.append([pos, len(inner), style_name])
        last_end = me
    result.append(t[last_end:])
    t2 = b"".join(result)

    # Phase 8: merge, sort, drop out-of-range, UTF-16 map.
    merged = adj + inline
    merged.sort(key=lambda e: e[0])
    style_strings = []
    for cp_start, cp_len, style_type in merged:                   # LOOP styles
        c.loop()
        if cp_start < 0 or cp_start + cp_len > len(t2) or cp_len <= 0:   # B11
            c.branch(True)
            continue
        c.branch(False)
        prefix = t2[:cp_start]
        seg = t2[cp_start:cp_start + cp_len]
        # C's utf16_len walks raw bytes; equivalent to decoding whole valid
        # UTF-8 spans, so decode the full text once and slice by chars only
        # when the boundaries are UTF-8-aligned (they are — regex-derived).
        t2_str = t2.decode("utf-8")
        u16 = lambda byte_off: _utf16_len(t2_str[:len(t2[:byte_off].decode("utf-8"))])
        style_strings.append(
            "%d:%d:%s" % (u16(cp_start), u16(cp_start + cp_len) - u16(cp_start),
                          style_type))
    return t2.decode("utf-8"), style_strings


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("markdown_to_signal")
        got = trace(raw, c)
        live_t, live_s = _sf.markdown_to_signal(raw)
        if got != (live_t, live_s):
            raise AssertionError(
                "traced ladder diverges from LIVE markdown_to_signal for %r\n"
                " traced=%r\n live  =%r" % (raw, got, (live_t, live_s)))
        cups.append(c.finalize())
    return cups
