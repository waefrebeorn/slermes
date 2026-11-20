#!/usr/bin/env python3
"""Oracle for the hermes_cli/plugins_install_scan_context.py UNCLAIMED FLOW port
(src/cli/plugins_install_scan_flow2.c, prefix pis2_).

Drives the LIVE context-demotion predicates (all 38 module defs, AST-extracted
from the read-only quarry; no heavy import chain) with plain JSON inputs and
compares the canonical return against the C harness.

Regex is SEAMED: every module-level compiled pattern is replaced by a play
double that consumes the per-case ``io[name]`` queue recorded (from the REAL
pattern) by gen_plugins_install_scan_flow2_cases.py. Both sides therefore see
byte-identical match data; the port under test is the surrounding predicate
logic (set membership, all/any, span containment, Path algebra, base64 media
sniffing with the full builtin exception hierarchy). Internal composition is
REAL on both sides (prose_cap -> is_agent_facing; catalog_cap -> prose_cap;
_executes; _sudo_in_safety_clause; _decoded_head; logical_line).

Representation contract (both sides, see src/cli/plugins_install_scan_flow2.c):
- bool/str/None project to JSON bool/string/null; bytes -> array of ints.
- uncaught exception -> {"__raise__": cls}; *_in_safety_clause gates a negation.
- fixture domain: ASCII text/line operands, ASCII rel_paths, integer spans.
Fail-closed: 0 cases => exit 1. Summary:
'ORACLE plugins_install_scan_flow2: N cases, M mismatches'.
"""
import __future__
import builtins
import json
import re
import sys
import types
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "plugins_install_scan_context.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "plugins_install_scan_context.py")

NAME = "plugins_install_scan_flow2"

TARGETS = [
    "is_doc_prose", "is_ci_workflow", "is_agent_facing", "prose_cap",
    "_is_trusted_installer_url", "is_trusted_installer_pipe", "is_locale_catalog",
    "catalog_cap", "_uninstall_operand", "is_self_uninstall_doc",
    "_exec_first_arg_literal_spans", "_executes", "is_inert_fixture_line",
    "is_test_tree", "_decoded_head", "is_base64_media",
    "is_base64_archive_roundtrip", "_is_alternation_member", "_is_whole_literal",
    "is_regex_alternation_token", "_sudo_in_safety_clause", "is_sudo_inert",
    "is_env_dump_in_safety_clause", "is_quoted_injection_phrase",
    "is_repo_root_traversal", "is_benign_lazy_os_import", "is_disclosed_config_note",
    "is_authorized_keys_prose", "is_loopback_only", "is_pip_install_in_prose_literal",
    "is_hex_in_char_class", "logical_line", "is_loopback_continuation",
    "is_json_prose_value", "_is_standalone_coin_name", "is_coin_name_only",
    "is_public_client_key", "is_own_proc_read",
]
WANTED = set(TARGETS)

_SRCTEXT = Path(SRC).read_text(encoding="utf-8")
_FLAGS = __future__.annotations.compiler_flag
_CODE = compile(_SRCTEXT, SRC, "exec", flags=_FLAGS, dont_inherit=True)

# ── namespace construction ────────────────────────────────────────────────────
CUR = {"io": {}, "pos": {}}


def _seam(name):
    """Pop the next recorded result for seam *name* (fail-closed if missing)."""
    q = CUR["io"].get(name)
    i = CUR["pos"].get(name, 0)
    if not isinstance(q, list) or i >= len(q):
        raise RuntimeError("seam exhausted: " + name)
    CUR["pos"][name] = i + 1
    return q[i]


class _Match:
    """Match object projection over a recorded {s,e} descriptor."""
    def __init__(self, d, line=""):
        if isinstance(d, dict):
            self._s = d.get("s", 0)
            self._e = d.get("e", 0)
            self._g0 = d.get("t", line[self._s:self._e])
            self._g = d.get("g", [])
        else:
            self._s = self._e = 0
            self._g0 = str(d)
            self._g = [str(d)]

    def start(self, n=0):
        return self._s

    def end(self, n=0):
        return self._e

    def span(self):
        return (self._s, self._e)

    def group(self, n=0):
        if n == 0:
            return self._g0
        return self._g[n - 1] if n - 1 < len(self._g) else ""


