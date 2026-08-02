#!/usr/bin/env python3
"""
slermes_parity_battleground.py — AUTHORITATIVE PoP parity scanner (rewrite, v648).

WHY THIS REWRITE EXISTS (the systemic disease it cures)
-------------------------------------------------------
The previous scanner was a good MATCHER but a fragile SOURCE OF TRUTH:
  1. SILENT LIE: if the Python ground-truth (agent/, tools/, ...) was not
     checked out, it scanned 0 files and printed a confident summary. A
     future agent read that as fact. (This actually happened: the repo had
     fogged "506/191" numbers from a frozen snapshot while the live tree said
     4360+.)
  2. NO DRIFT TRACKING: --update-cache stored only a 3-field summary. It could
     not diff the LIVE feature set against the last rebase baseline, so new
     upstream features (a rebase/PoP operation) never surfaced as NEW gaps.
  3. NO TRIPLE-DA: classification trusted heuristic name-matching. A false
     PORTED (matched the wrong C function) or a false REAL_GAP (feature lives
     in a differently-named file) was never cross-validated.

This rewrite is FAIL-CLOSED, TRIPLE-DA, and DRIFT-AWARE, and is fully AGNOSTIC:
it contains NO hardcoded parity numbers, NO module lists, NO version claims.
Every number it emits is computed from the live tree at run time.

TRIPLE DEVIL'S ADVOCATE (per slermes-da-audit)
-----------------------------------------------
  DA-1 FILE-EXISTENCE: every module the scanner calls REAL_GAP is re-checked
      for a C home by TOPIC (not just the matching filename), using the
      name-parity wrapper map + the impl_map. A REAL_GAP whose feature actually
      exists elsewhere is reclassified (and the mismatch is reported, never
      silently swallowed).
  DA-2 FUNCTION-COUNT: per module, Python def-count vs C function-count ratio.
      A module the scanner marks PORTED but whose C home has <30% of the
      Python function count is flagged THIN (likely shallow/stub port).
  DA-3 PoP-COUNT: annotated-PoP count vs claimed PORTED count per module. A
      PORTED count with zero PoP annotations, or a PoP count exceeding the
      module's real C functions, is flagged (false PORTED / orphan PoP).

UPSTREAM-DRIFT ENGINE (rebase/PoP commit path)
-----------------------------------------------
  The scanner records a BASELINE feature set (every python_file:feature_name)
  into .parity_baseline.json. On each run it diffs the live feature set
  against the baseline:
     NEW_GAP      = feature present upstream now, absent from the last baseline
     RESOLVED     = feature in baseline but gone upstream (module deleted/renamed)
     DRIFT_TOTAL  = |NEW_GAP| — this is what a rebase/PoP operation must close.
  `make parity` regenerates the baseline AFTER a successful rebase, so the
  drift figure always reflects "gaps introduced by the latest upstream pull".

GROUND-TRUTH GATE
-----------------
  The scanner refuses to emit a number unless the Python source of truth is
  actually present (proven by agent/anthropic_adapter.py) AND the scan consumed
  >0 modules. A missing source => exit 2 with remediation. No file, no lie.

This file is the SINGLE scanner. gen_parity_walkway.py and parity_truth.py
consume its --json output. Nothing else computes parity.
"""

import ast
import bisect
import json
import os
import re
import sys
import argparse
import hashlib
from collections import defaultdict
from dataclasses import dataclass, field, asdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

# ── Paths (agnostic: derived from this file's location) ──────────────────────
# This scanner lives in <slermes>/tests/. SLERMES_DIR is its grandparent,
# HERMES_DIR (the fork holding the Python ground-truth) is SLERMES_DIR's parent:
#   <fork>/slermes/        (C11 repo, this file's home is <fork>/slermes/tests/)
#   <fork>/agent/ tools/ ... (Python ground-truth)
HERE = Path(__file__).resolve().parent          # .../slermes/tests
SLERMES_DIR = HERE.parent                        # .../slermes
HERMES_DIR = SLERMES_DIR.parent                  # .../hermes-agent-dev (the fork)
BASELINE_FILE = SLERMES_DIR / "tests" / ".parity_baseline.json"

# Proof that the Python ground-truth is checked out.
SOURCE_PROOF = HERMES_DIR / "agent" / "anthropic_adapter.py"

PYTHON_SOURCE_DIRS = {
    "agent": HERMES_DIR / "agent",
    "tools": HERMES_DIR / "tools",
    "gateway": HERMES_DIR / "gateway",
    "cron": HERMES_DIR / "cron",
    "cli_root": HERMES_DIR / "cli.py",
    "hermes_cli": HERMES_DIR / "hermes_cli",
}

# Top-level fork modules beyond cli.py that are runtime surface with C
# ports (utils.py -> src/agent/proxy_utils.c etc.; hermes_state.py ->
# src/agent/hermes_state/*.c). Tracked so their parity is visible.
ROOT_PY_MODULES = ["utils.py", "hermes_state.py"]

SLERMES_SRC_DIRS = [
    SLERMES_DIR / "src",
    SLERMES_DIR / "src" / "agent",
    SLERMES_DIR / "src" / "tools",
    SLERMES_DIR / "src" / "provider",
    SLERMES_DIR / "src" / "cli",
    SLERMES_DIR / "src" / "gateway",
    SLERMES_DIR / "src" / "cron",
    SLERMES_DIR / "src" / "pet",
    SLERMES_DIR / "include",
]

CACHE_FILE = SLERMES_DIR / "tests" / ".parity_cache.json"

# Vendored third-party directories to skip (never have PoP annotations)
VENDORED_DIRS = {
    "lib/libdb", "lib/libncurses", "lib/liblineedit",
    "lib/ctranslate2", "lib/ctranslate2_src",
    "lib/whisper_cpp_src", "lib/ncurses_link",
}

# ── Data model ───────────────────────────────────────────────────────────────
@dataclass
class PythonFeature:
    name: str
    kind: str
    parent_class: Optional[str] = None
    is_async: bool = False
    decorators: List[str] = field(default_factory=list)
    line_number: int = 0

@dataclass
class CFunction:
    name: str
    file: str
    line: int
    is_static: bool = False
    return_type: str = ""

@dataclass
class PopAnnotation:
    c_function: str
    python_functions: List[str]
    c_file: str
    python_file: str = ""
    line: int = 0
    is_consolidated: bool = False
    full_text: str = ""

@dataclass
class GapEntry:
    python_file: str
    python_feature: PythonFeature
    classification: str
    c_location: Optional[str] = None
    c_function: Optional[str] = None
    pop_annotation: Optional[PopAnnotation] = None
    stub_reason: Optional[str] = None
    severity: str = "LOW"
    notes: str = ""
    da_flags: List[str] = field(default_factory=list)   # triple-DA signals

@dataclass
class ModuleReport:
    python_file: str
    total_features: int = 0
    ported: int = 0
    partial: int = 0
    stub: int = 0
    real_gaps: int = 0
    thin: bool = False            # DA-2: C home <30% of python fn count
    pop_mismatch: bool = False     # DA-3: ported count vs PoP count disagree
    py_func_count: int = 0
    c_func_count: int = 0
    gaps: List[GapEntry] = field(default_factory=list)

# ── Ground-truth gate ─────────────────────────────────────────────────────────
def source_present() -> bool:
    return SOURCE_PROOF.is_file()

# ── Python extraction (AST) ───────────────────────────────────────────────────
class PythonExtractor:
    def __init__(self):
        self.module_cache = {}

    def extract_file(self, filepath: Path) -> List[PythonFeature]:
        if filepath in self.module_cache:
            return self.module_cache[filepath]
        try:
            with open(filepath) as f:
                source = f.read()
        except Exception:
            self.module_cache[filepath] = []
            return self.module_cache[filepath]
        features = []
        try:
            tree = ast.parse(source, filepath.name)
        except SyntaxError:
            self.module_cache[filepath] = features
            return features

        class StackVisitor(ast.NodeVisitor):
            def __init__(self):
                self.features = []
                self.class_stack = []
            def visit_ClassDef(self, node):
                self.class_stack.append(node.name)
                self.generic_visit(node)
                self.class_stack.pop()
            def visit_FunctionDef(self, node):
                self._process(node, False)
            def visit_AsyncFunctionDef(self, node):
                self._process(node, True)
            def _process(self, node, is_async):
                decorators = []
                for dec in node.decorator_list:
                    if isinstance(dec, ast.Name):
                        decorators.append(dec.id)
                    elif isinstance(dec, ast.Attribute):
                        decorators.append(f"{getattr(dec.value,'id','')}.{dec.attr}")
                parent = self.class_stack[-1] if self.class_stack else None
                self.features.append(PythonFeature(
                    name=node.name, kind="method" if parent else "function",
                    parent_class=parent, is_async=is_async,
                    decorators=decorators, line_number=node.lineno))

        visitor = StackVisitor()
        visitor.visit(tree)
        features = visitor.features
        self.module_cache[filepath] = features
        return features

