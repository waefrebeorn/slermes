#!/usr/bin/env python3
"""sta_oracle_process_bootstrap_pure.py — LIVE-Python oracle for
pbs_interleave_addrinfos (agent/process_bootstrap.py:_interleave_addrinfos).

Reads a JSON array of addrinfo rows
  [{"family":2,"socktype":1,"proto":6,"sockaddr":"1.2.3.4:443"}, ...]
(one JSON array per line), runs the LIVE _interleave_addrinfos, and prints
the emitted sockaddr sequence as one JSON array per line.
"""
import json
import os
import sys

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from agent.process_bootstrap import _interleave_addrinfos  # noqa: E402


def main():
    for line in open(sys.argv[1], encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        rows = json.loads(line)
        addrinfos = [
            (r["family"], r["socktype"], r["proto"], "", r["sockaddr"])
            for r in rows
        ]
        out = _interleave_addrinfos(addrinfos)
        print(json.dumps([a[4] for a in out]))


if __name__ == "__main__":
    main()
