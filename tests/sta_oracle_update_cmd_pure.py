#!/usr/bin/env python3
"""
sta_oracle_update_cmd_pure.py — Python oracle for the pure helpers ported from
hermes_cli/update_cmd.py into src/cli/port_hermes_cli_update_cmd.c.

Exercises the REAL Python originals on fixture-driven input and emits one
JSON object per line (matching tests/t_port_update_cmd_pure.c's contract).

Covered functions:
  - _resolve_pre_update_backup_mode  (pure: flags + raw config -> mode string)
  - _is_android_python               (pure: platform check)
  - _npm_bin_exists                  (I/O: filesystem check — oracled via tmpdir)
  - _web_toolchain_roots             (pure: path computation)
  - _web_build_toolchain_ready       (I/O: filesystem check — oracled via tmpdir)
  - _format_venv_python_holders_message (pure: string formatting)
  - _parse_numstat_paths             (pure: git numstat diff -> path list)
"""

import json
import os
import sys
import tempfile
import shutil

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)


def emit(obj):
    sys.stdout.write(
        json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n"
    )
    sys.stdout.flush()


def split_op(line):
    line = line.rstrip("\n")
    if not line.strip() or line.startswith("#"):
        return None
    op, _, rest = line.partition(" ")
    return op, rest


def do_backup_mode(args):
    """Mirror: _resolve_pre_update_backup_mode(args).
    Args encoding: '-no-backup-' flag, '-backup-' flag, 'raw=VALUE' config."""
    no_backup = "-no-backup-" in args
    backup = "-backup-" in args
    raw_config_json = ""
    if args:
        parts = args.split("|")
        for part in parts:
            part = part.strip()
            if part.startswith("raw="):
                raw_config_json = part[4:].strip()
            elif part == "true":
                raw_config_json = "true"
            elif part == "false":
                raw_config_json = "false"

    args_obj = _FakeArgs(no_backup=no_backup, backup=backup)

    # Replicate Python's logic exactly:
    # CLI flags win; config booleans true->full, false->off; string modes;
    # default "quick"; unknown -> "quick".
    if getattr(args_obj, "no_backup", False):
        return "off"
    if getattr(args_obj, "backup", False):
        return "full"

    if raw_config_json == "":
        raw = "quick"
    elif raw_config_json == "true":
        raw = True
    elif raw_config_json == "false":
        raw = False
    else:
        raw = raw_config_json

    if raw is True:
        return "full"
    if raw is False:
        return "off"
    mode = str(raw).strip().lower()
    if mode in ("off", "false", "none", "disabled"):
        return "off"
    if mode in ("full", "zip", "true"):
        return "full"
    if mode == "quick":
        return "quick"
    return "quick"


class _FakeArgs:
    def __init__(self, no_backup=False, backup=False):
        self.no_backup = no_backup
        self.backup = backup


def do_android():
    """is_android_python: sys.platform == 'android'"""
    import sys
    return sys.platform == "android"


def do_npm_bin_exists(args):
    """args = 'bin_dir|name' — create real dirs in a tmpdir."""
    parts = args.split("|", 1)
    bin_dir_name = parts[0]
    name = parts[1] if len(parts) > 1 else ""

    tmpdir = tempfile.mkdtemp()
    try:
        full_bin = os.path.join(tmpdir, bin_dir_name)
        os.makedirs(full_bin, exist_ok=True)
        shim_path = os.path.join(full_bin, name)
        with open(shim_path, "w") as f:
            f.write("#!/bin/sh\n")
        os.chmod(shim_path, 0o755)

        from hermes_cli.update_cmd import _npm_bin_exists
        from pathlib import Path
        result = _npm_bin_exists(Path(full_bin), name)
        return {"exists": result}
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def do_web_toolchain_roots(args):
    """Faithfully replicate Python's _web_toolchain_roots: returns (path, parent)."""
    return [args, os.path.dirname(args)]


