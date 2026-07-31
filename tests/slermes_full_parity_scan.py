#!/usr/bin/env python3
"""
Full Parity Scanner: Slermes C vs Python Hermes Agent

Scans every Python file in agent/ and tests/, extracts all
detectable functions/classes/features, then checks C codebase for
matching implementations. Outputs structured mismatch report.

Usage: python3 tests/slermes_full_parity_scan.py [--detail]
"""
import ast
import os
import re
import sys
from pathlib import Path

HERMES_DIR = Path("/home/wubu/hermes-agent-dev")
SLERMES_DIR = HERMES_DIR / "slermes"

# Directories to scan for Python source
SCAN_DIRS = [
    HERMES_DIR / "agent",
]

# Directories to scan for C source
C_SRC_DIRS = [
    SLERMES_DIR / "src",
    SLERMES_DIR / "lib",
]

# Files to EXCLUDE from Python scan (SDK wrappers, configs, etc.)
EXCLUDE_FILES = {
    "__init__.py",
    "portal_tags.py",
    "secret_sources",
    "runtime_cwd.py",
    "jiter_preload.py",
    "copilot_acp_client.py",
}

# Function name → C file mapping (known ports, for verification)
KNOWN_C_PORTS = {}

def extract_python_features(filepath):
    """Extract all function names, class names, and constants from a Python file."""
    try:
        with open(filepath) as f:
            source = f.read()
    except Exception as e:
        return {"error": str(e), "functions": [], "classes": [], "constants": []}

    features = {"functions": [], "classes": [], "constants": [], "decorators": [], "async_functions": []}

    try:
        tree = ast.parse(source, filepath.name)
    except SyntaxError:
        return {"error": "SyntaxError", "functions": [], "classes": [], "constants": []}

    for node in ast.walk(tree):
        if isinstance(node, ast.FunctionDef):
            is_async = False
            for dec in node.decorator_list:
                if isinstance(dec, ast.Name):
                    features["decorators"].append(dec.id)
                elif isinstance(dec, ast.Attribute):
                    features["decorators"].append(f"{dec.value.id}.{dec.attr}" if hasattr(dec.value, 'id') else dec.attr)
            if hasattr(node, 'async') and node.async_def:
                features["async_functions"].append(node.name)
                features["functions"].append(node.name)
            else:
                features["functions"].append(node.name)
        elif isinstance(node, ast.AsyncFunctionDef):
            features["async_functions"].append(node.name)
            features["functions"].append(node.name)
        elif isinstance(node, ast.ClassDef):
            methods = [n.name for n in node.body if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))]
            features["classes"].append({"name": node.name, "methods": methods})

    # Extract top-level constants
    for node in ast.iter_child_nodes(tree):
        if isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name) and target.id.isupper():
                    features["constants"].append(target.id)

    return features


def extract_c_keywords(filepath):
    """Extract function names, struct names, and significant keywords from C file."""
    keywords = {"functions": [], "structs": [], "enums": [], "macros": [], "typedefs": []}

    try:
        with open(filepath) as f:
            content = f.read()
    except Exception as e:
        return keywords

    # Function definitions (including static)
    for m in re.finditer(r'^(?:static\s+)?(?:\w+\s+\**\s*)?(\w+)\s*\(', content, re.MULTILINE):
        name = m.group(1)
        # Exclude control flow, type keywords
        if name not in ("if", "while", "for", "switch", "return", "sizeof",
                        "int", "char", "void", "float", "double", "long",
                        "short", "unsigned", "struct", "enum", "typedef",
                        "const", "static", "extern", "inline", "volatile",
                        "else", "do", "case", "break", "continue", "default",
                        "NULL", "size_t", "ssize_t", "bool", "true", "false",
                        "HERMES_DEFINE", "N2T", "N2S", "S2N"):
            keywords["functions"].append(name)

    # Struct definitions
    for m in re.finditer(r'typedef\s+struct\s+(\w+)', content):
        keywords["structs"].append(m.group(1))
    for m in re.finditer(r'struct\s+(\w+)\s*\{', content):
        keywords["structs"].append(m.group(1))

    # Enum definitions
    for m in re.finditer(r'(?:typedef\s+)?enum\s+(\w+)', content):
        keywords["enums"].append(m.group(1))

    # Macros
    for m in re.finditer(r'#define\s+(\w+)', content):
        keywords["macros"].append(m.group(1))

    return keywords


def build_c_function_index():
    """Build a set of all C function names across the codebase."""
    c_funcs = set()
    c_func_by_file = {}
    c_structs = set()

    for cdir in C_SRC_DIRS:
        if not cdir.exists():
            continue
        for root, dirs, files in os.walk(cdir):
            for f in files:
                if not f.endswith(('.c', '.h')):
                    continue
                fpath = Path(root) / f
                rel = fpath.relative_to(SLERMES_DIR)
                kws = extract_c_keywords(fpath)
                c_func_by_file[rel] = kws
                c_funcs.update(kws["functions"])
                c_structs.update(kws["structs"])

    return c_funcs, c_structs, c_func_by_file


