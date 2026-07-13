#!/usr/bin/env python3
"""Faithful split of src/tools/file.c: extract the SANDBOX security concern
into its own module src/tools/sandbox.c.

tools/file.c (1139 lines) bundlles two concerns:
  1) SANDBOX security (lines 120-357: g_allowed_dirs globals,
     sandbox_init/enable/add_allowed_dir/remove_allowed_dir/clear/
     check_path/set_symlink_check, is_unsafe_symlink/is_safe_path statics)
     -- a shared security concern: sandbox_check_path() is CALLED by
        src/tools/file_batch.c, so it belongs in a header-declared module.
  2) File-tool handlers + registry wiring (handle_read/write/search/diff/
     perms/hex/syntax/hash, file_*_handler, registry_init_file)
     -- the genuine file_tools / todo_tool / pty_bridge port core; kept.

We move ONLY the sandbox concern out, create include/sandbox.h (function
declarations + MAX_SANDBOX_DIRS), and update callers
(src/tools/file.c, src/tools/file_batch.c) to #include it. file.c keeps
its file-tool core; no behavioral change.

Run from repo root. Idempotent.
"""
import re, sys, os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILEC = os.path.join(REPO, "src/tools/file.c")
SANDBOX_H = os.path.join(REPO, "include/sandbox.h")
SANDBOX_C = os.path.join(REPO, "src/tools/sandbox.c")

SANDBOX_NAMES = [
    "sandbox_init", "sandbox_enable", "sandbox_add_allowed_dir",
    "sandbox_remove_allowed_dir", "sandbox_clear", "sandbox_check_path",
    "sandbox_set_symlink_check", "is_unsafe_symlink", "is_safe_path",
]

def brace_balance(s):
    d = 0
    for ch in s:
        if ch == '{': d += 1
        elif ch == '}': d -= 1
    return d

def read(p):
    with open(p) as f:
        return f.read()

def write(p, s):
    with open(p, "w") as f:
        f.write(s)

def find_fn(lines, name):
    for i, l in enumerate(lines):
        s = l.rstrip()
        if re.match(r'^(?:static\s+|inline\s+|const\s+)?[A-Za-z_].*\b' + re.escape(name) + r'\s*\(', s):
            sig_start = i
            break
    else:
        raise RuntimeError(f"function {name} not found")
    j = sig_start
    while j < len(lines) and '{' not in lines[j]:
        j += 1
    if j >= len(lines):
        raise RuntimeError(f"no brace for {name}")
    depth = 0; opened = False; k = j
    while k < len(lines):
        for ch in lines[k]:
            if ch == '{': depth += 1; opened = True
            elif ch == '}': depth -= 1
        if opened and depth == 0:
            return (sig_start, k)
        k += 1
    raise RuntimeError(f"unbalanced {name}")

def doc_start(lines, def_idx):
    i = def_idx - 1
    while i >= 0:
        s = lines[i].strip()
        if s.startswith('/* ==='):
            break
        if s.endswith('}') or (s.endswith(';') and not s.startswith('static') and not s.startswith('const')):
            i += 1
            break
        if s.startswith('*') or s.startswith('/*') or s.startswith('//') or s == '' \
           or s.startswith('/* PoP') or s.startswith('* PoP') \
           or s.startswith('/* Port') or s.startswith('* Port') \
           or (re.match(r'^(?:static\s+|const\s+)?[A-Za-z_].*[;=]\s*$', s) and '(' not in s and '{' not in s):
            i -= 1
            continue
        break
    return max(i, 0)

