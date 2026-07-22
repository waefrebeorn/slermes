"""For a given .c file that includes hermes.h, determine which of the
umbrella headers' symbols it actually references, so we can replace the
umbrella include with minimal specific includes."""
import sys, re, os
from pathlib import Path

# All headers that hermes.h pulls in (minus core_types which is the base).
CANDIDATES = [
    "hermes_core_types", "hermes_json", "hermes_yaml", "hermes_http",
    "hermes_crypto", "hermes_db", "hermes_display", "hermes_skin",
    "hermes_agent", "hermes_credits_tracker", "hermes_plugin",
    "hermes_memory", "hermes_tirith", "hermes_media_cache", "hermes_cli",
    "hermes_cron", "hermes_skills", "registry",
]

def header_symbols(h):
    p = Path(f"include/{h}.h")
    if not p.exists():
        return set()
    txt = p.read_text(errors="ignore")
    syms = set()
    # function-like declarations: name before '('
    for m in re.finditer(r'\b([a-z_][a-z0-9_]*)\s*\(', txt):
        syms.add(m.group(1))
    # typedef/struct/enum names
    for m in re.finditer(r'(?:typedef|struct|enum|union)\s+(?:struct\s+)?(?:enum\s+)?([a-z_][a-z0-9_]*)', txt):
        if m.group(1) not in ('struct','enum','union','typedef'):
            syms.add(m.group(1))
    # #define macros
    for m in re.finditer(r'#define\s+([A-Z_][A-Z0-9_]*)', txt):
        syms.add(m.group(1))
    return syms

def file_identifiers(f):
    txt = Path(f).read_text(errors="ignore")
    return set(re.findall(r'\b([a-z_][a-z0-9_]*)\b', txt)), txt

def already_included(txt):
    return set(re.findall(r'#include\s+"([a-z0-9_]+\.h)"', txt))

for f in sys.argv[1:]:
    idents, txt = file_identifiers(f)
    already = already_included(txt)
    print(f"\n=== {f} ===")
    needed = []
    for h in CANDIDATES:
        if f"include/{h}.h" in already:
            continue
        syms = header_symbols(h)
        used = syms & idents
        if used:
            needed.append((h, len(used)))
    for h, n in sorted(needed, key=lambda x: -x[1]):
        print(f"  NEEDS {h}.h  ({n} symbols)")
    print("  already includes:", sorted(already))