class _Rx:
    """Play double for a compiled regex; all methods consume the named queue."""
    def __init__(self, name):
        self.name = name

    def finditer(self, line, *a):
        return [_Match(d, line) for d in _seam(self.name)]

    def search(self, line, *a):
        r = _seam(self.name)
        if not r:
            return None
        return _Match(r, line)

    def match(self, line, *a):
        r = _seam(self.name)
        if not r:
            return None
        return _Match(r, line)

    def sub(self, repl, line, *a):
        return _seam(self.name)   # recorded substituted line

    # search/match returning a plain bool descriptor
    def bool_search(self, line, *a):
        return bool(_seam(self.name))


class BoolRx:
    """Play double whose search()/match() return None or a truthy match, so
    both `is not None` and boolean-context call sites behave like re.Pattern."""
    def __init__(self, name):
        self.name = name

    def search(self, line, *a):
        return _Match({}) if _seam(self.name) else None

    def match(self, line, *a):
        return _Match({}) if _seam(self.name) else None


class StrRx:
    """Play double: finditer() -> list of group strings (or None per entry)."""
    def __init__(self, name):
        self.name = name

    def finditer(self, line, *a):
        rs = _seam(self.name)
        return [_Match({"g": [r]}, line) for r in rs]

    def search(self, line, *a):
        r = _seam(self.name)
        if r is None or r == "":
            return None
        return _Match({"g": [r]}, line)


class IntRx:
    """Play double: finditer() -> list of match START ints; search()->bool."""
    def __init__(self, name):
        self.name = name

    def finditer(self, line, *a):
        return [_Match({"s": int(v), "e": int(v)}, line) for v in _seam(self.name)]

    def search(self, line, *a):
        return bool(_seam(self.name))

    def match(self, line, *a):
        return bool(_seam(self.name))


class EndRx:
    """Play double: finditer() -> list of match END ints."""
    def __init__(self, name):
        self.name = name

    def finditer(self, line, *a):
        return [_Match({"s": int(v), "e": int(v)}, line) for v in _seam(self.name)]

    def search(self, line, *a):
        return bool(_seam(self.name))


class PairRx:
    """Play double: finditer() -> list of [group1, group2] pairs."""
    def __init__(self, name):
        self.name = name

    def finditer(self, line, *a):
        return [_Match({"g": list(p)}, line) for p in _seam(self.name)]


class JsonLineRx:
    """Play double for _JSON_STRING_LINE.match -> named-group spans."""
    def __init__(self, name):
        self.name = name

    def match(self, line, *a):
        d = _seam(self.name)
        if not d:
            return None
        return _JLine(d, line)


class _JLine:
    def __init__(self, d, line):
        self.d = d
        self.line = line

    def start(self, g):
        return self.d[g][0]

    def end(self, g):
        return self.d[g][1]

    def group(self, g):
        s, e = self.d[g]
        return self.line[s:e]


