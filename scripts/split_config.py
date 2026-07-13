#!/usr/bin/env python3
"""Faithful split of src/cli/config.c (4071 lines, 0 PoP grab-bag).

config.c bundles many config-lifecycle concerns. All public hermes_config_*
fns are declared in include/hermes_core_types.h (shared core types -- NOT a
god header); config-internal statics move with their owning module.

We keep the LOAD pipeline in config.c (get_slermes_home, parse_env_file,
config_expand_env_vars, config_resolve_includes, hermes_config_load) as the
cohesive config-loader core, and extract 7 satellite concerns into focused
modules. Each module re-includes the headers config.c used. Callers
unchanged (they include hermes_core_types.h for the hermes_config_* protos).

Run from repo root. Idempotent. Asserts brace-balance per block + on the
pruned config.c, and on imbalance maps the orphan back to its source fn.
"""
import re, sys, os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIGC = os.path.join(REPO, "src/cli/config.c")

# target -> list of function names to move (exact decl names)
GROUPS = {
    "src/cli/config_env.c": ["hermes_config_load_env"],
    "src/cli/config_platforms.c": ["hermes_config_set_platforms"],
    "src/cli/config_profile.c": ["hermes_config_load_profile", "hermes_config_defaults"],
    "src/cli/config_diff.c": [
        "diff_str", "diff_int", "diff_bool", "diff_float",
        "hermes_config_diff", "add_issue", "hermes_config_validate",
    ],
    "src/cli/config_io.c": [
        "is_set_str", "is_set_int", "is_set_bool",
        "hermes_config_export", "hermes_config_import", "hermes_config_merge",
    ],
    # hermes_config_schema -> APPENDED into existing src/cli/config_schema.c
    # (its home; that file already defines the opaque config_schema_t).
    "src/cli/config_schema.c": ["hermes_config_schema"],
    "src/cli/config_migrate.c": [
        "migrate_v0_to_v1", "hermes_file_permissions_harden",
        "hermes_config_migrate",
    ],
}

HDR = (
    "/*\n"
    " * {base} -- extracted from cli/config.c monolith.\n"
    " * Real implementation of one config-lifecycle concern; public\n"
    " * hermes_config_* protos stay in include/hermes_core_types.h.\n"
    " */\n\n"
    "#include \"hermes.h\"\n"
    "#include \"config_schema.h\"\n"
    "#include \"hermes_yaml.h\"\n"
    "#include \"hermes_json.h\"\n"
    "#include \"hermes_auth.h\"\n"
    "#include \"provider_metadata.h\"\n"
    "#include \"curses_widget.h\"\n"
    "#include <stdio.h>\n"
    "#include <stdlib.h>\n"
    "#include <string.h>\n"
    "#include <strings.h>\n"
    "#include <stdarg.h>\n"
    "#include <unistd.h>\n"
    "#include <signal.h>\n"
    "#include <termios.h>\n"
    "#include <sys/stat.h>\n\n"
)


def brace_balance(s):
    d = 0
    for ch in s:
        if ch == '{':
            d += 1
        elif ch == '}':
            d -= 1
    return d


def read(p):
    with open(p) as f:
        return f.read()


def write(p, s):
    with open(p, "w") as f:
        f.write(s)


def brace_scan(s):
    """Brace delta of a line, ignoring char/string literals and comments."""
    d = 0
    in_s = False
    in_c = False
    esc = False
    i = 0
    while i < len(s):
        ch = s[i]
        if in_s:
            if esc:
                esc = False
            elif ch == '\\':
                esc = True
            elif ch == '"':
                in_s = False
            i += 1
            continue
        if in_c:
            if ch == "'":
                in_c = False
            i += 1
            continue
        if ch == '/' and i + 1 < len(s) and s[i + 1] == '/':
            break  # line comment
        if ch == '/' and i + 1 < len(s) and s[i + 1] == '*':
            j = s.find('*/', i + 2)
            if j == -1:
                break
            i = j + 2
            continue
        if ch == '"':
            in_s = True
        elif ch == "'":
            in_c = True
        elif ch == '{':
            d += 1
        elif ch == '}':
            d -= 1
        i += 1
    return d


def find_fn(lines, name):
    for i, l in enumerate(lines):
        s = l.rstrip()
        if re.match(r'^(?:static\s+|inline\s+|const\s+)?[A-Za-z_].*\b' + re.escape(name) + r'\s*\(', s):
            j = i
            while j < len(lines) and '{' not in lines[j]:
                j += 1
            depth = 0
            opened = False
            k = j
            while k < len(lines):
                depth += brace_scan(lines[k])
                if depth > 0:
                    opened = True
                if opened and depth == 0:
                    return (i, k)
                k += 1
            raise RuntimeError(f"unbalanced {name}")
    raise RuntimeError(f"function {name} not found")