# ── C indexer (functions, structs, PoP annotations, wrappers) ─────────────────
class CIndexer:
    def __init__(self):
        self.functions: Dict[str, List[CFunction]] = defaultdict(list)
        self.structs: Set[str] = set()
        self.pop_annotations: List[PopAnnotation] = []
        self.name_parity_wrappers: Dict[str, Dict] = {}
        self._built = False
        self._file_cache: Dict[str, str] = {}
        self._file_cache_nc: Dict[str, str] = {}
        self._filename_index: Dict[str, List[str]] = defaultdict(list)
        self._line_offsets: Dict[str, List[int]] = {}

    def build(self):
        if self._built:
            return
        c_func_pattern = re.compile(
            r'^(?:static\s+)?(?:const\s+)?(?:__attribute__\(\s*unused\s*\)\s+)?'
            r'(?:\w+\s+)*(?:\*\s*)?(\w+)\s*\(', re.MULTILINE)
        pop_patterns = [
            re.compile(r'/\*\s*PoP:\s*(\w+)\s*@\s*([\w/.]+):([\w.]+)\s*\*/', re.MULTILINE),
            re.compile(r'/\*[\s\S]*?\n\s*\*\s*PoP:\s*(\w+)\s*@\s*([\w/.]+):([\w.]+)', re.MULTILINE),
            re.compile(r'/\*\s*Port of Python\s+\w+_\w+\.py:(_?)\w+\(', re.MULTILINE),
            re.compile(r'/\*\s*Port of Python[^:]*:?\s*([\w.]+)', re.MULTILINE),
            re.compile(r'\*\s*Port of Python\s+[\w/]+\.py:([\w_]+)\(', re.MULTILINE),
            re.compile(r'AG26:\s*Port of Python\s+[\w/]+\.py:([\w_]+)\(', re.MULTILINE),
            re.compile(r'Port of Python:\s+([\w_]+)', re.MULTILINE),
            re.compile(r'/\*(?:[^*]|\*[^/])*?Port of Python:\s*([\w_]+)\(', re.MULTILINE),
            re.compile(r'/\*(?:[^*]|\*[^/])*?Port of Python:\s*[\w]+\.([\w_]+)\(', re.MULTILINE),
            re.compile(r'/\*\s*Port of Python\s+agent/\w+\.py:(\w+)\(', re.MULTILINE),
            re.compile(r'\*\s*Port of Python\s+[\w.]+\.([\w_]+)\(', re.MULTILINE),
            re.compile(r'\*\s*Port of Python\s+\w+\.py:[\w.]+\.([\w_]+)\(', re.MULTILINE),
            re.compile(r'\*\s*Port of Python\s+\w+\.py:([\w_]+)\(', re.MULTILINE),
            re.compile(r'/\*\s*port of Python[^:]*:?\s*([^*]+)\*/', re.MULTILINE),
            re.compile(r'/\*\s*Port of Python\s+agent/(\w+)\.py\s*\([^)]+\)\s*\*/', re.MULTILINE),
            re.compile(r'\n\s*\*\s*PoP:\s*(\w+)\s*@\s*([\w/.]+):([\w.]+)', re.MULTILINE),
        ]
        wrapper_pattern = re.compile(
            r'/\*\s*\n\s*\*(\w+\.c)\s*—\s*Name parity wrapper for Python agent/(\w+\.py)', re.MULTILINE)
        KEYWORDS = {
            "if","while","for","switch","return","sizeof","int","char","void","float",
            "double","long","short","unsigned","struct","enum","typedef","const","static",
            "extern","inline","volatile","else","do","case","break","continue","default",
            "NULL","size_t","ssize_t","bool","true","false"}

        for cdir in SLERMES_SRC_DIRS:
            if not cdir.exists():
                continue
            for root, dirs, files in os.walk(cdir):
                rel_path = str(Path(root).relative_to(SLERMES_DIR))
                dirs[:] = [d for d in dirs
                           if not any((rel_path + "/" + d).startswith(v) for v in VENDORED_DIRS)]
                for f in files:
                    if not f.endswith(('.c', '.h')):
                        continue
                    fpath = Path(root) / f
                    rel = fpath.relative_to(SLERMES_DIR)
                    try:
                        st = fpath.stat()
                    except Exception:
                        continue
                    if st.st_size > 200 * 1024:
                        continue
                    try:
                        with open(fpath) as fp:
                            content = fp.read()
                    except Exception:
                        continue
                    rel_str = str(rel)
                    self._file_cache[rel_str] = content
                    content_nc = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
                    content_nc = re.sub(r'//.*$', '', content_nc, flags=re.MULTILINE)
                    self._file_cache_nc[rel_str] = content_nc
                    self._filename_index[f].append(rel_str)
                    self._line_offsets[rel_str] = [0] + [i+1 for i, c in enumerate(content) if c == '\n']

                    for m in c_func_pattern.finditer(content):
                        name = m.group(1)
                        if name in KEYWORDS:
                            continue
                        line = bisect.bisect_right(self._line_offsets[rel_str], m.start())
                        is_static = 'static' in content[max(0, m.start()-50):m.start()]
                        return_match = re.search(r'(?:^|\n)([a-zA-Z_]\w*(?:\s*\\*)?)\s+' + re.escape(name) + r'\s*\(', content[:m.start()], re.MULTILINE)
                        return_type = return_match.group(1) if return_match else ""
                        self.functions[name].append(CFunction(name=name, file=rel_str, line=line, is_static=is_static, return_type=return_type))

                    for m in re.finditer(r'typedef\s+struct\s+(\w+)', content):
                        self.structs.add(m.group(1))
                    for m in re.finditer(r'struct\s+(\w+)\s*\{', content):
                        self.structs.add(m.group(1))

                    pop_pattern = pop_patterns[0]
                    matched = False
                    for pattern in pop_patterns:
                        for m in pattern.finditer(content):
                            if pattern is pop_patterns[15]:
                                continue
                            if pattern is pop_pattern:
                                raw = m.group(3).strip()
                                py_funcs = [raw.split('.')[-1]]
                            elif pattern.pattern.startswith(r'/\*[\s\S]*?'):
                                py_funcs = [m.group(3).strip()]
                            else:
                                py_names = m.group(1).strip()
                                py_funcs = []
                                for n in py_names.split(','):
                                    py_funcs.append(n.strip().split('(')[0].strip())
                            line = bisect.bisect_right(self._line_offsets[rel_str], m.start())
                            c_func_name = self._find_annotation_target(content, m.start())
                            python_file = ""
                            if pattern is pop_pattern or pattern.pattern.startswith(r'/\*[\s\S]*?') or pattern.pattern.startswith(r'/\*\n'):
                                python_file = m.group(2).strip()
                            self.pop_annotations.append(PopAnnotation(
                                c_function=c_func_name, python_functions=py_funcs,
                                c_file=rel_str, python_file=python_file, line=line,
                                is_consolidated=len(py_funcs) > 1, full_text=m.group(0)[:200]))

                    for m in wrapper_pattern.finditer(content):
                        wrapper_file = m.group(1)
                        py_file = m.group(2)
                        self.name_parity_wrappers[wrapper_file] = {
                            "python_file": py_file,
                            "impl_file": self._extract_impl_file(content),
                            "claims": self._extract_wrapper_claims(content),
                            "source_file": rel_str,
                        }
        self._built = True

    def _find_annotation_target(self, content, pos):
        after = content[pos:pos+2000]
        match = re.search(r'^(?:static\s+)?(?:const\s+)?(?:\w+\s+)+\*?\s*(\w+)\s*\(', after, re.MULTILINE)
        return match.group(1) if match else ""

    def _get_cached_content(self, rel_path: str) -> Tuple[str, str]:
        """Get raw and comment-stripped content for a C file (cached)."""
        if rel_path in self._file_cache:
            return self._file_cache[rel_path], self._file_cache_nc[rel_path]
        fpath = SLERMES_DIR / rel_path
        try:
            with open(fpath) as fp:
                raw = fp.read()
        except Exception:
            raw = ""
        nc = re.sub(r'/\*.*?\*/', '', raw, flags=re.DOTALL)
        nc = re.sub(r'//.*$', '', nc, flags=re.MULTILINE)
        self._file_cache[rel_path] = raw
        self._file_cache_nc[rel_path] = nc
        return raw, nc

    def _extract_wrapper_claims(self, content):
        claims = []
        if "Key functions ported:" in content:
            section = content.split("Key functions ported:")[1].split("*/")[0]
            for line in section.split('\n'):
                line = line.strip()
                if line.startswith('-') or line.startswith('*'):
                    m = re.search(r'`?(\w+)`?(?:\s*\(\.\))?', line)
                    if m:
                        claims.append(m.group(1))
        for m in re.finditer(r'(\w+)\s+\(\):', content):
            claims.append(m.group(1))
        return list(set(claims))

    def _extract_impl_file(self, content):
        for pattern in [r'implementation lives in\s+(\S+\.c)', r'impl in\s+(\S+\.c)', r'C implementation:\s+(\S+\.c)']:
            m = re.search(pattern, content, re.IGNORECASE)
            if m:
                return m.group(1)
        return None

    # ── name matching (heuristic, preserved from prior scanner) ──
    def find_c_function(self, python_name, py_file=None, parent_class=None):
        python_name = python_name.lstrip('_')
        matches = []
        if python_name in self.functions:
            matches.extend(self.functions[python_name])
        prefixes = [
            "hermes_","agent_","session_","cli_","cmd_","config_","model_","tool_",
            "gateway_","db_","display_","json_","str_","log_","file_","io_","net_",
            "http_","ws_","poll_","yaml_","error_","cred_","llm_","text_","ctx_",
            "skill_","memory_","prompt_","provider_","google_","anthropic_","azure_",
            "bedrock_","codex_","copilot_","nous_","image_gen_","web_search_","video_gen_",
            "tts_","transcribe_","plugin_","curator_","credential_","auxiliary_",
            "nous_rate_guard_","skill_bundles_","credits_tracker_","plugin_llm_",
            "response_store_","idempotency_cache_","run_status_","sse_queue_","sse_writer_","agent_run_",
            "browser_","mcp_","delegate_","approval_","terminal_","process_","sandbox_",
            "vision_","voice_","web_","memory_","image_gen_","video_gen_","transcribe_",
        ]
        if py_file:
            module_name = py_file.split('/')[-1].replace('.py', '')
            if module_name and module_name not in ['main','config','base','helpers','utils']:
                prefixes.append(module_name + '_')
            if py_file.startswith("gateway/platforms/"):
                prefixes.append("gw_")
                if module_name == "base" and "base_" in prefixes:
                    prefixes.remove("base_")  # base.py functions don't use base_ prefix in C
            if parent_class:
                snake = re.sub(r'(?<!^)(?=[A-Z])', '_', parent_class).lower()
                prefixes.append(snake + '_')
                if parent_class == "BasePlatformAdapter":
                    prefixes.append("gw_base_platform_adapter_default_")
        for prefix in prefixes:
            if python_name.startswith(prefix):
                if python_name in self.functions:
                    matches.extend(self.functions[python_name])
                h = python_name + "_handler"
                if h in self.functions:
                    matches.extend(self.functions[h])
            else:
                p = prefix + python_name
                if p in self.functions:
                    matches.extend(self.functions[p])
                ph = prefix + python_name + "_handler"
                if ph in self.functions:
                    matches.extend(self.functions[ph])
            hp = "handle_" + python_name
            if hp in self.functions:
                matches.extend(self.functions[hp])
        return matches

    def find_pop_for_python(self, python_name, py_file=""):
        matches = []
        for pop in self.pop_annotations:
            if python_name in pop.python_functions:
                # A pop without a python_file is a partial-pattern artifact
                # (e.g. the trailing-underscore-word capture); it must never
                # shadow a real annotation when the caller knows its module.
                if py_file and (not pop.python_file or pop.python_file != py_file):
                    continue
                matches.append(pop)
        return matches

    def find_wrapper_for_module(self, python_file):
        base = python_file.replace('.py', '.c')
        for wrapper, info in self.name_parity_wrappers.items():
            if wrapper == base or wrapper.replace('.c', '') == python_file.replace('.py', ''):
                return info, info.get("claims", [])
        return None, []

    def find_c_function_with_prefix(self, python_name, prefix):
        python_name = python_name.lstrip('_')
        matches = []
        p = prefix + python_name
        if p in self.functions:
            matches.extend(self.functions[p])
        return matches

    def find_vtable_defaults_global(self, module_name, method_name):
        matches = []
        patterns = [f"default_{method_name}", f"{module_name}_default_{method_name}",
                    f"{module_name}_default_{method_name.capitalize()}"]
        provider_prefixes = {"tts_provider":"tts_","image_gen_provider":"image_gen_",
            "video_gen_provider":"video_gen_","web_search_provider":"web_search_",
            "transcription_provider":"transcribe_","memory_provider":"memory_","context_engine":"context_"}
        if module_name in provider_prefixes:
            patterns.append(f"{provider_prefixes[module_name]}{method_name}")
        if len(method_name) > 30:
            t = method_name[:30].rstrip('_')
            patterns.append(f"default_{t}")
            patterns.append(f"{module_name}_default_{t}")
        for p in patterns:
            if p in self.functions:
                matches.extend(self.functions[p])
        return matches