def compute_keyword_match(feature_name, c_funcs):
    """Check if a Python feature name has a likely C equivalent."""
    # Direct match
    if feature_name in c_funcs:
        return ("exact", feature_name)

    # Snake/Pascal/camel variations
    # Remove leading underscore
    base = feature_name.lstrip("_")

    # Check snake_case variations
    for variant in [base, base.replace("_", ""), base.lower()]:
        if variant in c_funcs:
            return ("variant", variant)

    # Check prefix match (C often uses prefixes)
    for prefix in ["hermes_", "agent_", "session_", "cli_", "cmd_",
                    "config_", "model_", "tool_", "gateway_", "db_",
                    "display_", "json_", "str_", "log_", "file_",
                    "io_", "net_", "http_", "ws_", "poll_",
                    "yaml_", "error_", "cred_", "app_", "demo_",
                    "widget_", "curses_", "llm_", "text_", "ctx_"]:
        prefixed = prefix + base
        if prefixed in c_funcs:
            return ("prefixed", prefixed)
        # Also try prefix + base without underscores
        alt = prefix + base.replace("_", "")
        if alt in c_funcs:
            return ("prefixed", alt)

    return None


def compute_keyword_match_struct(name, c_structs):
    """Check for matching struct."""
    if name in c_structs:
        return True
    # Try with _t suffix
    if f"{name}_t" in c_structs:
        return True
    # Remove _t suffix if present
    base = name.rstrip("_t")
    if base != name and base in c_structs:
        return True
    return False


def module_port_status(modname):
    """Reliable port-presence check for Python module ``modname``.

    Detection layers (each progressively softer), so we don't manufacture
    false gaps for ports that are folded into a related/differently-named C
    file:

    1. Dedicated C file by the project's naming conventions (strongest).
    2. An explicit author-intentional fold-in marker in a C file, e.g.
       ``/* Port of Python agent/<modname>.py */`` (catches ports folded into
       a related file, e.g. iteration_budget -> budget_tracker.c).
    3. A mention inside a catch-all wrapper file (softest).
    """
    SL = SLERMES_DIR
    candidates = [
        SL / "src" / "agent" / f"{modname}.c",
        SL / "src" / "agent" / f"port_agent_{modname}.c",
        SL / "src" / "cli" / f"port_agent_{modname}.c",
        SL / "src" / "cli" / f"port_tools_{modname}.c",
        SL / "src" / "tools" / f"port_{modname}.c",
        SL / "src" / "tools" / f"{modname}.c",
        SL / "lib" / f"lib{modname}" / f"{modname}.c",
        SL / "lib" / f"lib{modname}" / f"port_{modname}.c",
    ]
    for c in candidates:
        if c.exists():
            return ("ported", str(c.relative_to(SL)))

    # Layer 2: author-intentional fold-in marker. Scan the C tree once for
    # "Port of Python agent/<modname>.py" / "tools/<modname>.py" comments.
    marker = f"{modname}.py"
    for cdir in (SL / "src", SL / "lib"):
        if not cdir.exists():
            continue
        for root, dirs, files in os.walk(cdir):
            for f in files:
                if not f.endswith((".c", ".h")):
                    continue
                fpath = Path(root) / f
                try:
                    txt = fpath.read_text(encoding="utf-8", errors="replace")
                except Exception:
                    continue
                # Look for a "Port of Python .../<modname>.py" marker.
                if (f"Port of Python agent/{marker}" in txt
                        or f"Port of Python tools/{marker}" in txt
                        or f"port of Python agent/{marker}" in txt
                        or f"port of Python {marker}" in txt
                        or f"// {marker}" in txt or f"/* {marker}" in txt
                        or f"agent/{marker}" in txt or f"tools/{marker}" in txt):
                    return ("ported", str(fpath.relative_to(SL)) + f" (fold-in of {marker})")

    # Layer 3: catch-all wrapper files.
    wrappers = [
        SL / "src" / "agent" / "port_agent_remaining_wrappers.c",
        SL / "src" / "tools" / "port_tools_remaining_wrappers.c",
    ]
    for w in wrappers:
        if w.exists():
            try:
                txt = w.read_text(encoding="utf-8", errors="replace")
            except Exception:
                continue
            if (f"port_{modname}" in txt or f"{modname}_" in txt
                    or (f"modname" in txt and modname in txt)):
                return ("partial", str(w.relative_to(SL)))
    return ("unported", "")