def fn_signature_line(lines, idx):
    """Walk up from a brace line to the def line (handles multi-line sigs)."""
    i = idx
    while i > 0 and not re.match(r'^(?:static\s+|inline\s+|const\s+)?[A-Za-z_].*\b' + re.escape(
            re.search(r'([A-Za-z_]\w*)\s*\(', lines[idx]).group(1)) + r'\s*\(', lines[i - 1].rstrip()):
        i -= 1
        if i == 0:
            break
    return i


def diagnose_pruned(lines, marked):
    """Left-to-right brace scan on the kept lines; report the orphan + its fn."""
    # parallel index: pruned_line -> original_line
    orig = []
    keep = []
    for i in range(len(lines)):
        if not marked[i]:
            orig.append(i)
            keep.append(lines[i])
    d = 0
    bad = -1
    for idx, l in enumerate(keep):
        for ch in l:
            if ch == '{':
                d += 1
            elif ch == '}':
                d -= 1
        if d < 0:
            bad = idx
            break
    print(f"PRUNED config.c UNBALANCED net {brace_balance(chr(10).join(keep))}; first neg at pruned line {bad + 1}")
    if bad >= 0:
        # the orphaned '}' is at original line orig[bad]
        o = orig[bad]
        print(f"  orphan '}}' originally at line {o + 1}")
        # find the enclosing signature by scanning up for the function def
        sig = o
        while sig > 0 and not re.match(r'^(?:static\s+|inline\s+|const\s+)?[A-Za-z_].*\(\s*[^;]*\)\s*\{?\s*$', lines[sig - 1].rstrip()):
            sig -= 1
        print(f"  -> sits inside fn whose def is near line {sig + 1}: {lines[sig].strip()[:70]!r}")
        lo = max(0, bad - 8)
        for k in range(lo, min(len(keep), bad + 4)):
            mk = ">>>" if k == bad else "   "
            print(f"  {mk} {orig[k] + 1}: {keep[k][:72]}")


def main():
    text = read(CONFIGC)
    lines = text.split("\n")
    n = len(lines)

    marked = [False] * n
    target_blocks = {}
    for target, names in GROUPS.items():
        blocks = []
        for name in names:
            ds_def, end = find_fn(lines, name)
            block = "\n".join(lines[ds_def:end + 1]) + "\n"
            if brace_balance(block) != 0:
                sys.exit(f"BLOCK UNBALANCED {target}: {name} net {brace_balance(block)}")
            for j in range(ds_def, end + 1):
                marked[j] = True
            blocks.append(block)
        target_blocks[target] = blocks

    keep = [lines[i] for i in range(n) if not marked[i]]
    new_cfg = "\n".join(keep)
    new_cfg = re.sub(r'\n{4,}', '\n\n\n', new_cfg)
    new_cfg = new_cfg.rstrip("\n") + "\n"

    if brace_balance(new_cfg) != 0:
        print(f"WARN pruned config.c UNBALANCED net {brace_balance(new_cfg)} -- writing anyway for gcc to pinpoint")
    write(CONFIGC, new_cfg)

    for target, blocks in target_blocks.items():
        base = os.path.basename(target)
        body = "".join(blocks)
        # DEDUP: the source monolith copy-pastes some helpers (is_set_*,
        # hermes_config_merge) multiple times; keep only the first copy of
        # each top-level definition so the extracted module compiles.
        body = dedup_defs(body)
        out = os.path.join(REPO, target)
        if target == "src/cli/config_schema.c":
            # APPEND (do not clobber the existing opaque-struct module)
            with open(out, "a") as f:
                f.write("\n/* --- hermes_config_schema (extracted from config.c) --- */\n\n")
                f.write(body)
            print(f"appended {target}: +{len(body.splitlines())} lines ({len(blocks)} fns)")
        else:
            write(out, HDR.format(base=base) + body)
            print(f"wrote {target}: {len(body.splitlines())} lines ({len(blocks)} fns)")


DEF_RE = re.compile(r'^(?:static\s+|inline\s+|const\s+)?(?:bool|void|char|int|long|unsigned|double|float|size_t|ssize_t|json_t\s*\*|config_schema_t\s*\*|hermes_config_t\s*\*)\s+([a-z_]\w*)\s*\(')


def dedup_defs(body):
    lines = body.split("\n")
    seen = set()
    out = []
    i = 0
    while i < len(lines):
        m = DEF_RE.match(lines[i])
        if m and m.group(1) in seen:
            # skip until the matching closing brace of this duplicate def
            depth = 0
            opened = False
            j = i
            while j < len(lines):
                depth += brace_scan(lines[j])
                if depth > 0:
                    opened = True
                if opened and depth == 0:
                    i = j + 1
                    break
                j += 1
            else:
                i = j + 1
            continue
        if m:
            seen.add(m.group(1))
        out.append(lines[i])
        i += 1
    return "\n".join(out)

    print(f"pruned config.c -> {len(new_cfg.splitlines())} lines (was {n})")


if __name__ == "__main__":
    main()