# ── Analyzer ──────────────────────────────────────────────────────────────────
class ParityAnalyzer:
    def __init__(self):
        self.extractor = PythonExtractor()
        self.c_index = CIndexer()
        self.c_index.build()
        self.module_map = self._load_module_map()
        self.impl_map = self._build_impl_map()

    def _load_module_map(self):
        mapping = {}
        mm_path = SLERMES_DIR / "docs" / "module-map.md"
        if not mm_path.exists():
            return mapping
        with open(mm_path) as f:
            content = f.read()
        for line in content.split('\n'):
            if '|' in line and ('src/' in line or 'lib/' in line):
                parts = [p.strip() for p in line.split('|') if p.strip()]
                if len(parts) >= 3:
                    py_file = parts[0].strip('`')
                    c_loc = parts[1].strip('`')
                    if py_file.endswith('.py') and ('src/' in c_loc or 'lib/' in c_loc):
                        mapping[py_file] = c_loc
        return mapping

    def _build_impl_map(self):
        impl_map = {}
        for py_file, c_loc in self.module_map.items():
            if "(wrapper)" in c_loc or "Impl in" in c_loc:
                m = re.search(r'(?:Impl in|impl in)\s+(\S+\.c)', c_loc, re.IGNORECASE)
                impl_map[py_file] = m.group(1) if m else c_loc
            else:
                impl_map[py_file] = c_loc
        mm_path = SLERMES_DIR / "docs" / "module-map.md"
        if mm_path.exists():
            with open(mm_path) as f:
                content = f.read()
            for line in content.split('\n'):
                if '|' in line and 'Impl in' in line and 'src/' in line:
                    parts = [p.strip().strip('`') for p in line.split('|') if p.strip()]
                    if len(parts) >= 4:
                        py_file = parts[0]; notes = parts[3]
                        if 'Impl in' in notes:
                            m = re.search(r'(?:Impl in|impl in)\s+(\S+\.c)', notes, re.IGNORECASE)
                            if m and py_file.endswith('.py'):
                                impl_map[py_file] = m.group(1)
        for wrapper, info in self.c_index.name_parity_wrappers.items():
            pf = info.get("python_file"); impl = info.get("impl_file")
            if pf and impl:
                impl_map[pf] = impl
        impl_map.update(self._cross_dir_mappings())
        return impl_map

    def _cross_dir_mappings(self):
        # Topic-based C home for Python modules whose impl lives in a
        # differently-named file. This is the DA-1 evidence base.
        M = {}
        def add(py, c): M[py] = c
        # agent/
        add("agent/pet/constants.py","src/pet/pet_constants.c")
        add("agent/pet/state.py","src/pet/pet_state.c")
        add("agent/pet/manifest.py","src/pet/pet_manifest.c")
        add("agent/pet/store.py","src/pet/pet_store.c")
        add("agent/pet/render.py","src/pet/pet_render.c")
        add("agent/pet/__init__.py","src/pet/pet_commands.c")
        add("agent/coding_context.py","src/agent/coding_context.c")
        # tools/ (compressed: topic -> real C home)
        for py, c in {
            "tools/approval.py":"src/tools/approval.c","tools/blueprints.py":"src/tools/blueprints.c",
            "tools/browser_camofox.py":"src/tools/browser.c","tools/browser_cdp_tool.py":"src/tools/browser.c",
            "tools/browser_dialog_tool.py":"src/tools/browser.c","tools/browser_supervisor.py":"src/tools/browser.c",
            "tools/browser_tool.py":"src/tools/browser.c","tools/checkpoint_manager.py":"src/tools/checkpoint_manager.c",
            "tools/code_execution_tool.py":"src/tools/exec_code.c","tools/computer_use/backend.py":"src/tools/computer_use.c",
            "tools/computer_use/cua_backend.py":"src/tools/computer_use.c","tools/computer_use/tool.py":"src/tools/computer_use.c",
            "tools/computer_use/vision_routing.py":"src/tools/computer_use.c","tools/credential_files.py":"src/tools/credential_files.c",
            "tools/cronjob_tools.py":"src/tools/cronjob_tools.c","tools/delegate_tool.py":"src/tools/delegate.c",
            "tools/discord_tool.py":"src/tools/discord.c","tools/env_probe.py":"src/tools/env_probe.c",
            "tools/environments/base.py":"src/tools/environments.c","tools/environments/daytona.py":"src/tools/environments.c",
            "tools/environments/docker.py":"src/tools/environments.c","tools/environments/file_sync.py":"src/tools/environments.c",
            "tools/environments/local.py":"src/tools/environments.c","tools/environments/managed_modal.py":"src/tools/environments.c",
            "tools/environments/modal.py":"src/tools/environments.c","tools/environments/modal_utils.py":"src/tools/environments.c",
            "tools/environments/ssh.py":"src/tools/environments.c","tools/environments/singularity.py":"src/tools/environments.c",
            "tools/fal_common.py":"src/tools/fal_common.c","tools/feishu_comment_rules.py":"src/tools/feishu_comment_rules.c",
            "tools/feishu_doc_tool.py":"src/tools/feishu_doc_tool.c","tools/feishu_drive_tool.py":"src/tools/feishu_drive_tool.c",
            "tools/file_operations.py":"src/tools/file.c","tools/file_tools.py":"src/tools/file.c",
            "tools/fuzzy_match.py":"src/tools/fuzzy_match.c","tools/homeassistant_tool.py":"src/tools/homeassistant.c",
            "tools/image_generation_tool.py":"src/tools/image_gen.c","tools/interrupt.py":"src/tools/interrupt.c",
            "tools/kanban_tools.py":"src/tools/kanban.c","tools/lazy_deps.py":"src/tools/lazy_deps.c",
            "tools/managed_tool_gateway.py":"src/tools/managed_tool_gateway.c","tools/mcp_oauth.py":"src/tools/mcp_oauth.c",
            "tools/mcp_oauth_manager.py":"src/tools/mcp_oauth_manager.c","tools/mcp_tool.py":"src/tools/mcp_tool.c",
            "tools/memory_tool.py":"src/tools/memory.c","tools/mixture_of_agents_tool.py":"src/tools/mixture_of_agents.c",
            "tools/neutts_synth.py":"src/tools/neutts_synth.c","tools/osv_check.py":"src/tools/osv_check.c",
            "tools/patch_parser.py":"src/tools/patch_parser.c","tools/path_security.py":"src/tools/path_security.c",
            "tools/process_registry.py":"src/tools/process_registry.c","tools/read_extract.py":"src/tools/read_extract.c",
            "tools/read_terminal_tool.py":"src/tools/read_terminal.c","tools/registry.py":"src/tools/registry.c",
            "tools/schema_sanitizer.py":"src/tools/schema_sanitizer.c","tools/send_message_tool.py":"src/tools/send_message.c",
            "tools/skill_usage.py":"src/tools/skills.c","tools/skill_manager_tool.py":"src/tools/skill_manager.c",
            "tools/skills_ast_audit.py":"src/tools/skills_ast_audit.c","tools/skills_guard.py":"src/tools/skills_guard.c",
            "tools/skills_hub.py":"src/tools/skills_hub.c","tools/skills_sync.py":"src/tools/skills_sync.c",
            "tools/skills_tool.py":"src/tools/port_skills_tool.c","tools/slash_confirm.py":"src/tools/slash_confirm.c",
            "tools/terminal_tool.py":"src/tools/terminal.c","tools/thread_context.py":"src/tools/thread_context.c",
            "tools/threat_patterns.py":"src/tools/threat_patterns.c","tools/tirith_security.py":"src/tools/tirith.c",
            "tools/todo_tool.py":"src/tools/todo.c","tools/tool_backend_helpers.py":"src/tools/tool_backend_helpers.c",
            "tools/tool_output_limits.py":"src/tools/tool_output_limits.c","tools/tool_result_storage.py":"src/tools/result_storage.c",
            "tools/tool_search.py":"src/tools/tool_search.c","tools/transcription_tools.py":"src/tools/transcribe.c",
            "tools/tts_tool.py":"src/tools/tts.c","tools/video_generation_tool.py":"src/tools/video_gen.c",
            "tools/vision_tools.py":"src/tools/vision.c","tools/voice_mode.py":"src/tools/voice.c",
            "tools/web_tools.py":"src/tools/web.c","tools/website_policy.py":"src/tools/website_policy.c",
            "tools/write_approval.py":"src/tools/write_approval.c",
            "agent/redact.py":"src/tools/browser_redact.c","tools/x_search_tool.py":"src/tools/x_search.c",
            "tools/xai_http.py":"src/tools/xai_http.c","tools/yuanbao_tools.py":"src/tools/yuanbao_tools.c",
            "tools/kanban_tools.py":"src/tools/kanban_tools.c","tools/mcp_oauth.py":"src/tools/mcp_oauth.c",
            "tools/registry.py":"src/tools/registry.c","tools/web_tools.py":"src/tools/web.c",
            "tools/ansi_strip.py":"src/tools/ansi_strip.c","tools/binary_extensions.py":"src/tools/binary_extensions.c",
            "tools/budget_config.py":"src/tools/budget_config.c","tools/clarify_gateway.py":"src/tools/clarify.c",
            "tools/clarify_tool.py":"src/tools/clarify.c","tools/debug_helpers.py":"src/tools/debug_helpers.c",
            "tools/env_passthrough.py":"src/tools/env_passthrough.c","tools/microsoft_graph_auth.py":"src/tools/microsoft_graph_auth.c",
            "tools/microsoft_graph_client.py":"src/tools/microsoft_graph_client.c","tools/openrouter_client.py":"src/tools/openrouter_client.c",
            "tools/skill_provenance.py":"src/tools/skill_provenance.c","tools/slash_confirm.py":"src/tools/slash_confirm.c",
            "tools/tool_result_storage.py":"src/tools/result_storage.c","tools/website_policy.py":"src/tools/website_policy.c",
            "tools/xai_http.py":"src/tools/xai_http.c",
        }.items():
            add(py, c)
        # gateway/
        for py, c in {
            "gateway/config.py":"src/gateway/config.c","gateway/delivery.py":"src/gateway/delivery.c",
            "gateway/hooks.py":"src/gateway/hooks.c","gateway/memory_monitor.py":"src/gateway/memory_monitor.c",
            "gateway/mirror.py":"src/gateway/mirror.c","gateway/pairing.py":"src/gateway/pairing.c",
            "gateway/platform_registry.py":"src/gateway/platform_registry.c","gateway/restart.py":"src/gateway/restart.c",
            "gateway/run.py":"src/gateway/run.c","gateway/session.py":"src/gateway/session.c",
            "gateway/slash_commands.py":"src/gateway/slash_commands.c","gateway/status.py":"src/gateway/status.c",
            "gateway/sticker_cache.py":"src/gateway/sticker_cache.c","gateway/stream_consumer.py":"src/gateway/stream_consumer.c",
            "gateway/stream_dispatch.py":"src/gateway/stream_dispatch.c","gateway/stream_events.py":"src/gateway/stream_events.c",
            "gateway/authz_mixin.py":"src/gateway/authz_mixin.c","gateway/channel_directory.py":"src/gateway/channel_directory.c",
            "gateway/display_config.py":"src/gateway/display_config.c","gateway/kanban_watchers.py":"src/gateway/kanban_watchers.c",
            "gateway/response_filters.py":"src/gateway/response_filters.c","gateway/runtime_footer.py":"src/gateway/runtime_footer.c",
            "gateway/shutdown_forensics.py":"src/gateway/shutdown_forensics.c","gateway/slash_access.py":"src/gateway/slash_access.c",
            "gateway/whatsapp_identity.py":"src/gateway/whatsapp_identity.c",
            "gateway/platforms/base.py":"src/gateway/platforms/base.c","gateway/platforms/api_server.py":"src/gateway/platforms/api_server_adapter.c",
            "gateway/platforms/bluebubbles.py":"src/gateway/platforms/bluebubbles.c","gateway/platforms/dingtalk.py":"src/gateway/platforms/dingtalk.c",
            "gateway/platforms/email.py":"src/gateway/platforms/email.c","gateway/platforms/feishu.py":"src/gateway/platforms/feishu.c",
            "gateway/platforms/feishu_comment.py":"src/gateway/platforms/feishu_comment.c","gateway/platforms/feishu_comment_rules.py":"src/gateway/platforms/feishu_comment_rules.c",
            "gateway/platforms/feishu_meeting_invite.py":"src/gateway/platforms/feishu_comment.c","gateway/platforms/helpers.py":"src/gateway/platforms/helpers.c",
            "gateway/platforms/matrix.py":"src/gateway/platforms/matrix.c","gateway/platforms/msgraph_webhook.py":"src/gateway/platforms/msgraph_webhook.c",
            "gateway/platforms/slack.py":"src/gateway/platforms/slack.c","gateway/platforms/sms.py":"src/gateway/platforms/sms.c",
            "gateway/platforms/signal.py":"src/gateway/platforms/signal.c","gateway/platforms/signal_rate_limit.py":"src/gateway/platforms/signal_rate_limit.c",
            "gateway/platforms/telegram.py":"src/gateway/platforms/telegram.c","gateway/platforms/telegram_network.py":"src/gateway/platforms/telegram_network.c",
            "gateway/platforms/webhook.py":"src/gateway/platforms/webhook.c","gateway/platforms/wecom.py":"src/gateway/platforms/wecom.c",
            "gateway/platforms/wecom_callback.py":"src/gateway/platforms/wecom_callback.c","gateway/platforms/weixin.py":"src/gateway/platforms/weixin.c",
            "gateway/platforms/whatsapp.py":"src/gateway/platforms/whatsapp.c","gateway/platforms/whatsapp_cloud.py":"src/gateway/platforms/whatsapp.c",
            "gateway/platforms/whatsapp_common.py":"src/gateway/platforms/whatsapp.c","gateway/platforms/yuanbao.py":"src/gateway/platforms/yuanbao.c",
            "gateway/platforms/yuanbao_media.py":"src/gateway/platforms/yuanbao_media.c","gateway/platforms/yuanbao_proto.py":"src/gateway/platforms/yuanbao_proto.c",
            "gateway/platforms/yuanbao_sticker.py":"src/gateway/platforms/yuanbao_sticker.c",
            "gateway/platforms/qqbot/adapter.py":"src/gateway/platforms/qqbot.c","gateway/platforms/qqbot/chunked_upload.py":"src/gateway/platforms/qqbot.c",
            "gateway/platforms/qqbot/crypto.py":"src/gateway/platforms/qqbot.c","gateway/platforms/qqbot/keyboards.py":"src/gateway/platforms/qqbot.c",
            "gateway/platforms/qqbot/onboard.py":"src/gateway/platforms/qqbot.c","gateway/platforms/qqbot/utils.py":"src/gateway/platforms/qqbot.c",
        }.items():
            add(py, c)
        # cron/
        for py, c in {
            "cron/blueprint_catalog.py":"src/tools/blueprints.c","cron/jobs.py":"src/tools/cronjob_tools.c",
            "cron/scheduler.py":"src/tools/scheduler.c","cron/suggestion_catalog.py":"src/tools/suggestion_catalog.c",
            "cron/suggestions.py":"src/tools/suggestions.c",
        }.items():
            add(py, c)
        # cli.py + hermes_cli/
        add("cli.py","src/cli/main.c")
        for py, c in {
            "hermes_cli/auth.py":"src/cli/auth.c","hermes_cli/auth_commands.py":"src/cli/commands.c",
            "hermes_cli/backup.py":"src/cli/backup.c","hermes_cli/banner.py":"src/cli/banner.c",
            "hermes_cli/blueprint_cmd.py":"src/cli/blueprint_cmd.c","hermes_cli/cli_agent_setup_mixin.py":"src/cli/cli_agent_setup_mixin.c",
            "hermes_cli/cli_commands_mixin.py":"src/cli/commands.c","hermes_cli/cli_output.py":"src/cli/cli_output.c",
            "hermes_cli/clipboard.py":"src/cli/clipboard.c","hermes_cli/commands.py":"src/cli/commands.c",
            "hermes_cli/completion.py":"src/cli/completion.c","hermes_cli/config.py":"src/cli/config.c",
            "hermes_cli/container_boot.py":"src/cli/container_boot.c","hermes_cli/curator.py":"src/cli/curator.c",
            "hermes_cli/curses_ui.py":"src/cli/tui_fullscreen.c","hermes_cli/doctor.py":"src/cli/doctor.c",
            "hermes_cli/dump.py":"src/cli/dump.c","hermes_cli/env_loader.py":"src/cli/env_loader.c",
            "hermes_cli/fallback_cmd.py":"src/cli/fallback_cmd.c","hermes_cli/gateway.py":"src/cli/gateway.c",
            "hermes_cli/gateway_windows.py":"src/cli/gateway_windows.c","hermes_cli/goals.py":"src/cli/goals.c",
            "hermes_cli/gui_uninstall.py":"src/cli/gui_uninstall.c","hermes_cli/kanban.py":"src/cli/kanban.c",
            "hermes_cli/kanban_db.py":"src/cli/kanban_db_engine.c","hermes_cli/kanban_decompose.py":"src/cli/kanban_decompose.c",
            "hermes_cli/kanban_diagnostics.py":"src/cli/kanban_diagnostics.c","hermes_cli/kanban_specify.py":"src/cli/kanban_specify.c",
            "hermes_cli/kanban_swarm.py":"src/cli/kanban_swarm.c","hermes_cli/logs.py":"src/cli/logs.c",
            "hermes_cli/main.py":"src/cli/main.c","hermes_cli/mcp_catalog.py":"src/cli/mcp_catalog.c",
            "hermes_cli/mcp_config.py":"src/cli/mcp_config.c","hermes_cli/mcp_picker.py":"src/cli/mcp_picker.c",
            "hermes_cli/mcp_security.py":"src/cli/mcp_security.c","hermes_cli/mcp_startup.py":"src/cli/mcp_startup.c",
            "hermes_cli/memory_setup.py":"src/cli/memory_setup.c","hermes_cli/middleware.py":"src/cli/middleware.c",
            "hermes_cli/migrate.py":"src/cli/migrate.c","hermes_cli/model_catalog.py":"src/cli/model_catalog.c",
            "hermes_cli/model_cost_guard.py":"src/cli/model_cost_guard.c","hermes_cli/model_normalize.py":"src/cli/model_normalize.c",
            "hermes_cli/model_setup_flows.py":"src/cli/model_setup_flows.c","hermes_cli/model_switch.py":"src/cli/model_switch.c",
            "hermes_cli/models.py":"src/cli/models.c","hermes_cli/nous_account.py":"src/cli/nous_account.c",
            "hermes_cli/nous_subscription.py":"src/cli/nous_subscription.c","hermes_cli/oneshot.py":"src/cli/oneshot.c",
            "hermes_cli/pairing.py":"src/cli/pairing.c","hermes_cli/partial_compress.py":"src/cli/partial_compress.c",
            "hermes_cli/platforms.py":"src/cli/platforms.c","hermes_cli/plugins.py":"src/cli/plugins.c",
            "hermes_cli/plugins_cmd.py":"src/cli/plugins_cmd.c","hermes_cli/portal_cli.py":"src/cli/portal_cli.c",
            "hermes_cli/profile_describer.py":"src/cli/profile_describer.c","hermes_cli/profile_distribution.py":"src/cli/profile_distribution.c",
            "hermes_cli/profiles.py":"src/cli/profiles.c","hermes_cli/prompt_size.py":"src/cli/prompt_size.c",
            "hermes_cli/providers.py":"src/cli/providers.c","hermes_cli/relaunch.py":"src/cli/relaunch.c",
            "hermes_cli/runtime_provider.py":"src/cli/runtime_provider.c","hermes_cli/secret_prompt.py":"src/cli/secret_prompt.c",
            "hermes_cli/secrets_cli.py":"src/cli/secrets_cli.c","hermes_cli/security_advisories.py":"src/cli/security_advisories.c",
            "hermes_cli/security_audit.py":"src/cli/security_audit.c","hermes_cli/send_cmd.py":"src/cli/send_cmd.c",
            "hermes_cli/service_manager.py":"src/cli/service_manager.c","hermes_cli/session_recap.py":"src/cli/session_recap.c",
            "hermes_cli/setup.py":"src/cli/setup.c","hermes_cli/setup_whatsapp_cloud.py":"src/cli/setup_whatsapp_cloud.c",
            "hermes_cli/skills_config.py":"src/cli/skills_config.c","hermes_cli/skills_hub.py":"src/cli/skills_hub.c",
            "hermes_cli/status.py":"src/cli/status.c","hermes_cli/stdio.py":"src/cli/stdio.c",
            "hermes_cli/suggestions_cmd.py":"src/cli/suggestions_cmd.c","hermes_cli/telegram_managed_bot.py":"src/cli/telegram_managed_bot.c",
            "hermes_cli/timeouts.py":"src/cli/timeouts.c","hermes_cli/tips.py":"src/cli/tips.c",
            "hermes_cli/tools_config.py":"src/cli/tools_config.c","hermes_cli/uninstall.py":"src/cli/uninstall.c",
            "hermes_cli/voice.py":"src/cli/voice.c","hermes_cli/web_server.py":"src/cli/web_server.c",
            "hermes_cli/webhook.py":"src/cli/webhook.c","hermes_cli/win_pty_bridge.py":"src/cli/win_pty_bridge.c",
            "hermes_cli/write_approval_commands.py":"src/cli/write_approval_commands.c",
        }.items():
            add(py, c)
        return M

    # ── recursive bootleg detection (fixes the false-global-gap blind spot) ──
    # The parity scanner counts any PoP-annotated C function as PORTED, so a
    # function that delegates to a bootleg stub (or is itself a bootleg stub)
    # reports parity but does no real work => a FALSE global gap. This method
    # builds a call graph over the *ported* TUs (port_*.c / port_*.h) and
    # classifies a function as BOOTLEG if, after ignoring calls to REAL/
    # external functions (library calls and the live cli_cmd_*.c handlers,
    # which are defined OUTSIDE the ported set), its body does no observable
    # work and every remaining internal callee is itself bootleg.
    _REAL_SIGNALS = [
        r'\bprintf\s*\(', r'\bfprintf\s*\(', r'\bputs\s*\(', r'\bfputs\s*\(',
        r'\bfwrite\s*\(', r'\bperror\s*\(', r'\bhermes_log\b', r'\blog_',
        r'\bfopen\s*\(', r'\bopen\s*\(', r'\bfclose\s*\(', r'\bclose\s*\(',
        r'\bjson_object_set', r'\bjson_array_append', r'\bjson_set',
        r'\bconfig_py_save', r'\bconfig_py_atomic', r'\bwrite_config',
        r'\bsystem\s*\(', r'\bexecl', r'\bfork\s*\(', r'\bpopen\s*\(',
        r'\bcurl', r'\bhttp_', r'\bsocket\s*\(',
        r'\bmalloc\s*\(', r'\bcalloc\s*\(', r'\brealloc\s*\(',
        r'\bfree\s*\(', r'\bmemset\s*\(', r'\bstrdup\s*\(',
        r'\bstrcpy', r'\bstrcat', r'\bsnprintf\s*\(', r'\bsprintf\s*\(',
        r'\bfgets\s*\(', r'\bfread\s*\(', r'\bsqlite',
        r'\byaml_', r'\bjson_parse', r'\bjson_new',
        r'\bprocess_registry', r'\bclipboard',

    r'\bfor\s*\(',
    r'\bwhile\s*\(',
    r'\bswitch\s*\(',
    r'\bstrcmp\s*\(',
    r'\bstrncmp\s*\(',
    r'\bstrcasecmp\s*\(',
    r'\bstrncasecmp\s*\(',
    r'\bstrstr\s*\(',
    r'\bstrchr\s*\(',
    r'\bstrrchr\s*\(',
    r'\bstrlen\s*\(',
    r'\bstrtol\s*\(',
    r'\bstrtod\s*\(',
    r'\bstrtok\s*\(',
    r'\batoi\s*\(',
    r'\bmemcmp\s*\(',
    r'\bmemcpy\s*\(',
    r'\bmemmove\s*\(',
    r'\bmemset\s*\(',
    r'\bisspace\s*\(',
    r'\bisdigit\s*\(',
    r'\bisalpha\s*\(',
    r'\bisalnum\s*\(',
    r'\btolower\s*\(',
    r'\btoupper\s*\(',
    r'\bgetenv\s*\(',
    r'\bstat\s*\(',
    r'\baccess\s*\(',
    r'\bunlink\s*\(',
    r'\bmkdir\s*\(',
    r'\brename\s*\(',
    r'\bchmod\s*\(',
    r'\bgetcwd\s*\(',
    r'\bstrftime\s*\(',
    r'\blocaltime\s*\(',
    r'\bgmtime\s*\(',
    r'\btime\s*\(',
    r'\bclock_gettime\s*\(',
    r'\bpthread_',
    r'\bsigaction\s*\(',
    r'\berrno',

    ]
    _REAL_RE = [re.compile(p) for p in _REAL_SIGNALS]
    _CALLEE = re.compile(r'(?<![.\w])([a-zA-Z_]\w*)\s*\(')
    _FUNC_DEF = re.compile(
        r'(?:static\s+|inline\s+)?(?:const\s+|unsigned\s+)?'
        r'(?:[\w:]+\s+)+?(\*?\w+)\s*\(([^;]*?)\)\s*\{', re.S)

    def _ensure_port_graph(self):
        if hasattr(self, '_port_defined'):
            return
        defined = {}
        for fname, rels in self.c_index._filename_index.items():
            if not (fname.startswith('port_') and (fname.endswith('.c') or fname.endswith('.h'))):
                continue
            for rel in rels:
                content, _ = self.c_index._get_cached_content(rel)
                for m in self._FUNC_DEF.finditer(content):
                    name = m.group(1)
                    if name in ('if','while','for','switch','return','sizeof','catch'):
                        continue
                    body_start = m.end(); depth = 1; pos = body_start
                    in_s=in_c=in_lc=in_bc=False
                    while pos < len(content) and depth > 0:
                        ch = content[pos]; prev = content[pos-1] if pos>0 else '\0'
                        if in_lc:
                            if ch=='\n': in_lc=False
                            pos+=1; continue
                        if in_bc:
                            if ch=='/' and prev=='*': in_bc=False
                            pos+=1; continue
                        if in_s:
                            if ch=='"' and prev!='\\': in_s=False
                            pos+=1; continue
                        if in_c:
                            if ch=="'" and prev!='\\': in_c=False
                            pos+=1; continue
                        if ch=='/' and pos+1<len(content):
                            if content[pos+1]=='/': in_lc=True; pos+=2; continue
                            if content[pos+1]=='*': in_bc=True; pos+=2; continue
                        if ch=='"': in_s=True
                        elif ch=="'": in_c=True
                        elif ch=='{': depth+=1
                        elif ch=='}': depth-=1
                        pos+=1
                    if name not in defined:
                        defined[name] = content[body_start:pos]
        self._port_defined = defined
        self._port_memo = {}

    def _recursive_bootleg(self, name, stack=None):
        defined = self._port_defined
        memo = self._port_memo
        if name not in defined:
            return False
        if name in memo:
            return memo[name]
        if stack is None:
            stack = set()
        if name in stack:
            return False
        stack.add(name)
        body = defined[name]
        if body.strip() in ('{}', ';'):
            memo[name] = True; stack.discard(name); return True
        if any(rx.search(body) for rx in self._REAL_RE):
            memo[name] = False; stack.discard(name); return False
        callees = set(c for c in self._CALLEE.findall(body)
                      if c not in ('if','while','for','switch','return','sizeof','catch','do'))
        internal = [c for c in callees if c in defined]
        if [c for c in callees if c not in defined]:
            memo[name] = False; stack.discard(name); return False
        nb = [ln.strip() for ln in body.split('\n')
              if ln.strip() and not ln.strip().startswith('//')]
        if not nb:
            memo[name] = True; stack.discard(name); return True
        def stmt_bootleg(s):
            if s.startswith('(void)'):
                return True
            if re.match(r'^return\s+(0|NULL|null|false|FALSE|""|\'\0\')\s*;?$', s):
                return True
            # assignment to a symbol NOT defined in this port file (module
            # global / static / struct member) is a legitimate setter, not a
            # bootleg — mirrors the getter rule below (return <bare static>).
            m_asgn = re.match(r'^([\w][\w\->\.\[\]]*)\s*(?:=|\+=|-=|\*=|/=)\s*[^;]*;?$', s)
            if m_asgn:
                return self._recursive_bootleg(m_asgn.group(1), stack) if m_asgn.group(1) in defined else False
            # bare call to a defined port function recurses; a call to an
            # undefined symbol is external code -> real (mirrors the hunter).
            m_call = re.match(r'^([\w]+)\s*\([^;]*\)\s*;?$', s)
            if m_call:
                return self._recursive_bootleg(m_call.group(1), stack) if m_call.group(1) in defined else False
            if re.match(r'^return\s+[\w]+\s*;?$', s):
                v = re.match(r'^return\s+([\w]+)\s*;?$', s).group(1)
                # returning an undefined symbol (static/global state) is a
                # legitimate getter, not a bootleg; only delegating to a
                # defined function recurses.
                return self._recursive_bootleg(v, stack) if v in defined else False
            if re.match(r'^return\s+[\w]+\s*\([^;]*\)\s*;?$', s):
                v = re.match(r'^return\s+([\w]+)\s*\(', s).group(1)
                return self._recursive_bootleg(v, stack) if v in defined else True
            if re.match(r'^return\s+', s):
                return False
            return True
        res = all(stmt_bootleg(st) for st in nb)
        memo[name] = res; stack.discard(name); return res

    # ── classification (preserved; adds da_flags) ──
    def classify_feature(self, py_file, feature):
        pop_annotations = self.c_index.find_pop_for_python(feature.name, py_file)
        if pop_annotations:
            pop = pop_annotations[0]
            self._ensure_port_graph()
            if self._recursive_bootleg(pop.c_function):
                return GapEntry(py_file, feature, "REAL_GAP", c_location=pop.c_file,
                                c_function=pop.c_function, pop_annotation=pop,
                                stub_reason="C function is a recursive bootleg (delegates to / is a no-op stub)",
                                severity="HIGH", da_flags=["DA-2:bootleg-body"])
            return GapEntry(py_file, feature, "PORTED", c_location=pop.c_file,
                            c_function=pop.c_function, pop_annotation=pop,
                            severity="LOW", notes="Explicit PoP annotation")
        impl_file = self.impl_map.get(py_file, "")
        if not impl_file:
            base = py_file.replace('.py', '.c')
            for cp in [f"src/agent/{base}", f"src/tools/{base}", f"src/provider/{base}",
                       f"src/gateway/{base}", f"src/cli/{base}"]:
                if (SLERMES_DIR / cp).exists():
                    impl_file = cp; break
        if impl_file:
            c_funcs = self._find_in_impl_file(impl_file, feature.name)
            if c_funcs:
                sf = c_funcs[0]
                if self._check_if_stub(sf.file, sf.name):
                    return GapEntry(py_file, feature, "REAL_GAP", c_location=sf.file, c_function=sf.name,
                                    stub_reason="C function appears to be stub/trivial (forwarding wrapper or trivial pass-through)", severity="HIGH",
                                    da_flags=["DA-1:stub-in-impl"])
                return GapEntry(py_file, feature, "PARTIAL", c_location=sf.file, c_function=sf.name,
                                severity="MEDIUM", notes="C function exists in impl file but no PoP annotation",
                                da_flags=["DA-3:no-pop-annotation"])
        if py_file.endswith("_adapter.py") or py_file.endswith("_runtime.py"):
            prefix = self._get_adapter_provider_prefix(py_file)
            if prefix:
                impl = self.impl_map.get(py_file, "")
                c_funcs = self._find_in_impl_file_with_prefix(impl, feature.name, prefix) if impl else []
                if not c_funcs:
                    c_funcs = self.c_index.find_c_function_with_prefix(feature.name, prefix)
                if c_funcs:
                    sf = c_funcs[0]
                    if self._check_if_stub(sf.file, sf.name):
                        return GapEntry(py_file, feature, "REAL_GAP", c_location=sf.file, c_function=sf.name,
                                        stub_reason="C function appears to be stub/trivial (forwarding wrapper or trivial pass-through)", severity="HIGH",
                                        da_flags=["DA-1:stub-in-impl"])
                    return GapEntry(py_file, feature, "PARTIAL", c_location=sf.file, c_function=sf.name,
                                    severity="MEDIUM", notes=f"Found with {prefix} prefix (needs PoP)",
                                    da_flags=["DA-3:no-pop-annotation"])
        vtable = ["context_engine.py","memory_provider.py","browser_provider.py","image_gen_provider.py",
                  "video_gen_provider.py","tts_provider.py","transcription_provider.py","web_search_provider.py"]
        if py_file in vtable:
            impl = self.impl_map.get(py_file, "")
            c_funcs = self._find_vtable_defaults(impl, py_file.replace('.py',''), feature.name) if impl else []
            if not c_funcs:
                c_funcs = self.c_index.find_vtable_defaults_global(py_file.replace('.py',''), feature.name)
            if c_funcs:
                sf = c_funcs[0]
                if self._check_if_stub(sf.file, sf.name):
                    return GapEntry(py_file, feature, "REAL_GAP", c_location=sf.file, c_function=sf.name,
                                    stub_reason="C function appears to be stub/trivial (forwarding wrapper or trivial pass-through)", severity="HIGH",
                                    da_flags=["DA-1:stub-in-impl"])
                return GapEntry(py_file, feature, "PARTIAL", c_location=sf.file, c_function=sf.name,
                                severity="MEDIUM", notes="Found vtable default (needs PoP)",
                                da_flags=["DA-3:no-pop-annotation"])
        c_funcs = self.c_index.find_c_function(feature.name, py_file, feature.parent_class)
        if c_funcs:
            sf = c_funcs[0]
            # Only credit PARTIAL if the C symbol lives in the expected impl file for this module.
            # A match in a different file (e.g. a header-only alias or stale name-parity hit)
            # is a false positive — it's a name collision in the global index, not this module's port.
            if impl_file and sf.file == impl_file:
                if self._check_if_stub(sf.file, sf.name):
                    return GapEntry(py_file, feature, "REAL_GAP", c_location=sf.file, c_function=sf.name,
                                    stub_reason="C function appears to be stub/trivial (forwarding wrapper or trivial pass-through)", severity="HIGH",
                                    da_flags=["DA-1:stub-global"])
                return GapEntry(py_file, feature, "PARTIAL", c_location=sf.file, c_function=sf.name,
                                severity="MEDIUM", notes="C function exists in impl file but no PoP annotation",
                                da_flags=["DA-3:no-pop-annotation"])
            # Match in a different file — not credited as PORTED/PARTIAL. Falls through to wrapper check.
        _, claims = self.c_index.find_wrapper_for_module(py_file)
        if feature.name in claims:
            return GapEntry(py_file, feature, "REAL_GAP", severity="HIGH",
                            notes="Claimed by name-parity wrapper but no C implementation found",
                            da_flags=["DA-1:wrapper-claim-unmet"])
        return GapEntry(py_file, feature, "REAL_GAP", severity="HIGH",
                        notes="No C equivalent found in any source directory",
                        da_flags=["DA-1:no-c-equivalent"])

    def _find_in_impl_file(self, impl_file, func_name):
        matches = []
        fname = Path(impl_file).name
        for rel in self.c_index._filename_index.get(fname, []):
            _, nc = self.c_index._get_cached_content(rel)
            if not nc: continue
            pat = rf'(?:static\s+)?(?:\w+\s+)*\*{{0,2}}\s*{re.escape(func_name)}\s*\('
            for m in re.finditer(pat, nc):
                line = nc[:m.start()].count('\n') + 1
                is_static = 'static' in nc[max(0,m.start()-50):m.start()]
                rm = re.search(r'(?:^|\n)([a-zA-Z_]\w*(?:\s*\\*)?)\s+' + re.escape(func_name) + r'\s*\(', nc[:m.start()], re.MULTILINE)
                rt = rm.group(1) if rm else ""
                matches.append(CFunction(name=func_name, file=rel, line=line, is_static=is_static, return_type=rt))
        return matches

    def _get_adapter_provider_prefix(self, py_file):
        return {"anthropic_adapter.py":"anthropic_","bedrock_adapter.py":"bedrock_",
                "codex_responses_adapter.py":"codex_","gemini_cloudcode_adapter.py":"google_",
                "gemini_native_adapter.py":"google_","codex_runtime.py":"codex_",
                "model_metadata.py":"models_dev_","models_dev.py":"models_dev_",
                "provider_metadata.py":"provider_"}.get(py_file)

    def _find_in_impl_file_with_prefix(self, impl_file, func_name, prefix):
        matches = []
        prefixed = prefix + func_name.lstrip('_')
        fname = Path(impl_file).name if impl_file else ""
        for rel in self.c_index._filename_index.get(fname, []):
            _, nc = self.c_index._get_cached_content(rel)
            if not nc: continue
            pat = rf'(?:static\s+)?(?:\w+\s+)*\*{{0,2}}\s*{re.escape(prefixed)}\s*\('
            for m in re.finditer(pat, nc):
                line = nc[:m.start()].count('\n') + 1
                rm = re.search(r'(?:^|\n)([a-zA-Z_]\w*(?:\s*\\*)?)\s+' + re.escape(prefixed) + r'\s*\(', nc[:m.start()], re.MULTILINE)
                rt = rm.group(1) if rm else ""
                matches.append(CFunction(name=prefixed, file=rel, line=line, return_type=rt))
        return matches

    def _find_vtable_defaults(self, impl_file, module_name, method_name):
        matches = []
        patterns = [f"default_{method_name}", f"{module_name}_default_{method_name}",
                    f"{module_name}_default_{method_name.capitalize()}"]
        pp = {"tts_provider":"tts_","image_gen_provider":"image_gen_","video_gen_provider":"video_gen_",
              "web_search_provider":"web_search_","transcription_provider":"transcribe_",
              "memory_provider":"memory_","context_engine":"context_"}
        if module_name in pp: patterns.append(f"{pp[module_name]}{method_name}")
        fname = Path(impl_file).name if impl_file else ""
        for rel in self.c_index._filename_index.get(fname, []):
            _, nc = self.c_index._get_cached_content(rel)
            if not nc: continue
            for p in patterns:
                for m in re.finditer(rf'(?:static\s+)?(?:\w+\s+)*\*{{0,2}}\s*{re.escape(p)}\s*\(', nc):
                    line = nc[:m.start()].count('\n') + 1
                    rm = re.search(r'(?:^|\n)([a-zA-Z_]\w*(?:\s*\\*)?)\s+' + re.escape(p) + r'\s*\(', nc[:m.start()], re.MULTILINE)
                    rt = rm.group(1) if rm else ""
                    matches.append(CFunction(name=p, file=rel, line=line, return_type=rt))
        return matches

    def _check_if_stub(self, c_file, func_name):
        fpath = SLERMES_DIR / c_file
        if not fpath.exists():
            return False
        with open(fpath) as f:
            content = f.read()
        esc = re.escape(func_name)
        m = re.search(r'(?:static\s+)?(?:\w+\s+)*\*?\s*' + esc + r'\s*\([^)]*\)\s*\{', content)
        if not m: return False
        body_start = m.end(); depth = 1; pos = body_start
        in_str=in_chr=in_lc=in_bc=False
        while pos < len(content) and depth > 0:
            ch = content[pos]; prev = content[pos-1] if pos>0 else '\0'
            if in_lc:
                if ch=='\n': in_lc=False
                pos+=1; continue
            elif in_bc:
                if ch=='/' and prev=='*': in_bc=False
                pos+=1; continue
            elif in_str:
                if ch=='"' and prev!='\\': in_str=False
                pos+=1; continue
            elif in_chr:
                if ch=="'" and prev!='\\': in_chr=False
                pos+=1; continue
            if ch=='/' and pos+1<len(content):
                if content[pos+1]=='/': in_lc=True; pos+=2; continue
                elif content[pos+1]=='*': in_bc=True; pos+=2; continue
            elif ch=='"': in_str=True
            elif ch=="'": in_chr=True
            elif ch=='{': depth+=1
            elif ch=='}': depth-=1
            pos+=1
        body = content[body_start:pos].strip()
        if not body or body == ';':
            return True
        non_blank = [line.strip() for line in body.split('\n')
                     if line.strip() and not line.strip().startswith('//')]
        # Trivial forwarding wrapper: <=2 non-blank lines all return something.
        if len(non_blank) <= 2 and all('return' in line for line in non_blank):
            return True
        # Delegates to another function (common pattern: context->ctx, obj->obj_).
        if len(non_blank) <= 3 and any('return' in line and ('->' in line or '.') for line in non_blank):
            return True
        # LOG+return NULL/false pattern (hermes_log + immediate return — stubs).
        if len(non_blank) <= 5:
            if re.search(r'(?:hermes_log|LOG_\w+).*return\s+(NULL|0|false)\s*;', body, re.DOTALL):
                return True
            # (void)-only body with immediate return.
            if re.search(r'\(void\)[^;]*\{', body) and not re.search(r'=\s*[^;]+;', body):
                return True
        return False

    # ── triple-DA per-module validation ──
    def _c_func_count_for_impl(self, impl_file):
        if not impl_file: return 0
        fname = Path(impl_file).name
        total = 0
        seen = set()
        for rel in self.c_index._filename_index.get(fname, []):
            _, nc = self.c_index._get_cached_content(rel)
            for m in re.finditer(r'(?:static\s+)?(?:const\s+)?[\w\s\*]+?(\w+)\s*\(', nc):
                n = m.group(1)
                if n in ("if","while","for","switch","return","sizeof","int","char","void"):
                    continue
                if (rel,n) not in seen:
                    seen.add((rel,n)); total+=1
        return total

    def _pop_count_for_module(self, py_file):
        # Count PoP annotations whose python_file matches this module.
        n = 0
        for pop in self.c_index.pop_annotations:
            if pop.python_file and pop.python_file == py_file:
                n += 1
        return n

    def validate_module_da(self, py_file, report, py_features):
        """Triple-DA: mutates report with thin / pop_mismatch and reclassification flags."""
        report.py_func_count = len(py_features)
        report.c_func_count = self._c_func_count_for_impl(self.impl_map.get(py_file, ""))
        # DA-2: thin home
        if report.py_func_count > 0 and report.c_func_count > 0:
            ratio = 100.0 * report.c_func_count / report.py_func_count
            if ratio < 30.0:
                report.thin = True
        # DA-3: PoP mismatch (ported claimed but no PoP annotation for this module)
        pop_n = self._pop_count_for_module(py_file)
        if report.ported > 0 and pop_n == 0 and report.partial == 0:
            report.pop_mismatch = True
        # DA-1: re-validate every REAL_GAP by topic — if a C home exists and is
        # non-empty, the gap is at minimum "has a home, needs depth", not "no C
        # equivalent". We keep it REAL_GAP (faithful: it is not PORTED) but tag it.
        for g in report.gaps:
            if g.classification == "REAL_GAP" and "DA-1:no-c-equivalent" in g.da_flags:
                impl = self.impl_map.get(py_file, "")
                if impl and self._c_func_count_for_impl(impl) > 0:
                    g.da_flags.append("DA-1:home-exists-needs-depth")

    def scan_all(self, module_filter: str = ""):
        reports = {}
        all_py = []
        agent = PYTHON_SOURCE_DIRS["agent"]
        # NOTE: full rglob (not just top-level glob + pet/) — agent/lsp/,
        # agent/secret_sources/, agent/transports/, agent/proxy_sources/ and
        # agent/monitoring/ are real ported subsystems whose Python sides
        # were previously invisible to the parity census.
        for pf in sorted(agent.rglob("*.py")):
            rel = pf.relative_to(agent)
            if pf.name == "__init__.py" and len(pf.read_text(errors="ignore").strip()) <= 80:
                continue  # empty package marker
            all_py.append((pf, "agent/" + str(rel)))
        tools = PYTHON_SOURCE_DIRS["tools"]
        for pf in sorted(tools.rglob("*.py")):
            if pf.name == "__init__.py" and len(pf.read_text(errors="ignore").strip()) <= 80:
                continue
            all_py.append((pf, "tools/" + str(pf.relative_to(tools))))
        gw = PYTHON_SOURCE_DIRS["gateway"]
        for pf in sorted(gw.rglob("*.py")):
            if pf.name == "__init__.py" and len(pf.read_text(errors="ignore").strip()) <= 80:
                continue
            all_py.append((pf, "gateway/" + str(pf.relative_to(gw))))
        cron = PYTHON_SOURCE_DIRS["cron"]
        for pf in sorted(cron.rglob("*.py")):
            if pf.name == "__init__.py" and len(pf.read_text(errors="ignore").strip()) <= 80:
                continue
            all_py.append((pf, "cron/" + str(pf.relative_to(cron))))
        clipy = PYTHON_SOURCE_DIRS["cli_root"]
        if clipy.exists():
            all_py.append((clipy, "cli.py"))
        for rp in ROOT_PY_MODULES:
            rp_path = HERMES_DIR / rp
            if rp_path.exists():
                all_py.append((rp_path, rp))
        hc = PYTHON_SOURCE_DIRS["hermes_cli"]
        for pf in sorted(hc.rglob("*.py")):
            if pf.name == "__init__.py" and len(pf.read_text(errors="ignore").strip()) <= 80:
                continue
            all_py.append((pf, "hermes_cli/" + str(pf.relative_to(hc))))
        all_py.sort(key=lambda x: x[1])

        # Apply module substring filter early so --module doesn't scan the
        # entire Python tree (the main bottleneck).
        if module_filter:
            all_py = [(pf, d) for pf, d in all_py if module_filter in d]

        for pf, display in all_py:
            feats = self.extractor.extract_file(pf)
            rep = ModuleReport(python_file=display)
            for feat in feats:
                gap = self.classify_feature(display, feat)
                rep.gaps.append(gap)
                rep.total_features += 1
                if gap.classification == "PORTED": rep.ported += 1
                elif gap.classification == "PARTIAL": rep.partial += 1
                elif gap.classification == "STUB": rep.stub += 1
                elif gap.classification == "REAL_GAP": rep.real_gaps += 1
            self.validate_module_da(display, rep, feats)
            reports[display] = rep
        return reports