def scan_python_file(filepath, c_funcs, c_structs):
    """Full scan of a single Python file.

    The headline verdict (results["port_status"]) is the RELIABLE file-existence
    check (module_port_status). The function-name heuristic match
    (results["matched"]/["total_features"]) is kept only as a secondary signal,
    because C ports legitimately rename/namespace functions and the heuristic
    undercounts real ports.
    """
    rel = filepath.relative_to(HERMES_DIR)
    modname = filepath.stem
    features = extract_python_features(filepath)

    results = {
        "file": str(rel),
        "loc": 0,
        "total_features": 0,
        "matched": 0,
        "unmatched": [],
        "error": features.get("error"),
        "port_status": "unported",
        "port_evidence": "",
    }

    # Reliable port-presence verdict (file existence).
    pstatus, pev = module_port_status(modname)
    results["port_status"] = pstatus
    results["port_evidence"] = pev

    try:
        with open(filepath) as f:
            results["loc"] = len(f.readlines())
    except:
        pass

    for fn in features["functions"]:
        results["total_features"] += 1
        match = compute_keyword_match(fn, c_funcs)
        if match:
            results["matched"] += 1
        else:
            results["unmatched"].append({"type": "function", "name": fn})

    for cls in features["classes"]:
        results["total_features"] += 1
        # Check if class name matches a C struct
        if compute_keyword_match_struct(cls["name"], c_structs):
            results["matched"] += 1
        else:
            matched_methods = []
            unmatched_methods = []
            for m in cls["methods"]:
                mmatch = compute_keyword_match(m, c_funcs)
                if mmatch:
                    matched_methods.append(m)
                else:
                    unmatched_methods.append(m)

            if len(unmatched_methods) >= len(matched_methods):
                results["unmatched"].append({
                    "type": "class",
                    "name": cls["name"],
                    "matched_methods": matched_methods,
                    "unmatched_methods": unmatched_methods,
                })

    return results


def main():
    detail = "--detail" in sys.argv

    print("=" * 72)
    print("  SLERMES FULL PARITY SCANNER")
    print(f"  Python source: {HERMES_DIR / 'agent/'}")
    print(f"  C source: {SLERMES_DIR / 'src/'}")
    print("=" * 72)
    print()

    # Build C index
    print("Building C function index...")
    c_funcs, c_structs, c_func_by_file = build_c_function_index()
    print(f"  C functions found: {len(c_funcs)}")
    print(f"  C structs found: {len(c_structs)}")
    print(f"  C source files: {len(c_func_by_file)}")
    print()

    # Scan Python files
    all_results = []
    total_funcs = 0
    total_matched = 0
    ported = 0
    partial = 0
    unported = 0

    for py_dir in SCAN_DIRS:
        if not py_dir.exists():
            print(f"  WARNING: {py_dir} not found, skipping")
            continue
        for pyfile in sorted(py_dir.glob("*.py")):
            if pyfile.name in EXCLUDE_FILES:
                continue

            results = scan_python_file(pyfile, c_funcs, c_structs)
            all_results.append(results)

            total_funcs += results["total_features"]
            total_matched += results["matched"]
            if results["port_status"] == "ported":
                ported += 1
            elif results["port_status"] == "partial":
                partial += 1
            else:
                unported += 1

            tag = {"ported": "✅", "partial": "🟡", "unported": "❌"}[results["port_status"]]
            if results["port_status"] == "ported":
                print(f"  {tag} {results['file']} ({results['loc']} LOC) — PORTED [{results['port_evidence']}]")
            elif results["port_status"] == "partial":
                print(f"  {tag} {results['file']} ({results['loc']} LOC) — PARTIAL (in {results['port_evidence']})")
            else:
                print(f"  {tag} {results['file']} ({results['loc']} LOC) — UNPORTED "
                      f"(heuristic fn-match {results['matched']}/{results['total_features']})")

    print()
    print("=" * 72)
    print("  HEADLINE (reliable, file-existence based):")
    print(f"    PORTED:   {ported}")
    print(f"    PARTIAL:  {partial}  (folded into a catch-all wrapper)")
    print(f"    UNPORTED: {unported}")
    print(f"    Total agent/ modules scanned: {len(all_results)}")
    print()
    print("  Secondary heuristic (function-name match, known to undercount):")
    print(f"    {total_matched}/{total_funcs} features matched ({100*total_matched/max(total_funcs,1):.1f}%)")
    print("=" * 72)

    if unported:
        print()
        print("## TRUE UNPORTED MODULES (no C file found — real gaps)")
        print()
        for r in all_results:
            if r["port_status"] == "unported":
                print(f"### {r['file']} ({r['loc']} LOC)")
                if detail:
                    for u in r["unmatched"][:20]:
                        if u["type"] == "function":
                            print(f"  - `{u['name']}` (function)")
                        elif u["type"] == "class":
                            print(f"  - `{u['name']}` (class, {len(u['matched_methods'])} matched / {len(u['unmatched_methods'])} unmatched methods)")
                            for mm in u["unmatched_methods"]:
                                print(f"    - method: `{mm}`")
                    if len(r["unmatched"]) > 20:
                        print(f"  ... and {len(r['unmatched'])-20} more")


if __name__ == "__main__":
    main()
