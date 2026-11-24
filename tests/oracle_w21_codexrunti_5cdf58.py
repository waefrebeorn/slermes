#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/codex_runtime.py
(gate w21_codexrunti_5cdf58).

The live sub-command module and the live `MigrationReport` dataclass are
executed as-is. Only the two seams `cmd_codex_runtime_migrate` reaches through
imports are stubbed: `hermes_cli.codex_runtime_plugin_migration.migrate` (the
run that rewrites ~/.codex/config.toml, here returning the fixture's report)
and `hermes_cli.config.load_config` (config read). summary() is the real
dataclass method, and --json mode parses the real printed payload back.
build_codex_runtime_parser is exercised with a real argparse tree and the
resulting sub-command table is compared.
"""
import argparse
import importlib.util
import json
import sys
import types
from dataclasses import fields
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "subcommands" / "codex_runtime.py"
GATE = "w21_codexrunti_5cdf58"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli.codex_runtime_plugin_migration as _migration  # noqa: E402

REPORT_CLS = _migration.MigrationReport
_FIELD_NAMES = {f.name: f for f in fields(REPORT_CLS)}


def load_live():
    spec = importlib.util.spec_from_file_location("codex_runtime_live", SRC)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["codex_runtime_live"] = mod
    spec.loader.exec_module(mod)
    return mod


LIVE = load_live()


def install_stubs(report):
    stub = types.ModuleType("hermes_cli.codex_runtime_plugin_migration")
    stub.MigrationReport = REPORT_CLS

    def _migrate(config, dry_run=False):
        return report
    stub.migrate = _migrate
    sys.modules["hermes_cli.codex_runtime_plugin_migration"] = stub

    cfg = types.ModuleType("hermes_cli.config")
    cfg.load_config = lambda: {}
    sys.modules["hermes_cli.config"] = cfg


def make_report(payload):
    """Fixture fields -> a live MigrationReport.

    `target_path` arrives as the string form (`str(Path)`), which is exactly
    what both projections print; the Path object itself is the filesystem seam
    and never crosses over.
    """
    payload = payload if isinstance(payload, dict) else {}
    kw = {}
    for name in _FIELD_NAMES:
        if name not in payload:
            continue
        kw[name] = payload[name]
    return REPORT_CLS(**kw)


class _Args:
    def __init__(self, **kw):
        for k, v in kw.items():
            setattr(self, k, v)


def dump_parser():
    root = argparse.ArgumentParser(prog="hermes")
    root_sub = root.add_subparsers()
    LIVE.build_codex_runtime_parser(root_sub)
    # build_*_parser attaches exactly one command parser under the caller's
    # subparsers; that parser owns the sub-actions this table describes.
    outer = [a for a in root._actions if isinstance(a, argparse._SubParsersAction)][0]
    owner = list(outer.choices.values())[0]
    action = [a for a in owner._actions if isinstance(a, argparse._SubParsersAction)][0]
    choices = [c.dest for c in action._choices_actions]
    helps = {c.dest: c.help for c in action._choices_actions}
    aliases, subs, seen = {}, {}, {}
    for name, child in action.choices.items():
        if id(child) not in seen:
            seen[id(child)] = name
            owner = owner or child
            options = []
            for a in child._actions:
                if a.dest == "help":
                    continue
                if not a.option_strings:
                    continue
                options.append({
                    "options": list(a.option_strings),
                    "dest": a.dest,
                    "action": type(a).__name__,
                    "default": a.default,
                    "help": a.help,
                })
            func = child._defaults.get("func")
            subs[name] = {"func": func.__name__ if callable(func) else None,
                          "options": options}
        else:
            aliases[name] = seen[id(child)]
    func = owner._defaults.get("func") if owner is not None else None
    return {
        "dest": action.dest,
        "choices": choices,
        "aliases": aliases,
        "helps": helps,
        "func": func.__name__ if callable(func) else None,
        "subs": subs,
    }


def expected(r):
    f = r.get("func")
    if f == "parser":
        return dump_parser()
    report = make_report(r.get("report"))
    if f == "summary":
        return report.summary()
    if f == "migrate":
        install_stubs(report)
        args = _Args(dry_run=bool(r.get("dry_run")), json=bool(r.get("json")))
        import contextlib
        import dataclasses
        import io
        import json
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            rc = LIVE.cmd_codex_runtime_migrate(args)
        text = buf.getvalue()
        if args.json:
            return {"rc": rc, "payload": json.loads(text)}
        assert text.endswith("\n")
        return {"rc": rc, "text": text}
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"migrate", "summary", "parser"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