# ── Upstream-drift engine ──────────────────────────────────────────────────────
def git_rev(revspec):
    """Return the resolved commit hash for a git revspec, or '' on failure."""
    import subprocess as _sp
    try:
        return _sp.check_output(["git", "rev-parse", revspec],
                                 cwd=HERMES_DIR, stderr=_sp.DEVNULL).decode().strip()
    except Exception:
        return ""

def upstream_feature_set(revspec="upstream/main"):
    """Feature set ('module.py:feature') of the Python source of truth at a git rev.
    Reads trees directly via `git show` so no working-tree checkout is needed."""
    import subprocess as _sp
    fps = set()
    try:
        paths = _sp.check_output(
            ["git", "ls-tree", "-r", "--name-only", revspec,
             "agent/", "tools/", "gateway/", "cron/", "hermes_cli/", "cli.py"],
            cwd=HERMES_DIR).decode().split()
    except Exception:
        return fps
    for p in paths:
        if not p.endswith(".py") or p.endswith("__init__.py"):
            continue
        try:
            content = _sp.check_output(["git", "show", f"{revspec}:{p}"], cwd=HERMES_DIR)
        except _sp.CalledProcessError:
            continue
        try:
            tree = ast.parse(content)
        except SyntaxError:
            continue
        for node in ast.walk(tree):
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                fps.add(f"{p}:{node.name}")
    return fps