def build_ns():
    stub = types.ModuleType("tools.skills_guard")
    stub.Finding = type("Finding", (), {})
    stub._COMPILED_THREAT_PATTERNS = []
    tools = sys.modules.setdefault("tools", types.ModuleType("tools"))
    tools.skills_guard = stub
    sys.modules["tools.skills_guard"] = stub
    ns = {"__builtins__": builtins, "__name__": "pis2_live_context", "re": re}
    exec(_CODE, ns)
    missing = WANTED - {k for k in ns if callable(ns[k])}
    if missing:
        print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(missing)}", file=sys.stderr)
        sys.exit(1)

    # Replace every module regex constant with its play double.
    ns["_PATTERN_BY_ID"] = {pid: _Rx("pattern_hits") for pid in (
        "destructive_home_rm", "hardcoded_secret", "crypto_mining", "sudo_usage",
        "dump_all_env", "format_filesystem", "proc_access", "path_traversal_deep",
        "base64_decode_pipe", "prompt_injection_ignore", "disregard_rules",
        "forget_rules", "env_exfil_curl", "env_exfil_wget", "python_import_os",
        "unpinned_pip_install", "hex_encoded_string", "json_prose_value",
        "encoded_exfil", "ssh_backdoor", "agent_config_mod", "hermes_config_mod",
    )}
    ns["_PATTERN_TOKEN"] = {pid: _Rx("pattern_token_hits") for pid in (
        "sudo_usage", "dump_all_env", "format_filesystem")}
    ns["_URL_IN_LINE"] = PairRx("url_in_line")
    ns["_SELF_UNINSTALL_RM"] = BoolRx("self_uninstall_match")
    ns["_SELF_UNINSTALL_RM_IN_SPAN"] = BoolRx("self_uninstall_span_search")
    ns["_EXEC_ON_LINE"] = BoolRx("exec_on_line")
    ns["_EXEC_CALL_OPEN"] = EndRx("exec_call_ends")
    ns["_LITERAL_SPANS"] = _Rx("literal_spans")
    ns["_TEST_FILE_NAME"] = BoolRx("test_file_name")
    ns["_BASE64_RUN"] = StrRx("base64_runs")
    ns["_B64_ROUNDTRIP"] = BoolRx("b64_roundtrip_search")
    ns["_TAR_CREATE"] = BoolRx("tar_create_search")
    ns["_SUDO_TOKEN"] = IntRx("sudo_tokens")
    ns["_SUDO_ADMIN_HINT"] = IntRx("sudo_admin_hints")
    ns["_SUDO_IN_REGEX"] = BoolRx("sudo_in_regex")
    ns["_SUDO_NEGATION"] = BoolRx("sudo_negation")
    ns["_ENV_DUMP_TOKEN"] = IntRx("env_dump_tokens")
    ns["_QUOTED_SPAN"] = _Rx("quoted_spans")
    ns["_TRAVERSAL_RUN"] = EndRx("traversal_runs")
    ns["_SENSITIVE_AFTER_TRAVERSAL"] = BoolRx("sensitive_match")
    ns["_IMPORT_OS"] = StrRx("import_os_members")
    ns["_DANGEROUS_OS_ON_LINE"] = BoolRx("dangerous_os")
    ns["_AUTHORIZED_KEYS_WRITE"] = BoolRx("authorized_keys_write")
    ns["_LOOPBACK_IP_PORT"] = BoolRx("loopback_match")
    ns["_PIP_INSTALL_TOKEN"] = IntRx("pip_install_tokens")
    ns["_PIP_COMMAND_POSITION"] = BoolRx("pip_command_position")
    ns["_CHAR_CLASS"] = _Rx("char_classes")
    ns["_HEX_ESCAPE"] = _Rx("hex_escapes")
    ns["_URL_HOST"] = StrRx("url_hosts")
    ns["_JSON_STRING_LINE"] = JsonLineRx("json_string_line")
    ns["_JSON_COMMAND_KEY"] = BoolRx("json_command_key")
    ns["_COMMAND_POSITION"] = BoolRx("command_position")
    ns["_MINING_MACHINERY"] = BoolRx("mining_machinery")
    ns["_SECRET_LITERAL"] = StrRx("secret_literals")
    ns["_PROC_PATH"] = StrRx("proc_leaves")
    ns["_PROC_WRITE"] = BoolRx("proc_write")
    return ns


# ── canonical projection ─────────────────────────────────────────────────────
def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, (bytes, bytearray)):
        return list(v)
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


class Obj:
    def __init__(self, d):
        self.__dict__["_raw"] = d if isinstance(d, dict) else {}

    def __getattr__(self, k):
        d = self.__dict__.get("_raw", {})
        if k in d:
            return d[k]
        raise AttributeError(k)


def finding(inp):
    if "finding" not in inp:
        return None
    return Obj(inp["finding"])


