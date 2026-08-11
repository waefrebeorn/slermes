#!/usr/bin/env python3
"""
sta_oracle_curator_backup.py — oracle for t_port_curator_backup.c.

Recomputes each case from the LIVE agent/curator_backup.py (is_enabled /
get_keep). The live functions call hermes_cli.config.load_config_readonly();
we monkeypatch THAT to return the parsed YAML document, so is_enabled/get_keep
run with their exact canonical logic. Emits one JSON line per case (same
shape as the C harness). The runner diffs them.
"""
import os
import sys
import importlib
import yaml as pyyaml


def _load_cb():
    # Deterministic LIVE-Python resolution: prefer the canonical dev repo
    # (parent of slermes/) over any installed/stale copy on sys.path
    # (e.g. ~/.hermes/hermes-agent), which would manufacture false FAPs.
    _repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if _repo not in sys.path:
        sys.path.insert(0, _repo)
    import agent.curator_backup as mod  # type: ignore
    return mod


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_curator_backup.py <docs.yaml>\n")
        return 2
    with open(sys.argv[1], "rb") as f:
        raw = f.read()
    text = raw.decode("utf-8")

    docs = []
    cur = []
    for line in text.split("\n"):
        if line.strip() == "---":
            docs.append("\n".join(cur))
            cur = []
        else:
            cur.append(line)
    docs.append("\n".join(cur))

    cb = _load_cb()
    # The live agent/curator_backup.py calls `load_config_readonly()` which
    # returns the FULL config dict. Monkeypatch it to return the parsed YAML.
    import hermes_cli.config as real_cfg
    saved = real_cfg.load_config_readonly

    for i, doc in enumerate(docs):
        doc = doc.strip()
        if not doc:
            continue
        parsed = pyyaml.safe_load(doc) or {}
        # Correct closure: bind parsed via default argument
        real_cfg.load_config_readonly = lambda p=parsed: p
        try:
            enabled = bool(cb.is_enabled())
            keep = int(cb.get_keep())
        finally:
            real_cfg.load_config_readonly = saved
        sys.stdout.write(
            '{"enabled":%s,"keep":%d}\n' % ("true" if enabled else "false", keep)
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())