#!/usr/bin/env python3
"""
Batch PoP annotation tool — adds PoP comments to C files for PARTIAL matches.

Reads the parity scanner's JSON output and inserts PoP annotations before
C function definitions that already exist but lack proper PoP comments.

Usage:
  python3 tests/batch_pop_annotate.py [--dry-run] [--agent-only] [--check]
"""

import json
import re
import sys
from pathlib import Path
from collections import defaultdict

SLERMES_DIR = Path("/home/wubu/hermes-agent-dev/slermes")


def run_scanner():
    """Run the parity scanner and return JSON output."""
    import subprocess
    result = subprocess.run(
        ["python3", "tests/slermes_parity_battleground.py", "--json"],
        capture_output=True, text=True, timeout=120,
        cwd=str(SLERMES_DIR)
    )
    return json.loads(result.stdout)


def find_function_line(content, func_name):
    """Find the line number of a function definition in C content.

    Avoids matching inside comments and strings.
    """
    pattern = re.compile(
        rf'^(?:static\s+)?(?:const\s+)?(?:__attribute__\(\s*unused\s*\)\s+)?'
        rf'(?:\w+\s+)*(?:\*\s*)?{re.escape(func_name)}\s*\(',
        re.MULTILINE
    )
    for match in pattern.finditer(content):
        line_num = content[:match.start()].count('\n') + 1
        # Check this line isn't inside a comment
        line_start = content.rfind('\n', 0, match.start()) + 1
        line_text = content[line_start:match.end()]
        # Skip if line starts with // or is inside /* */
        stripped = line_text.lstrip()
        if stripped.startswith('//') or stripped.startswith('*') or stripped.startswith('/*'):
            continue
        # Check for /* */ on preceding lines
        prefix = content[:match.start()]
        # Simple check: look for unclosed /* before this position
        last_open = prefix.rfind('/*')
        last_close = prefix.rfind('*/')
        if last_open > last_close:
            continue  # Inside a block comment
        return line_num
    return None


def has_pop_nearby(content, line_num, lookback=10):
    """Check if there's already a PoP annotation within lookback lines."""
    lines = content.split('\n')
    start = max(0, line_num - lookback)
    for i in range(start, min(line_num, len(lines))):
        if 'Port of Python' in lines[i]:
            return True
    return False


def is_inside_comment_block(content, line_num):
    """Check if the given line is inside a /* */ block comment."""
    lines = content.split('\n')
    in_block = False
    for i in range(line_num - 1):  # 0-indexed, lines before target
        line = lines[i]
        # Handle /* and */ on same line
        pos = 0
        while pos < len(line):
            if not in_block:
                idx = line.find('/*', pos)
                if idx == -1:
                    break
                in_block = True
                pos = idx + 2
            else:
                idx = line.find('*/', pos)
                if idx == -1:
                    break
                in_block = False
                pos = idx + 2
    return in_block


def insert_pop_annotation(content, line_num, py_file, py_func):
    """Insert a PoP comment safely before the given line.

    If there's already a comment block immediately before the function,
    insert the PoP line at the start of the block.
    Otherwise, insert a new comment line.
    """
    lines = content.split('\n')
    idx = line_num - 1  # 0-indexed

    pop_line = f"/* Port of Python {py_file}:{py_func}(). */"

    # Check if we're right after a comment block end
    prev_idx = idx - 1
    while prev_idx >= 0 and lines[prev_idx].strip() == '':
        prev_idx -= 1

    if prev_idx >= 0 and lines[prev_idx].strip().endswith('*/'):
        # Find the start of this comment block
        block_end = prev_idx
        depth = 0
        block_start = block_end
        for i in range(block_end, -1, -1):
            line = lines[i]
            if '*/' in line:
                depth += line.count('*/')
            if '/*' in line:
                depth -= line.count('/*')
                if depth <= 0:
                    block_start = i
                    break

        # Check if PoP already exists in this block
        block_text = '\n'.join(lines[block_start:block_end+1])
        if 'Port of Python' in block_text:
            return content  # Already annotated

        # Insert PoP at the start of the comment block
        lines.insert(block_start, pop_line)
    else:
        # Insert new comment line before the function
        # But first check we're not inside a comment block
        if is_inside_comment_block(content, line_num):
            return content  # Can't safely insert
        lines.insert(idx, pop_line)

    return '\n'.join(lines)


def main():
    dry_run = '--dry-run' in sys.argv
    agent_only = '--agent-only' in sys.argv
    check_mode = '--check' in sys.argv

    if check_mode:
        # Just check if the build compiles
        import subprocess
        result = subprocess.run(
            ["make", "-j4"],
            capture_output=True, text=True, timeout=120,
            cwd=str(SLERMES_DIR)
        )
        errors = [l for l in result.stderr.split('\n') if 'error:' in l]
        if errors:
            print(f"Build errors ({len(errors)}):")
            for e in errors[:10]:
                print(f"  {e}")
        else:
            print("Build OK")
        return

    print("Running parity scanner...")
    data = run_scanner()

    # Collect PARTIAL matches
    partials = []
    seen = set()  # Deduplicate
    for name, mod in data['modules'].items():
        if agent_only and not name.startswith('agent/'):
            continue
        for g in mod['gaps']:
            if g['classification'] == 'PARTIAL':
                c_file = g.get('c_location', '')
                c_func = g.get('c_function', '')
                if c_file and c_func:
                    key = (name, c_func)
                    if key not in seen:
                        seen.add(key)
                        partials.append({
                            'py_file': name,
                            'py_func': g['python_feature']['name'],
                            'c_file': c_file,
                            'c_func': c_func,
                        })

    print(f"Found {len(partials)} unique PARTIAL matches with C locations")

    # Group by C file
    by_c_file = defaultdict(list)
    for p in partials:
        by_c_file[p['c_file']].append(p)

    print(f"Across {len(by_c_file)} C files")

    # Process each C file
    annotated = 0
    skipped = 0
    errors = []

    for c_file, entries in sorted(by_c_file.items()):
        fpath = SLERMES_DIR / c_file
        if not fpath.exists():
            errors.append(f"File not found: {c_file}")
            continue

        content = fpath.read_text()
        original = content
        modified = False

        for entry in entries:
            py_file = entry['py_file']
            py_func = entry['py_func']
            c_func = entry['c_func']

            # Find the function line
            line_num = find_function_line(content, c_func)
            if line_num is None:
                # Try with underscore prefix
                line_num = find_function_line(content, '_' + c_func)
                if line_num is None:
                    continue  # Skip silently — function may have been removed

            # Check if already annotated
            if has_pop_nearby(content, line_num):
                skipped += 1
                continue

            # Check if inside a comment block (can't safely annotate)
            if is_inside_comment_block(content, line_num):
                errors.append(f"  {c_func} in {c_file} is inside a comment block")
                continue

            # Insert annotation
            new_content = insert_pop_annotation(content, line_num, py_file, py_func)
            if new_content != content:
                content = new_content
                modified = True
                annotated += 1
            else:
                skipped += 1

        if modified and not dry_run:
            fpath.write_text(content)
            print(f"  Updated: {c_file}")

    print(f"\nResults:")
    print(f"  Annotations added: {annotated}")
    print(f"  Already annotated (skipped): {skipped}")
    print(f"  Errors: {len(errors)}")

    if errors:
        print(f"\nWarnings:")
        for e in errors[:20]:
            print(f"  {e}")

    if dry_run:
        print(f"\n[DRY RUN — no files modified]")


if __name__ == "__main__":
    main()