def drive(rec, ns):
    func = rec["func"]
    inp = rec["in"]
    f = ns[func]
    g = inp.get
    if func == "is_doc_prose":
        return f(g("rel_path", ""))
    if func == "is_ci_workflow":
        return f(g("rel_path", ""))
    if func == "is_agent_facing":
        return f(finding(inp))
    if func == "prose_cap":
        return f(finding(inp))
    if func == "_is_trusted_installer_url":
        return f(g("host", ""), g("path", ""))
    if func == "is_trusted_installer_pipe":
        return f(finding(inp), g("line", ""))
    if func == "is_locale_catalog":
        return f(g("rel_path", ""))
    if func == "catalog_cap":
        return f(finding(inp))
    if func == "_uninstall_operand":
        return f(g("group", "q0"))
    if func == "is_self_uninstall_doc":
        return f(finding(inp), g("line", ""))
    if func == "_exec_first_arg_literal_spans":
        return f(g("line", ""), [tuple(x) for x in g("spans", [])])
    if func == "_executes":
        return f(g("line", ""), g("rel_path", ""))
    if func == "is_inert_fixture_line":
        return f(finding(inp), g("line", ""), g("is_code", True), g("rel_path", ""))
    if func == "is_test_tree":
        return f(g("rel_path", ""))
    if func == "_decoded_head":
        return f(g("blob", ""))
    if func == "is_base64_media":
        return f(g("line", ""))
    if func == "is_base64_archive_roundtrip":
        return f(finding(inp), g("line", ""))
    if func == "_is_alternation_member":
        return f(g("line", ""), g("start", 0), g("end", 0))
    if func == "_is_whole_literal":
        return f(g("line", ""), g("start", 0), g("end", 0), tuple(g("span", [0, 0])))
    if func == "is_regex_alternation_token":
        return f(finding(inp), g("line", ""))
    if func == "_sudo_in_safety_clause":
        return f(g("line", ""), g("start", 0))
    if func == "is_sudo_inert":
        return f(finding(inp), g("line", ""), g("rel_path", ""))
    if func == "is_env_dump_in_safety_clause":
        return f(finding(inp), g("line", ""))
    if func == "is_quoted_injection_phrase":
        return f(finding(inp), g("line", ""))
    if func == "is_repo_root_traversal":
        return f(finding(inp), g("line", ""))
    if func == "is_benign_lazy_os_import":
        return f(finding(inp), g("line", ""))
    if func == "is_disclosed_config_note":
        return f(finding(inp), g("rel_path", ""))
    if func == "is_authorized_keys_prose":
        return f(finding(inp), g("line", ""), g("is_doc", False))
    if func == "is_loopback_only":
        return f(finding(inp), g("line", ""))
    if func == "is_pip_install_in_prose_literal":
        return f(finding(inp), g("line", ""))
    if func == "is_hex_in_char_class":
        return f(finding(inp), g("line", ""))
    if func == "logical_line":
        return f(list(g("lines", [])), g("index", 0))
    if func == "is_loopback_continuation":
        return f(finding(inp), g("line", ""), g("joined", ""))
    if func == "is_json_prose_value":
        return f(finding(inp), g("rel_path", ""), g("line", ""))
    if func == "_is_standalone_coin_name":
        return f(g("line", ""), g("start", 0), g("end", 0))
    if func == "is_coin_name_only":
        return f(finding(inp), g("line", ""))
    if func == "is_public_client_key":
        return f(finding(inp), g("line", ""))
    if func == "is_own_proc_read":
        return f(finding(inp), g("line", ""))
    raise KeyError("unknown func " + str(func))


def compute(rec, ns):
    CUR["io"] = rec.get("in", {}).get("io", {})
    CUR["pos"] = {}
    try:
        return {"ret": norm(drive(rec, ns)), "trace": [], "state": {}}
    except BaseException as e:
        return {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}


def main():
    n = mism = shown = 0
    ns = build_ns()
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        got = rec.get("out")
        out = compute(rec, ns)
        if got != out:
            mism += 1
            if shown < 15:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print("  C=" + json.dumps(got, ensure_ascii=False)[:900])
                print("  P=" + json.dumps(out, ensure_ascii=False)[:900])
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


def selftest(fixtures):
    ns = build_ns()
    bad = 0
    for i, rec in enumerate(fixtures):
        try:
            spec = compute(rec, ns)["ret"]
            if isinstance(spec, dict) and spec.get("__raise__") in ("AttributeError", "NameError", "KeyError", "RuntimeError"):
                bad += 1
                print(f"SELFTEST case {i} {rec['func']}: suspicious {spec}")
        except BaseException as e:
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = _HERE.parent / "fixtures" / "plugins_install_scan_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()