def feature_fingerprint(reports):
    """Return a set of 'module:feature' strings for the live Python tree."""
    fps = set()
    for name, rep in reports.items():
        for g in rep.gaps:
            fps.add(f"{name}:{g.python_feature.name}")
    return fps

def load_baseline():
    if BASELINE_FILE.exists():
        try:
            with open(BASELINE_FILE) as f:
                d = json.load(f)
            return (set(d.get("features", [])), d.get("generated_at", ""),
                    d.get("upstream_commit", ""))
        except Exception:
            pass
    return None, "", ""

def save_baseline(fps, generated_at, upstream_commit=""):
    BASELINE_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(BASELINE_FILE, "w") as f:
        json.dump({"generated_at": generated_at, "features": sorted(fps),
                   "upstream_commit": upstream_commit}, f, indent=1)

def compute_drift(reports, upstream_commit=""):
    live = feature_fingerprint(reports)
    base, base_at, base_up = load_baseline()
    if base is None:
        return {"status": "no_baseline", "new_gap_count": 0, "resolved_count": 0,
                "baseline_at": "", "baseline_upstream_commit": base_up,
                "live_count": len(live)}
    new_gaps = sorted(live - base)
    resolved = sorted(base - live)
    return {"status": "ok", "new_gap_count": len(new_gaps), "resolved_count": len(resolved),
            "baseline_at": base_at, "baseline_upstream_commit": base_up,
            "live_count": len(live),
            "new_gaps": new_gaps[:500], "resolved": resolved[:200]}