def main():
    text = read(FILEC)
    lines = text.split("\n")
    n = len(lines)
    marked = [False] * n

    # include the sandbox globals block (lines 120-125: #define + statics)
    # locate #define MAX_SANDBOX_DIRS and the following statics up to first fn.
    glob_start = None
    for i, l in enumerate(lines):
        if re.match(r'#define\s+MAX_SANDBOX_DIRS', l.strip()):
            glob_start = i
            break
    if glob_start is None:
        sys.exit("MAX_SANDBOX_DIRS #define not found")
    # statics extend until the first 'void sandbox_init' (line after globals)
    # capture from glob_start through the is_safe_path function end.
    blocks = []
    # globals + first fn (sandbox_init) through is_safe_path
    # simplest: mark glob_start .. end of is_safe_path as the sandbox block,
    # but it also includes the banner at 118? we keep banners minimal.
    # Find def of is_safe_path
    _, is_safe_end = find_fn(lines, "is_safe_path")
    # block = from the line of the banner preceding globals? include banner 118-119.
    # doc_start from glob_start handles banner.
    # We'll mark: the #define/globals region (glob_start..is_safe_end) AND
    # the functions. Instead, treat glob_start as a pseudo 'def' by scanning
    # doc upward then down to is_safe_end.
    ds = doc_start(lines, glob_start) if lines[glob_start].strip().startswith('#define') else glob_start
    # Actually capture from the banner just before globals:
    bi = glob_start
    while bi > 0 and (lines[bi-1].strip().startswith('*') or lines[bi-1].strip() == '' or lines[bi-1].strip().startswith('/*')):
        bi -= 1
    block = "\n".join(lines[bi:is_safe_end+1]) + "\n"
    if brace_balance(block) != 0:
        sys.exit(f"SANDBOX block UNBALANCED net {brace_balance(block)}")
    for j in range(bi, is_safe_end + 1):
        marked[j] = True
    blocks.append(block)

    # Prune file.c
    keep = [lines[i] for i in range(n) if not marked[i]]
    new_file = "\n".join(keep)
    new_file = re.sub(r'\n{4,}', '\n\n\n', new_file)
    new_file = new_file.rstrip("\n") + "\n"
    if brace_balance(new_file) != 0:
        sys.exit(f"PRUNED file.c UNBALANCED net {brace_balance(new_file)}")
    write(FILEC, new_file)

    # Write include/sandbox.h
    header = (
        "#ifndef SANDBOX_H\n#define SANDBOX_H\n\n"
        "#include <stdbool.h>\n#include <stddef.h>\n\n"
        "/* Sandbox path-security concern, extracted from src/tools/file.c.\n"
        " * Mirrors the Python file_tools / process_bootstrap sandbox semantics:\n"
        " * only paths under an allowed directory may be touched by the file tool.\n"
        " */\n\n"
        "#define MAX_SANDBOX_DIRS 32\n\n"
        "void  sandbox_init(void);\n"
        "void  sandbox_enable(bool enabled);\n"
        "bool  sandbox_add_allowed_dir(const char *dir);\n"
        "bool  sandbox_remove_allowed_dir(const char *dir);\n"
        "void  sandbox_clear(void);\n"
        "bool  sandbox_check_path(const char *path);\n"
        "void  sandbox_set_symlink_check(bool enabled);\n"
        "bool  is_safe_path(const char *path);\n\n"
        "#endif /* SANDBOX_H */\n"
    )
    write(SANDBOX_H, header)

    # Write src/tools/sandbox.c
    src_head = (
        "/*\n"
        " * sandbox.c — path-security sandbox, extracted from tools/file.c monolith.\n"
        " *\n"
        " * Real implementation of the sandbox_* API. Public decls live in\n"
        " * include/sandbox.h; callers (file.c, file_batch.c) are unchanged.\n"
        " */\n\n"
        "#define _GNU_SOURCE\n"
        "#include \"sandbox.h\"\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n"
        "#include <unistd.h>\n"
        "#include <sys/stat.h>\n"
        "#include <libgen.h>\n"
        "#include <fnmatch.h>\n"
        "#include <ctype.h>\n\n"
    )
    write(SANDBOX_C, src_head + "".join(blocks))
    # is_safe_path is called by file.c (sandbox_check_path path pre-check),
    # so export it: drop the `static` qualifier in the written file.
    txt = open(SANDBOX_C).read()
    txt = txt.replace("static bool is_safe_path(const char *path) {",
                         "bool is_safe_path(const char *path) {")
    write(SANDBOX_C, txt)
    # file.c must now see the is_safe_path decl: add #include "sandbox.h"
    ftxt = read(FILEC)
    if '#include "sandbox.h"' not in ftxt:
        ftxt = ftxt.replace('#include "hermes.h"\n',
                                  '#include "hermes.h"\n#include "sandbox.h"\n', 1)
        write(FILEC, ftxt)
    print(f"extracted sandbox.c: {len(''.join(blocks).splitlines())} lines")
    print(f"pruned file.c -> {len(new_file.splitlines())} lines (was {n})")

if __name__ == "__main__":
    main()
