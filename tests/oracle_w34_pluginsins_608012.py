#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_install_scan.py — gate w34_pluginsins_608012.

The live plugin-guard scanner is imported and only its pure value functions are
compared, with the filesystem as the seam:

  * ``_filter_findings`` — called with the fixture's own source lines written to
    a scratch file, so the AST-derived line sets (``_docstring_lines`` /
    ``_main_guard_body_lines``) are the live ones for that exact text;
  * the severity algebra (``_at_most`` / ``_comment_severity`` /
    ``_docstring_cap``) and the context caps (``prose_cap`` / ``catalog_cap`` /
    ``is_agent_facing``);
  * path classification (``is_doc_prose`` / ``is_ci_workflow`` /
    ``is_locale_catalog`` / ``is_test_tree`` / comment prefix / excluded dirs);
  * ``_is_defensive_documentation``, ``logical_line``, the dangerous-findings
    summary and ``should_allow_plugin_install`` over a ``ScanResult``;
  * the structural size/binary/bytecode findings and the module constants.

Seams not under test: ``_walk``, ``scan_file``, ``_file_lines`` (the fixture
carries the lines), git bytecode tracking, and the regex family in
``plugins_install_scan_context`` that the C port leaves to the caller.
"""
import json
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w34_pluginsins_608012"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import plugins_install_scan as live  # noqa: E402
from hermes_cli import plugins_install_scan_context as ctx  # noqa: E402
from tools.skills_guard import SUSPICIOUS_BINARY_EXTENSIONS  # noqa: E402

Finding = live.Finding
ScanResult = live.ScanResult

WANTED = {
    "path_class", "should_allow", "dangerous_summary", "scan_summary", "provenance",
    "severity_at_most", "comment_severity", "docstring_cap", "prose_cap", "catalog_cap",
    "is_agent_facing", "is_defensive_documentation", "filter_findings", "logical_line",
    "limits", "size_finding", "binary_finding", "bytecode_finding", "manifest_findings",
}


def mk_finding(d):
    return Finding(
        d.get("pattern_id", ""), d.get("severity", ""), d.get("category", ""),
        d.get("file", ""), int(d.get("line", 0)), d.get("match", ""), d.get("description", ""))


def fdict(f):
    return {"pattern_id": f.pattern_id, "severity": f.severity, "category": f.category,
            "file": f.file, "line": f.line, "match": f.match, "description": f.description}


def path_class(rel):
    p = Path(rel)
    suffix = p.suffix
    return {
        "rel": rel,
        "name": p.name,
        "suffix": suffix,
        "parts": list(p.parts),
        "is_code": suffix.lower() in live.CODE_FILE_EXTENSIONS,
        "is_python": suffix.lower() == ".py",
        "is_js": suffix.lower() in {".js", ".ts"},
        "is_doc_prose": ctx.is_doc_prose(rel),
        "is_ci_workflow": ctx.is_ci_workflow(rel),
        "doc_prose": ctx.is_doc_prose(rel) or ctx.is_ci_workflow(rel),
        "is_locale_catalog": ctx.is_locale_catalog(rel),
        "is_test_tree": ctx.is_test_tree(rel),
        "is_changelog": p.name.lower() in live.CHANGELOG_FILENAMES,
        "comment_prefix": live.COMMENT_PREFIXES_BY_EXTENSION.get(suffix.lower()),
        "is_excluded": any(part in live.EXCLUDED_DIRS for part in p.parts),
        "is_bytecode": rel.endswith(live._BYTECODE_SUFFIXES),
    }


def materialize(rel, lines):
    """Write the fixture's lines where the live scanner would read them."""
    root = Path(tempfile.mkdtemp(prefix="gate_%s_" % GATE))
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")
    return path


def filter_findings(req):
    rel = req.get("rel") or ""
    lines = [str(x) for x in (req.get("lines") or [])]
    path = materialize(rel, lines)
    if path.suffix.lower() == ".py":
        want_doc = sorted(live._docstring_lines(path))
        want_guard = sorted(live._main_guard_body_lines(path))
    else:
        want_doc, want_guard = [], []
    got_doc = sorted(int(x) for x in (req.get("docstring_lines") or []))
    got_guard = sorted(int(x) for x in (req.get("main_guard_lines") or []))
    if (want_doc, want_guard) != (got_doc, got_guard):
        raise AssertionError("fixture AST line sets stale: live=(%r,%r) fixture=(%r,%r)"
                             % (want_doc, want_guard, got_doc, got_guard))
    findings = [mk_finding(d) for d in (req.get("findings") or [])]
    return [fdict(f) for f in live._filter_findings(findings, rel, path)]


def should_allow(req):
    findings = [mk_finding(d) for d in (req.get("findings") or [])]
    result = ScanResult(skill_name="plug", source="plug", trust_level="community",
                        verdict=req.get("verdict") or "", findings=findings)
    allowed, reason = live.should_allow_plugin_install(result, force=bool(req.get("force")))
    return {"allowed": allowed, "reason": reason}


def scan_summary(req):
    findings = [mk_finding(d) for d in (req.get("findings") or [])]
    name = req.get("name") or ""
    verdict = req.get("verdict") or ""
    if findings:
        categories = sorted({f.category for f in findings})
        return f"{name}: {verdict} — {len(findings)} finding(s) in {', '.join(categories)}"
    return f"{name}: clean scan, no threats detected"


def provenance(req):
    return {"scanner_version": live.PLUGIN_SCANNER_VERSION,
            "verdict": req.get("verdict") or "", "source": req.get("source") or ""}