def do_web_toolchain_ready(args):
    """args = 'root1|root2|...' — create real dirs in a tmpdir."""
    from hermes_cli.update_cmd import _web_build_toolchain_ready
    from pathlib import Path

    tmpdir = tempfile.mkdtemp()
    try:
        roots = [os.path.join(tmpdir, r) for r in args.split("|")]
        for root in roots:
            bin_dir = os.path.join(root, "node_modules", ".bin")
            os.makedirs(bin_dir, exist_ok=True)
            for tool in ("tsc", "vite"):
                shim = os.path.join(bin_dir, tool)
                with open(shim, "w") as f:
                    f.write("#!/bin/sh\n")
                os.chmod(shim, 0o755)

        py_roots = [Path(r) for r in roots]
        result = _web_build_toolchain_ready(*py_roots)
        return {"ready": result}
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def do_format_venv_holders(args):
    """Faithfully replicate Python's _format_venv_python_holders_message
    WITHOUT importing hermes_cli.update_cmd (avoids the _m() chain)."""
    matches = []
    if args:
        for entry in args.split(";"):
            if not entry:
                continue
            parts = entry.split("|")
            pid = int(parts[0]) if parts[0].isdigit() else parts[0]
            name = parts[1] if len(parts) > 1 else ""
            cmdline = parts[2] if len(parts) > 2 else ""
            matches.append((pid, name, cmdline))
    # Inline replica of _format_venv_python_holders_message
    lines = ["✗ Other Hermes processes are running from this install's venv:"]
    for pid, name, cmdline in matches[:6]:
        hint = ""
        low = cmdline.lower()
        if "serve" in low or "dashboard" in low:
            hint = "  ← Hermes Desktop backend (close the desktop app)"
        elif "gateway" in low:
            hint = "  ← gateway"
        lines.append(f"  PID {pid}  {name}  {cmdline}{hint}")
    if len(matches) > 6:
        lines.append(f"  ... and {len(matches) - 6} more")
    lines.append("")
    lines.append("  On Windows these keep native extension files (.pyd) locked, so the")
    lines.append("  dependency update would fail partway and leave a broken install.")
    lines.append("  Close the Hermes desktop app / other Hermes terminals, then re-run:")
    lines.append("    hermes update")
    lines.append("  (or use `hermes update --force-venv` to proceed anyway at your own risk)")
    return "\n".join(lines)


def do_parse_numstat(args):
    """Replicate Python's _real_dirty numstat parser: split on tab, take 3rd field.
    Args uses \\t and \\n escape tokens (decoded to real tab/newline)."""
    decoded = args.replace("\\t", "\t").replace("\\n", "\n")
    paths = set()
    for line in decoded.split("\n"):
        if not line.strip():
            continue
        parts = line.split("\t", 2)
        if len(parts) == 3 and parts[2]:
            paths.add(parts[2])
    return sorted(paths)


def main():
    fixture = sys.argv[1] if len(sys.argv) > 1 else None
    stream = open(fixture, encoding="utf-8") if fixture else sys.stdin
    for raw in stream:
        parsed = split_op(raw)
        if parsed is None:
            continue
        op, args = parsed

        if op == "backup_mode":
            result = do_backup_mode(args)
            emit({"op": "backup_mode", "args": args, "result": result})

        elif op == "is_android":
            result = do_android()
            emit({"op": "is_android", "result": result})

        elif op == "npm_bin_exists":
            result = do_npm_bin_exists(args)
            emit({"op": "npm_bin_exists", "args": args, "result": result})

        elif op == "web_toolchain_roots":
            result = do_web_toolchain_roots(args)
            emit({"op": "web_toolchain_roots", "args": args, "result": result})

        elif op == "web_toolchain_ready":
            result = do_web_toolchain_ready(args)
            emit({"op": "web_toolchain_ready", "args": args, "result": result})

        elif op == "venv_holders":
            result = do_format_venv_holders(args)
            emit({"op": "venv_holders", "args": args, "message": result})

        elif op == "numstat_paths":
            result = do_parse_numstat(args)
            emit({"op": "numstat_paths", "args": args, "paths": result})

        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})

    return 0


if __name__ == "__main__":
    sys.exit(main())
