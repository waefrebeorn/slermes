#!/usr/bin/env python3
"""Devil's-advocate gate: find PoP-credited ports that cannot execute.

A PoP annotation is a claim. If the C function it names is `static` and the
translation unit exports no other entry point, the object contributes nothing
to the binary: the linker drops it. Wiring such an object into objects.mk
raises the scanner count without moving runtime behavior one byte.

This script does source-level triage (fast, no build) and can be tightened
with nm later. Report mode exits 0; --fail-on-wired-dead exits 1 when a
wired object is dead, so it can become a CI gate (DA-5 built-link criterion).
"""
import argparse
import json
import re
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
POP = re.compile(r'/\*\s*PoP:\s*([A-Za-z_]\w*)\s*@\s*([^\s:]+):([^\s*]+)\s*\*/')
DEFN = r'^[A-Za-z_][\w\s\*]*?\b{name}\s*\([^;]*?\)\s*\{{'


def is_static(src: str, name: str) -> bool | None:
    """Locate a definition and report whether it is static. None = not found."""
    m = re.search(DEFN.format(name=re.escape(name)), src, re.M)
    if not m:
        return None
    line_start = src.rfind('\n', 0, m.start()) + 1
    head = src[line_start:m.start()]
    return bool(re.search(r'\bstatic\b', head)) or bool(re.search(r'\bstatic\b', m.group(0).split(name)[0]))


def has_external_entry(src: str) -> bool:
    """Any non-static top-level function definition in the file."""
    for m in re.finditer(r'^([A-Za-z_][\w\s\*]*?)\b([A-Za-z_]\w*)\s*\([^;]*?\)\s*\{', src, re.M):
        decl = m.group(1)
        if 'static' not in decl and 'typedef' not in decl:
            return True
    return False


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--fail-on-wired-dead', action='store_true')
    ap.add_argument('--json-out', default=None)
    args = ap.parse_args()

    objects = set()
    obj_mk = ROOT / 'build' / 'objects.mk'
    if obj_mk.exists():
        objects = set(re.findall(r'[\w./\\-]+\.o', obj_mk.read_text(encoding='utf-8', errors='replace')))

    files, dead, wired_dead, unresolved = 0, [], [], []
    for src in list((ROOT / 'src').rglob('*.c')) + list((ROOT / 'lib').rglob('*.c')):
        text = src.read_text(encoding='utf-8', errors='replace')
        claims = POP.findall(text)
        if not claims:
            continue
        files += 1
        rel = src.relative_to(ROOT).as_posix()
        flags = [is_static(text, name) for name, _, _ in claims]
        missing = sum(f is None for f in flags)
        static_only = bool(flags) and all(f is True for f in flags)
        if missing:
            unresolved.append((rel, missing, len(flags)))
        if static_only and not has_external_entry(text):
            obj = rel[:-2] + '.o'
            rec = {'file': rel, 'claims': len(flags), 'wired': obj in objects}
            dead.append(rec)
            if obj in objects:
                wired_dead.append(rec)

    dead.sort(key=lambda r: (-r['claims'], r['file']))
    print(f'PoP-tagged C files: {files}')
    print(f'dead (all PoP fns static, no external entry): {len(dead)}')
    print(f'  of those wired into objects.mk: {len(wired_dead)}')
    print(f'claims with unresolved definitions: {sum(n for _, n, _ in unresolved)} '
          f'in {len(unresolved)} files')
    print('\nTop dead files by claim count:')
    for r in dead[:25]:
        print(f"  {r['claims']:3d} {'WIRED' if r['wired'] else '     '}  {r['file']}")
    by_dir = Counter(r['file'].split('/')[1] if '/' in r['file'] else r['file'] for r in dead)
    print('\nBy directory:', dict(by_dir.most_common(12)))

    if args.json_out:
        Path(args.json_out).write_text(json.dumps({
            'pop_files': files, 'dead': dead, 'wired_dead': wired_dead,
            'unresolved': unresolved}, indent=2), encoding='utf-8')
    return 1 if (args.fail_on_wired_dead and wired_dead) else 0


if __name__ == '__main__':
    raise SystemExit(main())