def limits():
    return {
        "scanner_version": live.PLUGIN_SCANNER_VERSION,
        "max_plugin_file_count": live.MAX_PLUGIN_FILE_COUNT,
        "max_plugin_total_size_kb": live.MAX_PLUGIN_TOTAL_SIZE_KB,
        "max_plugin_single_file_kb": live.MAX_PLUGIN_SINGLE_FILE_KB,
        "excluded_dirs": sorted(live.EXCLUDED_DIRS),
        "code_file_extensions": sorted(live.CODE_FILE_EXTENSIONS),
        "changelog_filenames": sorted(live.CHANGELOG_FILENAMES),
        "bytecode_suffixes": sorted(live._BYTECODE_SUFFIXES),
        "suspicious_binary_extensions": sorted(SUSPICIOUS_BINARY_EXTENSIONS),
        "code_exempt_pattern_ids": sorted(live.CODE_EXEMPT_PATTERN_IDS),
        "prose_keep_ids": sorted(ctx._PROSE_KEEPS_FULL_SEVERITY_IDS),
        "catalog_ids": sorted(ctx._CATALOG_STEPS_DOWN_IDS),
        "comment_prefixes": dict(live.COMMENT_PREFIXES_BY_EXTENSION),
        "comment_severity_cap": dict(live._COMMENT_SEVERITY_CAP),
        "severity_remap": dict(live.SEVERITY_REMAP),
        "js_capability_remap": dict(live.JS_CAPABILITY_REMAP),
        "doc_prose_demotions": dict(live.DOC_PROSE_DEMOTIONS),
        "main_guard_demotions": dict(live.MAIN_GUARD_DEMOTIONS),
        "severity_rank": dict(live._SEVERITY_RANK),
    }


def size_finding(req):
    rel = req.get("rel") or ""
    size = int(req.get("size") or 0)
    if size > live.MAX_PLUGIN_SINGLE_FILE_KB * 1024:
        return fdict(live._finding("oversized_file", "medium", "structural", rel,
                                   f"{size // 1024}KB",
                                   f"file is {size // 1024}KB (limit: {live.MAX_PLUGIN_SINGLE_FILE_KB}KB)"))
    return None


def binary_finding(req):
    rel = req.get("rel") or ""
    ext = Path(rel).suffix.lower()
    if ext in SUSPICIOUS_BINARY_EXTENSIONS:
        return fdict(live._finding("binary_file", live.SEVERITY_REMAP["binary_file"], "structural", rel,
                                   f"binary: {ext}",
                                   f"binary/executable file ({ext}) bundled in plugin (cannot be scanned)"))
    return None


def bytecode_finding(req):
    rel = req.get("rel") or ""
    return fdict(live._finding("bytecode_file", live.SEVERITY_REMAP["binary_file"], "structural", rel,
                               "committed bytecode",
                               "committed Python bytecode runs in place of the scanned source (catalog rule 17)"))


def manifest_findings(req):
    fc = int(req.get("file_count") or 0)
    ts = int(req.get("total_size") or 0)
    out = []
    if fc > live.MAX_PLUGIN_FILE_COUNT:
        out.append(fdict(live._finding("too_many_files", "medium", "structural", "(directory)",
                                       f"{fc} files",
                                       f"plugin has {fc} files (limit: {live.MAX_PLUGIN_FILE_COUNT})")))
    if ts > live.MAX_PLUGIN_TOTAL_SIZE_KB * 1024:
        out.append(fdict(live._finding("oversized_bundle", "medium", "structural", "(directory)",
                                       f"{ts // 1024}KB",
                                       f"plugin is {ts // 1024}KB total (limit: {live.MAX_PLUGIN_TOTAL_SIZE_KB}KB)")))
    return out


def expected(r):
    f = r.get("func")
    if f == "path_class":
        return path_class(r.get("rel") or "")
    if f == "should_allow":
        return should_allow(r)
    if f == "dangerous_summary":
        return live._dangerous_findings_summary([mk_finding(d) for d in (r.get("findings") or [])])
    if f == "scan_summary":
        return scan_summary(r)
    if f == "provenance":
        return provenance(r)
    if f == "severity_at_most":
        return live._at_most(r.get("sev") or "", r.get("cap") or "")
    if f == "comment_severity":
        return live._comment_severity(mk_finding(
            {"pattern_id": r.get("pattern_id"), "category": r.get("category"), "severity": r.get("sev")}))
    if f == "docstring_cap":
        return live._docstring_cap(mk_finding(
            {"pattern_id": r.get("pattern_id"), "category": r.get("category"), "severity": r.get("sev")}))
    if f == "prose_cap":
        return ctx.prose_cap(mk_finding(
            {"pattern_id": r.get("pattern_id"), "category": r.get("category"), "severity": r.get("sev")}))
    if f == "catalog_cap":
        return ctx.catalog_cap(mk_finding(
            {"pattern_id": r.get("pattern_id"), "category": r.get("category"), "severity": r.get("sev")}))
    if f == "is_agent_facing":
        return ctx.is_agent_facing(mk_finding(
            {"pattern_id": r.get("pattern_id"), "category": r.get("category"), "severity": "high"}))
    if f == "is_defensive_documentation":
        return live._is_defensive_documentation(
            mk_finding({"pattern_id": "x", "severity": "high", "category": "c", "file": r.get("rel"),
                        "line": int(r.get("line") or 0), "match": r.get("match")}),
            r.get("rel") or "", set(int(x) for x in (r.get("docstring_lines") or [])))
    if f == "filter_findings":
        return filter_findings(r)
    if f == "logical_line":
        lines = [str(x) for x in (r.get("lines") or [])]
        return ctx.logical_line(lines, int(r.get("index") or 0))
    if f == "limits":
        return limits()
    if f == "size_finding":
        return size_finding(r)
    if f == "binary_finding":
        return binary_finding(r)
    if f == "bytecode_finding":
        return bytecode_finding(r)
    if f == "manifest_findings":
        return manifest_findings(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
