"""twin for gw_format_truncate_message.

Traces the LIVE gateway truncation contract that src/gateway/gw_format.c's
E43 port captures: verbatim when short enough, word-boundary soft cut when
the break lands past half the budget, hard cut otherwise, '...' suffix.
Same rung order as harness.c. Probes: "text|maxlen".
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
from watercup_py import Cup


def truncate_message_trace(text, max_len):
    c = Cup("gw_truncate_message")
    # B1: guard
    if not text or max_len == 0:
        c.branch(True)
        result = ""
        c.valstr(result)
        c.finalize()
        return result, c
    c.branch(False)

    if len(text) <= max_len:          # B2
        c.branch(True)
        result = text
        c.valstr(result)
        c.finalize()
        return result, c
    c.branch(False)

    head = text[:max_len]
    break_at = max_len
    while break_at > 0 and head[break_at - 1] != " ":
        break_at -= 1

    if break_at > max_len // 2:       # B3
        c.branch(True)
        result = head[:break_at] + "..."
    else:                             # B4
        c.branch(False)
        result = head[:max_len] + "..."
    c.valstr(result)
    c.finalize()
    return result, c


def run(inputs):
    outs = []
    for probe in inputs:
        text, _, maxlen = probe.rpartition("|")
        outs.append(truncate_message_trace(text, int(maxlen))[1].finalize())
    return outs