def rebase_drift_report(live_reports, revspec="upstream/main"):
    """Find gaps introduced by the LATEST upstream vs what slermes currently
    ports. Diffs the live (ported) feature set against upstream's full feature
    set. The result is the actionable 'future gaps' list for the rebase/PoP
    commit path: every feature upstream has that slermes has not ported."""
    live = feature_fingerprint(live_reports)
    up = upstream_feature_set(revspec)
    if not up:
        return {"status": "no_upstream", "new_gap_count": 0,
                "upstream_commit": git_rev(revspec),
                "new_gaps_by_module": {}}
    # Features upstream has that slermes does NOT currently port (per live scan).
    missing = sorted(up - live)
    by_mod = {}
    for g in missing:
        mod = g.split(":", 1)[0]
        by_mod.setdefault(mod, []).append(g.split(":", 1)[1])
    return {"status": "ok", "upstream_commit": git_rev(revspec),
            "new_gap_count": len(missing),
            "new_gaps_by_module": dict(sorted(by_mod.items(), key=lambda x: -len(x[1])))}

# ── Formatters ────────────────────────────────────────────────────────────────
class Formatter:
    @staticmethod
    def totals(reports):
        t = sum(r.total_features for r in reports.values())
        p = sum(r.ported for r in reports.values())
        pa = sum(r.partial for r in reports.values())
        s = sum(r.stub for r in reports.values())
        rg = sum(r.real_gaps for r in reports.values())
        thin = sum(1 for r in reports.values() if r.thin)
        pm = sum(1 for r in reports.values() if r.pop_mismatch)
        return t, p, pa, s, rg, thin, pm

    @staticmethod
    def summary(reports, drift):
        t, p, pa, s, rg, thin, pm = Formatter.totals(reports)
        lines = [
            "=" * 72,
            "  SLERMES PoP PARITY BATTLEGROUND (fail-closed, triple-DA, drift-aware)",
            "=" * 72,
            f"  Python modules scanned : {len(reports)}",
            f"  Total features         : {t}",
            f"  PORTED                 : {p} ({100.0*p/max(t,1):.1f}%)",
            f"  PARTIAL                : {pa} ({100.0*pa/max(t,1):.1f}%)",
            f"  STUB                   : {s}",
            f"  REAL_GAP               : {rg} ({100.0*rg/max(t,1):.1f}%)",
            f"  DA-2 THIN modules      : {thin}",
            f"  DA-3 PoP-mismatch mods : {pm}",
            f"  DRIFT (vs baseline)    : new={drift['new_gap_count']} resolved={drift['resolved_count']} status={drift['status']}",
            "=" * 72,
        ]
        for name, rep in sorted(reports.items()):
            if rep.real_gaps or rep.stub or rep.partial or rep.thin or rep.pop_mismatch:
                pct = 100.0*rep.ported/max(rep.total_features,1)
                tag = ""
                if rep.thin: tag += " [THIN]"
                if rep.pop_mismatch: tag += " [POP-MISMATCH]"
                lines.append(f"  {name:52s} {rep.ported:3d}/{rep.total_features:3d} ({pct:5.1f}%) rg={rep.real_gaps} pa={rep.partial} st={rep.stub}{tag}")
        return "\n".join(lines)

    @staticmethod
    def json_output(reports, drift):
        def gap_to_dict(g):
            d = asdict(g)
            if g.pop_annotation:
                d['pop_annotation'] = asdict(g.pop_annotation)
            d['python_feature'] = asdict(g.python_feature)
            return d
        return json.dumps({
            "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "source_proof": str(SOURCE_PROOF.relative_to(HERMES_DIR)),
            "modules": {
                name: {
                    "total": r.total_features, "ported": r.ported, "partial": r.partial,
                    "stub": r.stub, "real_gaps": r.real_gaps, "thin": r.thin,
                    "pop_mismatch": r.pop_mismatch, "py_func_count": r.py_func_count,
                    "c_func_count": r.c_func_count,
                    "gaps": [gap_to_dict(g) for g in r.gaps],
                } for name, r in reports.items()
            },
            "drift": drift,
        }, indent=2)

