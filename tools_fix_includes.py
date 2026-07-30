#!/usr/bin/env python3
"""
Systematic missing-include fixer for the slermes C11 tree.

For every src/**/*.c:
  1. gcc -fsyntax-only with the project's real include flags + -DHERMES_VERSION.
  2. Parse "implicit declaration of function '<sym>'" (and "unknown type name
     '<sym>'" for typedefs) errors.
  3. Map each missing symbol -> the header under include/ that declares it
     (grep for the symbol as a declaration).
  4. Insert the missing header include near the top of the file (after the
     existing #include "hermes_core_types.h" block, before <system> includes).
  5. Retry until no progress or clean.

This is mechanical and verified by the compiler gate. It only *adds* includes;
it never rewrites logic. The goal: a clean `make slermes`.
"""
import os, re, subprocess, sys

ROOT = "/home/wubu/hermes-agent-dev/slermes"
INC_FLAGS = ("-I include -I src -I lib "
             + " ".join("-I" + d for d in sorted(
                 [os.path.join(ROOT,"lib",x) for x in os.listdir(ROOT+"/lib")
                  if os.path.isdir(os.path.join(ROOT,"lib",x))]))
             + " "
             + " ".join("-I" + d for d in sorted(
                 [os.path.join(ROOT,"lib",x,"include") for x in os.listdir(ROOT+"/lib")
                  if os.path.isdir(os.path.join(ROOT,"lib",x,"include"))])))
VER_DEF = '-DHERMES_VERSION=0.19.0-slermes -DHERMES_RELEASE_DATE=2026.7.20'

# Headers we must not auto-add (umbrella pulls them; avoid duplicate/clash noise)
SKIP_HEADERS = set()

def find_header_for_symbol(sym):
    """Return the include form for the header under include/ or lib/ that
    declares `sym`, or None. Emits 'hermes_x.h' (relative to include/) or
    'libyaml/yaml.h' (relative to lib/), matching the build's -I flags."""
    pats = [
        r'\b'+re.escape(sym)+r'\s*\(',                 # function call/decl
        r'typedef\b.*\b'+re.escape(sym)+r'\b',          # typedef
        r'struct\s+'+re.escape(sym)+r'\b',
        r'union\s+'+re.escape(sym)+r'\b',
        r'enum\s+'+re.escape(sym)+r'\b',
        r'#define\s+'+re.escape(sym)+r'\b',
        r'\b'+re.escape(sym)+r'\s',
    ]
    best = None
    best_rel = None
    for base in ("include", "lib"):
        for hr, _, files in os.walk(os.path.join(ROOT, base)):
            for fn in files:
                if not fn.endswith(".h"):
                    continue
                path = os.path.join(hr, fn)
                rel = os.path.relpath(path, os.path.join(ROOT, base))
                try:
                    with open(path, "r", errors="ignore") as f:
                        txt = f.read()
                except Exception:
                    continue
                for p in pats:
                    found = False
                    for mm in re.finditer(p, txt):
                        # ignore matches inside a comment line
                        line_start = txt.rfind("\n", 0, mm.start()) + 1
                        line = txt[line_start:mm.end()]
                        if "/*" in line or "*/" in line:
                            continue
                        found = True
                        break
                    if found:
                        if best is None or (best_rel is not None and len(rel) < len(best_rel)):
                            best = path
                            best_rel = rel
                        break
    return best_rel  # e.g. 'hermes_x.h' or 'libyaml/yaml.h' or None

def missing_symbols(srcpath):
    cmd = ["gcc", "-fsyntax-only", "-Werror=implicit-function-declaration"]
    cmd += VER_DEF.split()
    cmd += INC_FLAGS.split()
    cmd += [srcpath]
    r = subprocess.run(cmd, capture_output=True, text=True)
    syms = set()
    for line in r.stderr.splitlines():
        m = re.search(r"implicit declaration of (?:function|variable) [‘']([^’']+)[’']", line)
        if m:
            syms.add(m.group(1))
            continue
        m = re.search(r"unknown type name [‘']([^’']+)[’']", line)
        if m:
            syms.add(m.group(1))
            continue
        m = re.search(r"implicit declaration of function [‘']([^’']+)[’']", line)
        if m:
            syms.add(m.group(1))
    return syms

def insert_include(srcpath, header_rel):
    with open(srcpath) as f:
        lines = f.readlines()
    # Insert BEFORE the first #include line (keeps the new include at the top
    # of the include block, before any usage; outside a leading comment block
    # because the first #include sits after it).
    ins = None
    for i, ln in enumerate(lines):
        if ln.strip().startswith('#include'):
            ins = i
            break
    if ins is None:
        ins = 0
    inc_line = '#include "%s"\n' % header_rel
    # avoid duplicates
    if any(inc_line.strip() == l.strip() for l in lines):
        return False
    lines.insert(ins, inc_line)
    with open(srcpath, "w") as f:
        f.writelines(lines)
    return True

def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 999999
    targets = []
    for hr, _, files in os.walk(os.path.join(ROOT, "src")):
        for fn in files:
            if fn.endswith(".c"):
                targets.append(os.path.join(hr, fn))
    targets.sort()
    fixed_files = 0
    added_total = 0
    for src in targets[:limit]:
        rel = os.path.relpath(src, ROOT)
        added = 0
        for _ in range(15):  # bounded retries
            syms = missing_symbols(src)
            if not syms:
                break
            # pick a symbol not yet resolved, map to header
            did = False
            for sym in sorted(syms):
                hdr = find_header_for_symbol(sym)
                if hdr and hdr not in SKIP_HEADERS:
                    if insert_include(src, hdr):
                        added += 1
                        did = True
                        break
            if not did:
                break
        if added:
            fixed_files += 1
            added_total += added
            print(f"FIXED {rel}: +{added} includes")
    print(f"\nDONE: {fixed_files} files touched, {added_total} includes added")

if __name__ == "__main__":
    main()
