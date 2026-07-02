#!/usr/bin/env python3
"""
Coverage gate: parse gcov output and enforce minimum coverage threshold.

Usage: python3 scripts/coverage-gate.py [--threshold PERCENT]

Runs gcov from the project root, parses .gcov files to compute line/branch
coverage, and exits non-zero if below threshold (default: 30% line coverage).
"""

import os
import re
import sys
import subprocess
import glob

SRC_LINE_RE = re.compile(r'Source:(.+)$')
GCOV_LINE_RE = re.compile(r'^    ([-0-9#*]+):  (\d+):')


def run_gcov(source_dir):
    """Run gcov on all .gcda files from the project root."""
    gcda_files = []
    for root, dirs, files in os.walk(source_dir):
        for f in files:
            if f.endswith('.gcda'):
                gcda_files.append(os.path.join(root, f))

    if not gcda_files:
        print("No .gcda files found. Run 'make coverage' first.")
        sys.exit(1)

    print(f"Found {len(gcda_files)} .gcda files")

    # Run gcov from project root — it will write .gcov files to CWD
    for gcda in gcda_files:
        rel = os.path.relpath(gcda, source_dir)
        subprocess.run(
            ['gcov', '-b', rel],
            cwd=source_dir,
            capture_output=True, text=True, timeout=60
        )

    # Collect .gcov files (written to project root)
    gcov_files = glob.glob(os.path.join(source_dir, '*.gcov'))
    print(f"Generated {len(gcov_files)} .gcov files")
    return gcov_files


def parse_gcov(gcov_path):
    """Parse a .gcov file and return (source_rel, executed, total)."""
    executed = 0
    total = 0
    source_rel = None

    with open(gcov_path, 'r', errors='replace') as f:
        for line in f:
            line = line.rstrip('\n')

            # Skip branch/call metadata
            if line.startswith('branch ') or line.startswith('call '):
                continue

            # Read the source file path from header
            sm = SRC_LINE_RE.search(line)
            if sm and source_rel is None:
                source_rel = sm.group(1).strip()

            # Source line coverage
            m = GCOV_LINE_RE.match(line)
            if m:
                count_str = m.group(1).strip()
                if count_str == '-':
                    continue  # non-executable
                total += 1
                if count_str != '#' and not count_str.startswith('#####'):
                    try:
                        if int(count_str) > 0:
                            executed += 1
                    except ValueError:
                        pass

    return source_rel, executed, total


def main():
    threshold = 30.0
    for arg in sys.argv[1:]:
        if arg.startswith('--threshold='):
            threshold = float(arg.split('=')[1])

    source_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(source_dir)

    print(f"=== Coverage Gate ===")
    print(f"Source: {source_dir}")
    print(f"Threshold: {threshold:.1f}% line coverage")

    # Remove old .gcov files
    for f in glob.glob(os.path.join(source_dir, '*.gcov')):
        os.remove(f)

    gcov_files = run_gcov(source_dir)

    total_exec = 0
    total_lines = 0
    file_results = []

    for gcov_path in gcov_files:
        source_rel, exec_lines, total = parse_gcov(gcov_path)
        if not source_rel:
            continue

        # Only analyze source files in src/ and lib/
        if not (source_rel.startswith('src/') or source_rel.startswith('lib/')):
            continue

        if total > 0:
            pct = 100.0 * exec_lines / total
            file_results.append((source_rel, exec_lines, total, pct))
            total_exec += exec_lines
            total_lines += total

    if not file_results:
        print("No instrumented source files found in src/ or lib/.")
        sys.exit(1)

    file_results.sort(key=lambda x: x[3])

    print(f"\n{'File':<60} {'Covered':>8} {'Total':>8} {'%':>8}")
    print('-' * 88)
    for rel, exec_lines, total, pct in file_results:
        print(f"{rel[:58]:<60} {exec_lines:>8} {total:>8} {pct:>7.1f}%")

    print('-' * 88)
    overall_pct = 100.0 * total_exec / total_lines if total_lines > 0 else 0
    print(f"{'TOTAL':<60} {total_exec:>8} {total_lines:>8} {overall_pct:>7.1f}%")

    below_50 = sum(1 for _, _, _, p in file_results if p < 50.0)
    above_80 = sum(1 for _, _, _, p in file_results if p >= 80.0)
    print(f"\n  Files below 50%: {below_50}")
    print(f"  Files above 80%: {above_80}")

    if overall_pct < threshold:
        print(f"\n❌ FAIL: Coverage {overall_pct:.1f}% below threshold {threshold:.1f}%")
        sys.exit(1)
    else:
        print(f"\n✅ PASS: Coverage {overall_pct:.1f}% meets threshold {threshold:.1f}%")
        sys.exit(0)


if __name__ == '__main__':
    main()
