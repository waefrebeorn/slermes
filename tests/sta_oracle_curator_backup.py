#!/usr/bin/env python3
"""
sta_oracle_curator_backup.py — oracle for t_port_curator_backup.c.

Recomputes each case from the LIVE agent/curator_backup.py (is_enabled /
get_keep). The live functions call hermes_cli.config.load_config(); we
monkeypatch THAT to return the parsed YAML document, so is_enabled/get_keep
run with their exact canonical logic. Emits one JSON line per case (same
shape as the C harness). The runner diffs them.
"""
import os
import sys
import importlib.util
import yaml as pyyaml


def _load_cb():
    # Deterministic LIVE-Python resolution: prefer the canonical dev repo
    # (parent of slermes/) over any installed/stale copy on sys.path
    # (e.g. ~/.hermes/hermes-agent), which would manufacture false FAPs.
    _repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if _repo not in sys.path:
        sys.path.insert(0, _repo)
    for base in sys.path:
        cand = f"{base}/agent/curator_backup.py"
        try:
            spec = importlib.util.spec_from_file_location("live_cb", cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    import agent.curator_backup as mod  # type: ignore
    return mod


def _load_config_mod():
    for base in sys.path:
        cand = f"{base}/hermes_cli/config.py"
        try:
            spec = importlib.util.spec_from_file_location("live_cfg", cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    import hermes_cli.config as mod  # type: ignore
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
    cfg_mod = _load_config_mod()
    # The live agent/curator_backup.py does `from hermes_cli.config import
    # load_config` INSIDE _load_config() — that binds the attribute on the
    # REAL hermes_cli.config module at call time, not on our spec-loaded
    # "live_cfg" copy. Register the loaded module under its real name so the
    # patch actually lands.
    import importlib
    import hermes_cli.config as real_cfg
    importlib.reload(real_cfg)
    for k in list(sys.modules):
        if k.startswith("hermes_cli.config") and k != "hermes_cli.config":
            del sys.modules[k]
    # Point the real module's load_config at our monkeypatch too.
    saved = real_cfg.load_config

    for doc in docs:
        doc = doc.strip()
        if not doc:
            continue
        parsed = pyyaml.safe_load(doc) or {}
        cfg_mod.load_config = lambda: parsed
        real_cfg.load_config = lambda: parsed
        try:
            enabled = bool(cb.is_enabled())
            keep = int(cb.get_keep())
        finally:
            cfg_mod.load_config = saved
            real_cfg.load_config = saved
        sys.stdout.write(
            '{"enabled":%s,"keep":%d}\n' % ("true" if enabled else "false", keep)
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
