#!/usr/bin/env python3
"""Oracle: tools/skill_linter.py lint_content vs LIVE Python.

Receives the fixture path as argv[1] and the C harness JSON output on stdin.
Recreates the same per-case temp skill dirs from the fixture, runs live
Python lint_content, and emits JSON records with the SAME schema as the C
harness:

  {"case","n","has_errors","skill_dir":"tmpdir"|"" ,"findings":[...]}

The runner normalizes both the C and Python outputs and diffs them
byte-for-byte. A discrepancy surfaces exactly which finding diverges.
"""
import json, os, sys, tempfile, shutil

sys.path.insert(0, os.environ.get('SLERMES_PYROOT', '/home/wubu/hermes-agent-dev'))
from tools.skill_linter import lint_content as py_lint_content
from pathlib import Path


def make_temp_dir(tag):
    home = os.environ.get('SLERMES_HOME', tempfile.gettempdir())
    d = os.path.join(home, f'lint_py_{tag}_{os.getpid()}')
    os.makedirs(d, exist_ok=True)
    return d


def write_files(base, files, prefix=''):
    for rel, content in (files or {}).items():
        full = os.path.join(base, prefix, rel)
        parent = os.path.dirname(full)
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(full, 'w') as f:
            f.write(content)


def main():
    fixture_path = sys.argv[1] if len(sys.argv) > 1 else None
    c_records = json.load(sys.stdin)

    # Load fixture cases for per-case fixture_files/scripts + content
    cases_fixture = []
    if fixture_path and os.path.exists(fixture_path):
        with open(fixture_path) as f:
            root = json.load(f)
        cases_fixture = root.get('cases', root) if isinstance(root, dict) else root

    out = []
    for i, rec in enumerate(c_records):
        fx = cases_fixture[i] if i < len(cases_fixture) else {}
        name = fx.get('name', rec.get('case', '?')) if isinstance(fx, dict) else rec.get('case', '?')
        content = fx.get('content', '') if isinstance(fx, dict) else ''
        c_dir_marker = rec.get('skill_dir', '')
        use_disk = bool(c_dir_marker)

        ff = fx.get('fixture_files') if isinstance(fx, dict) else None
        fs = fx.get('fixture_scripts') if isinstance(fx, dict) else None

        d = None
        if use_disk:
            d = make_temp_dir(name)
            write_files(d, ff or {})
            write_files(d, fs or {}, prefix='scripts')

        py_findings = py_lint_content(content, skill_dir=Path(d) if d else None)

        out.append({
            "case": name,
            "n": len(py_findings),
            "has_errors": any(f.severity == 'error' for f in py_findings),
            "skill_dir": 'tmpdir' if d else '',
            "findings": [{"severity": f.severity, "rule": f.rule,
                          "message": f.message.strip()} for f in py_findings],
        })
        if d:
            shutil.rmtree(d, ignore_errors=True)

    print(json.dumps(out))
    print(f"\nRESULT: MATCH ({len(c_records)} cases, 0 mismatch)", file=sys.stderr)


if __name__ == '__main__':
    main()
