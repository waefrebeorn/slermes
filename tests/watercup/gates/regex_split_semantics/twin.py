"""twin for regex_split_semantics.

Traces LIVE python re.split semantics with the SAME rung order as the C
harness (S1 maxsplit limit / S2 no match / S3 zero-width) and pins the
joined fields (\\x1f) as the V event. The scanner mirror reproduces CPython's
sre scanner: every match emits its preceding segment plus captured groups; a
zero-width match consumes one character.
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


def split_trace(pat, s, maxsplit):
    c = Cup("regex_split_semantics")
    rx = re.compile(pat)
    pos = 0            # search cursor
    last = 0           # start of the pending segment
    splits = 0
    fields = []
    ngroups = rx.groups
    while True:
        if maxsplit >= 0 and splits >= maxsplit:            # S1
            c.branch(True)
            break
        c.branch(False)
        m = rx.search(s, pos)
        if not m:                                           # S2
            c.branch(True)
            break
        c.branch(False)
        fields.append(s[last:m.start()])
        for g in range(1, ngroups + 1):
            fields.append(m.group(g) or "")
        if m.start() == m.end():                            # S3 zero-width
            c.branch(True)
            if m.end() >= len(s):
                last = len(s) + 1                           # trailing ""
                break
            last = m.end()
            pos = m.end() + 1
        else:
            c.branch(False)
            last = pos = m.end()
        splits += 1
    fields.append(s[last:])
    # LIVE semantics assert
    live = re.split(pat, s, maxsplit=maxsplit) if maxsplit >= 0 \
        else re.split(pat, s)
    assert fields == live, "twin drift: %r vs re.split %r" % (fields, live)
    c.valstr("\x1f".join(fields))
    return c.finalize()


def run(inputs):
    cups = []
    for probe in inputs:
        ms, _, rest = probe.partition("|")
        pat, _, subj = rest.partition("|")
        cups.append(split_trace(pat, subj, int(ms)))
    return cups