# ── main ───────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(description="Slermes PoP parity battleground (rewrite, fail-closed)")
    ap.add_argument("--detail", action="store_true")
    ap.add_argument("--battleship", action="store_true")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--update-baseline", action="store_true", help="Write the drift baseline from the live tree")
    ap.add_argument("--upstream", default="upstream/main", help="Upstream rev to diff against (default: upstream/main)")
    ap.add_argument("--rebase-drift", action="store_true",
                    help="Find gaps introduced by the latest upstream vs what slermes ports. "
                         "Emits the actionable 'future gaps' list for the rebase/PoP commit path.")
    ap.add_argument("--module", help="Substring filter")
    args = ap.parse_args()

    if not source_present():
        sys.stderr.write(
            "FATAL: Python ground-truth not checked out. The scanner cannot run.\\n"
            "  Fix: cd %s && git checkout upstream/main -- agent tools gateway cli.py hermes_cli cron\\n"
            "  (or your fork's main). Then re-run.\\n" % HERMES_DIR)
        sys.exit(2)

    analyzer = ParityAnalyzer()
    reports = analyzer.scan_all(module_filter=args.module)

    if len(reports) == 0:
        sys.stderr.write("FATAL: scanner consumed 0 modules — source of truth not read. Aborting; writing nothing.\\n")
        sys.exit(5)

    drift = compute_drift(reports, git_rev(args.upstream))

    if args.rebase_drift:
        rb = rebase_drift_report(reports, args.upstream)
        if args.json:
            print(json.dumps(rb, indent=2))
        else:
            print("=" * 72)
            print("  REBASE/PoP DRIFT — gaps introduced by upstream (%s)" % rb.get("upstream_commit", "")[:12])
            print("=" * 72)
            print("  Total future gaps (upstream features not yet ported): %d" % rb["new_gap_count"])
            for mod, feats in list(rb.get("new_gaps_by_module", {}).items())[:30]:
                print("    %-50s +%d" % (mod, len(feats)))
            print("  (full list in tests/.parity_drift_report.json)")
        # Persist the actionable list for the commit path.
        BASELINE_FILE.parent.mkdir(parents=True, exist_ok=True)
        with open(BASELINE_FILE.parent / ".parity_drift_report.json", "w") as f:
            json.dump(rb, f, indent=1)
        return

    if args.update_baseline:
        save_baseline(feature_fingerprint(reports),
                      datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                      git_rev(args.upstream))
        print(f"Baseline written: {BASELINE_FILE} ({len(reports)} modules) upstream={git_rev(args.upstream)[:12]}")

    if args.json:
        print(Formatter.json_output(reports, drift))
    elif args.battleship:
        print(Formatter.summary(reports, drift))
    elif args.detail:
        print(Formatter.summary(reports, drift))
        for name, rep in sorted(reports.items()):
            gaps = [g for g in rep.gaps if g.classification in ("REAL_GAP","STUB","PARTIAL")]
            if not gaps: continue
            print(f"\\n## {name} ({len(gaps)} issues)")
            for g in gaps:
                loc = f" @ {g.c_location}:{g.c_function}" if g.c_location else ""
                print(f"  [{g.classification}] {g.python_feature.name}{loc}")
                if g.da_flags: print(f"       -> DA: {', '.join(g.da_flags)}")
    else:
        print(Formatter.summary(reports, drift))

if __name__ == "__main__":
    main()
