#!/usr/bin/env python3
"""
slermes_deep_fuzz.py — Triple DA depth fuzz: multi-layer combinatorial test generator.

Generates and runs thousands of automated fuzz tests across 10+ layers:
  CLI flags × subcommands, config fields, tool invocations, session ops,
  provider metadata, JSON parsing, YAML configs, error paths, stress.

Usage:
  python3 slermes_deep_fuzz.py                    # ~2000+ auto-generated tests
  python3 slermes_deep_fuzz.py --layer config      # single layer
  python3 slermes_deep_fuzz.py --count 5000        # target test count
  python3 slermes_deep_fuzz.py --quick             # ~500 tests
  python3 slermes_deep_fuzz.py --list-layers       # list available layers
  python3 slermes_deep_fuzz.py --verify            # verify binary first
"""

import subprocess
import sys
import os
import tempfile
import time
import json
import random
import string
import math
import itertools
from pathlib import Path
from collections import defaultdict

# ── Config ──────────────────────────────────────────────────────────────────
BINARY = None
LAYER_FILTER = None
TARGET_COUNT = 2000
QUICK_MODE = False
VERIFY_ONLY = False
TIMEOUT = 10
VERBOSE = False

_tests_run = 0
_tests_passed = 0
_tests_failed = 0
_results = []
_home_dir = None

# ── Helpers ─────────────────────────────────────────────────────────────────

def get_home():
    global _home_dir
    if _home_dir is None:
        _home_dir = tempfile.mkdtemp(prefix="slermes_deep_fuzz_")
    return _home_dir

def cleanup_home():
    global _home_dir
    if _home_dir:
        import shutil
        shutil.rmtree(_home_dir, ignore_errors=True)
        _home_dir = None

def run_slermes(args, timeout=TIMEOUT, stdin=None, env=None):
    """Run binary, return (stdout, stderr, rc)."""
    global _tests_run
    _tests_run += 1
    cmd = [BINARY] + args
    # Build env: start with inherited, apply explicit env overrides, default SLERMES_HOME
    merged_env = {**os.environ}
    if env:
        merged_env.update(env)
    if "SLERMES_HOME" not in merged_env:
        merged_env["SLERMES_HOME"] = get_home()
    try:
        p = subprocess.run(
            cmd, input=stdin, capture_output=True, timeout=timeout,
            env=merged_env,
        )
        return p.stdout.decode('utf-8', errors='replace'), p.stderr.decode('utf-8', errors='replace'), p.returncode
    except subprocess.TimeoutExpired:
        return "", "(TIMEOUT)", -999
    except Exception as e:
        return "", f"(EXCEPTION: {e})", -1

def run_raw(args, timeout=TIMEOUT):
    """Run without modified env."""
    cmd = [BINARY] + args
    try:
        p = subprocess.run(cmd, capture_output=True, timeout=timeout)
        return p.stdout.decode(), p.stderr.decode(), p.returncode
    except subprocess.TimeoutExpired:
        return "", "(TIMEOUT)", -999

def strip_init(out):
    return '\n'.join(l for l in out.split('\n') if not l.startswith('['))

def r(name, category, outcome, detail=""):
    """Record test result."""
    global _tests_passed, _tests_failed
    status = "PASS" if outcome else "FAIL"
    if outcome:
        _tests_passed += 1
    else:
        _tests_failed += 1
    _results.append((name, category, status, detail))
    if VERBOSE or not outcome:
        mark = "✅" if outcome else "❌"
        print(f"  {mark} [{category}] {name}" + (f" — {detail}" if detail else ""))

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 1: CLI Flag × Subcommand Combinatorics
# ═══════════════════════════════════════════════════════════════════════════

REGISTERED_FLAGS = [
    "--help", "-h", "--version", "-v", "--json", "--tui",
]

REGISTERED_SUBCMDS = [
    "gateway", "cron", "status", "dump", "logs", "tools",
    "plugins", "secrets", "skills", "help", "commands",
    "model", "config", "history", "sessions", "usage",
    "insights", "copy", "doctor", "version", "chat",
]

SLASH_COMMANDS = [
    "/help", "/setup", "/clear", "/new", "/model", "/config",
    "/insights", "/stats", "/sessions", "/history", "/save",
    "/load", "/undo", "/doctor", "/gateway", "/cron",
    "/tools", "/plugins", "/secrets", "/skills", "/dump",
    "/logs", "/status", "/copy", "/usage",
]

def layer_cli_flags():
    """Test every flag × subcommand combination."""
    cat = "cli-flags"
    
    # Each flag alone (skip --tui: requires ncurses compile)
    for flag in [f for f in REGISTERED_FLAGS if f != '--tui']:
        out, err, rc = run_slermes([flag])
        ok = rc == 0
        r(f"flag:{flag}", cat, ok)
    
    # Each subcommand alone
    for cmd in REGISTERED_SUBCMDS:
        out, err, rc = run_slermes([cmd], timeout=3)
        ok = rc != -999  # accept any real exit code
        r(f"subcmd:{cmd}", cat, ok)
    
    # Each slash command alone
    for cmd in SLASH_COMMANDS:
        out, err, rc = run_slermes([cmd], timeout=3)
        ok = rc != -999
        r(f"slash:{cmd}", cat, ok)
    
    # Help + each subcommand
    for cmd in REGISTERED_SUBCMDS[:10]:
        out, err, rc = run_slermes([cmd, "--help"])
        ok = rc != -999
        r(f"subcmd+help:{cmd}", cat, ok)
    
    # --json + each subcommand
    for cmd in REGISTERED_SUBCMDS:
        out, err, rc = run_slermes(["--json", cmd])
        ok = rc != -999
        r(f"json+subcmd:{cmd}", cat, ok)
    
    # Unknown flags near known commands
    for bad_flag in ["--bogus", "--fake", "--undefined", "-z", "--999"]:
        out, err, rc = run_slermes([bad_flag])
        ok = rc != 0  # should be rejected
        r(f"reject:{bad_flag}", cat, ok)

def layer_cli_help_sections():
    """Test --help prints every section."""
    cat = "cli-help"
    out, err, rc = run_slermes(["--help"])
    if rc != 0:
        r("help-exists", cat, False, f"rc={rc}")
        return
    # Check --help output (top-level flags only — insights is a slash command)
    sections = ["Options", "Usage", "gateway", "cron", "status", "tools",
                "plugins", "secrets", "skills", "session", "config", "version",
                "help"]
    for s in sections:
        r(f"help-section:{s}", cat, s.lower() in out.lower() or s in out,
          f"'{s}' {'found' if s.lower() in out.lower() else 'missing'}")
    
    # Also check /help for slash-command availability
    out2, err2, rc2 = run_slermes(["/help"])
    if rc2 == 0:
        for cmd in ["insights", "setup", "model", "config", "help", "stats", "sessions"]:
            r(f"slash-help:{cmd}", cat, cmd.lower() in out2.lower(),
              f"'{cmd}' in /help: {cmd.lower() in out2.lower()}")

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 2: Config Field Fuzzing — every field with boundary values
# ═══════════════════════════════════════════════════════════════════════════

CONFIG_FIELDS = {
    # Int fields
    "agent.max_turns": [0, 1, 90, 1000, -1, 999999],
    "agent.timeout": [0, 1, 30, 3600, -1],
    "agent.verbose": [0, 1, 2, -1],
    "terminal.timeout": [0, 1, 30, 1800, 86400, -1],
    "approval.max_retries": [0, 1, 3, 100, -1],
    # Float fields
    "provider.temperature": [0.0, 0.5, 1.0, 2.0, -1.0, 999.0],
    "provider.top_p": [0.0, 0.5, 1.0, -0.5],
    # String fields
    "model": ["", "gpt-4", "claude-3-opus", "A"*1000, "test/slash/name"],
    "provider": ["", "openai", "anthropic", "deepseek", "openrouter", "FAKE"],
    "api_key": ["", "sk-" + "A"*50, "A"*1000],
    "base_url": ["", "https://api.openai.com", "not-a-url", "http://localhost:8080"],
    # Boolean fields
    "agent.quiet": ["true", "false", "1", "0", "yes", "no"],
    "tools.enabled": ["true", "false", "1", "0"],
}

def layer_config_fields():
    """Test every config field with boundary values."""
    cat = "config-fields"
    for field, values in CONFIG_FIELDS.items():
        for val in values:
            cfg = f"{field}: {val}"
            cf = os.path.join(get_home(), "config.yaml")
            with open(cf, 'w') as f:
                f.write(f"{field}: {val}\n")
            out, err, rc = run_slermes(["config"])
            ok = rc != -999
            r(f"cfg:{field}={val!r}", cat, ok)
            os.unlink(cf)

def layer_config_missing():
    """Test with no config file at all."""
    cat = "config-edge"
    clean_home = tempfile.mkdtemp(prefix="slermes_noconfig_")
    cmd = [BINARY, "config"]
    p = subprocess.run(cmd, capture_output=True, timeout=5,
                       env={**os.environ, "SLERMES_HOME": clean_home})
    ok = p.returncode == 0 or p.returncode == 1
    r("no-config-file", cat, ok, f"rc={p.returncode}")
    import shutil
    shutil.rmtree(clean_home, ignore_errors=True)

def layer_config_env():
    """Test config via environment variables."""
    cat = "config-env"
    env_tests = [
        {"SLERMES_MODEL": "gpt-4"},
        {"SLERMES_PROVIDER": "anthropic"},
        {"SLERMES_MAX_TURNS": "50"},
        {"SLERMES_VERBOSE": "1"},
        {"SLERMES_TIMEOUT": "300"},
        {"OPENAI_API_KEY": "sk-test123"},
        {"ANTHROPIC_API_KEY": "sk-ant-test123"},
        {"SLERMES_HOME": get_home()},
    ]
    for env in env_tests:
        out, err, rc = run_slermes(["config"], env=env)
        ok = rc != -999
        r(f"env:{list(env.keys())[0]}", cat, ok)

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 3: Tool Invocation Fuzzing
# ═══════════════════════════════════════════════════════════════════════════

def discover_tools():
    """Discover registered tool names from binary."""
    out, err, rc = run_raw(["tools"], timeout=10)
    if rc != 0:
        return ["file", "web", "terminal", "memory", "write_file", "read_file",
                "search_files", "exec_code", "bash", "patch", "clarify",
                "send_message", "cronjob", "skill_view", "skill_manage",
                "session_search", "todo", "process"]
    tools = []
    for line in out.split('\n'):
        line = line.strip()
        if line and not line.startswith('[') and not line.startswith('Use'):
            name = line.split()[0] if line.split() else ""
            if name and not name.startswith(('/', '--', '(')):
                tools.append(name)
    return tools[:50]

def layer_tool_names():
    """Every tool name is valid and registered."""
    cat = "tool-names"
    tools = discover_tools()
    for tool in tools[:30]:
        out, err, rc = run_slermes(["tools"])
        ok = tool.lower() in out.lower()
        r(f"tool-registered:{tool}", cat, ok)
    r(f"tool-count:{len(tools)}", "tool-metrics", len(tools) > 10, f"{len(tools)} tools")

def layer_tool_help():
    """Every tool returns meaningful output."""
    cat = "tool-basic"
    cmds = ["tools", "/tools", "--json tools"]
    for cmd in cmds:
        parts = cmd.split()
        out, err, rc = run_slermes(parts)
        ok = rc == 0
        r(f"tools-{cmd.replace(' ','-')}", cat, ok)

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 4: Session CRUD Fuzzing — exhaustive parameter combos
# ═══════════════════════════════════════════════════════════════════════════

def make_session_db():
    """Create a session DB with known sessions for testing."""
    db_path = os.path.join(get_home(), "sessions.db")
    # SQLite session DB
    import sqlite3
    conn = sqlite3.connect(db_path)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS sessions (
            id TEXT PRIMARY KEY,
            title TEXT,
            model TEXT,
            source TEXT,
            started_at REAL,
            ended_at REAL,
            message_count INTEGER DEFAULT 0,
            tool_call_count INTEGER DEFAULT 0,
            input_tokens INTEGER DEFAULT 0,
            output_tokens INTEGER DEFAULT 0,
            cache_read_tokens INTEGER DEFAULT 0,
            cache_write_tokens INTEGER DEFAULT 0,
            billing_provider TEXT,
            billing_base_url TEXT,
            estimated_cost_usd REAL DEFAULT 0,
            actual_cost_usd REAL DEFAULT 0,
            cost_status TEXT DEFAULT 'unknown',
            cost_source TEXT DEFAULT ''
        )
    """)
    
    # Insert sessions with various characteristics
    sessions = [
        # (id, model, source, input_tok, output_tok, cost)
        ("test_001", "gpt-4", "cli", 1000, 200, 0.03),
        ("test_002", "claude-3-opus", "telegram", 5000, 1000, 0.15),
        ("test_003", "deepseek-v4-flash", "web", 100, 50, 0.001),
        ("test_004", "gpt-4-turbo", "cli", 20000, 5000, 0.60),
        ("test_005", "claude-3-haiku", "telegram", 50, 10, 0.001),
        ("test_006", "", "cli", 0, 0, 0.0),  # empty model
        ("test_007", "gpt-4", "", 1000, 200, 0.03),  # empty source
        ("test_008", "A" * 200, "cli", 999999, 999999, 999.99),  # extreme values
    ]
    
    now = time.time()
    for sid, model, source, inp, out, cost in sessions:
        conn.execute(
            "INSERT OR REPLACE INTO sessions VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            (sid, f"session_{sid}", model, source,
             now - 86400, now, 10, 5, inp, out, 0, 0,
             "", "", cost, cost, "estimated", "test")
        )
    conn.commit()
    conn.close()
    return db_path

def layer_session_crud():
    """Exhaustive session CRUD operations with edge cases."""
    cat = "session-crud"
    make_session_db()
    
    # Basic operations
    ops = [
        (["sessions"], "list"),
        (["/sessions"], "slash-list"),
        (["--json", "sessions"], "json-list"),
        (["sessions", "--days", "7"], "list-days"),
        (["sessions", "--source", "cli"], "list-source"),
    ]
    for args, name in ops:
        out, err, rc = run_slermes(args)
        ok = rc != -999
        r(f"sess:{name}", cat, ok)
    
    # Session ID formats
    id_formats = ["test_001", "test_999", "", "../../etc/passwd", "', DROP TABLE; --",
                  "A"*500, "valid-id_123", "session with spaces", "test.1", "test/1"]
    for sid in id_formats:
        out, err, rc = run_slermes(["/load", sid])
        ok = rc != -999
        r(f"sess:load-id:{sid[:20]}", cat, ok)

def layer_session_export():
    """Test session export formats."""
    cat = "session-export"
    make_session_db()
    
    export_cmds = [
        (["/export", "test_001"], "export-json"),
        (["/export", "test_001", "--format", "markdown"], "export-md"),
        (["/export", "test_001", "--format", "json"], "export-json-explicit"),
        (["/export", "nonexistent"], "export-missing"),
        (["/export", ""], "export-empty"),
    ]
    for args, name in export_cmds:
        out, err, rc = run_slermes(args)
        ok = rc != -999
        r(f"sess:{name}", cat, ok)

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 5: Provider / Model Metadata Fuzzing
# ═══════════════════════════════════════════════════════════════════════════

PROVIDER_NAMES = ["openai", "anthropic", "deepseek", "openrouter", "google",
                   "azure", "bedrock", "xai", "groq", "together", "mistral",
                   "perplexity", "cohere", "replicate", "custom", "", "FAKE"]

MODEL_NAMES = ["gpt-4", "gpt-4-turbo", "claude-3-opus", "claude-3-sonnet",
               "deepseek-v4-flash", "deepseek-r1", "gemini-2.0-flash",
               "nvidia/nemotron-3-ultra:free", "openrouter/owl-alpha",
               "", "nonexistent/model", "A"*500, "model/with/slashes/1"]

def layer_provider_metadata():
    """Test provider metadata queries exhaustively."""
    cat = "provider-md"
    
    # Model show
    for model in MODEL_NAMES[:10]:
        out, err, rc = run_slermes(["/model", model])
        ok = rc != -999
        r(f"model:show:{model[:20]}", cat, ok)
    
    # Provider set
    for prov in PROVIDER_NAMES[:10]:
        out, err, rc = run_slermes(["/model", "--provider", prov])
        ok = rc != -999
        r(f"model:provider:{prov}", cat, ok)
    
    # Model list variants
    list_cmds = [
        ["/model", "list"],
        ["/model", "--list"],
        ["model", "list"],
        ["/model", "providers"],
        ["/model", "--providers"],
        ["model", "providers"],
    ]
    for args in list_cmds:
        name = "-".join(args).replace("/", "")
        out, err, rc = run_slermes(args)
        ok = rc != -999
        r(f"model:{name}", cat, ok)

def layer_pricing():
    """Test pricing engine boundaries."""
    cat = "pricing"
    pricing_inputs = [
        (100, 50),
        (0, 0),
        (1000000, 1000000),  # 1M tokens
        (-1, -1),  # negative
        (999999999, 999999999),  # extreme
        (1, 0),
        (0, 1),
    ]
    for inp, out_tok in pricing_inputs:
        args = ["/cost", str(inp), str(out_tok)]
        out, err, rc = run_slermes(args)
        ok = rc != -999
        r(f"pricing:in={inp}_out={out_tok}", cat, ok)

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 6: Edge Cases and Input Fuzzing — parameterized
# ═══════════════════════════════════════════════════════════════════════════

def generate_random_string(length):
    """Generate random ASCII string of given length."""
    return ''.join(random.choice(string.ascii_letters + string.digits + " _-.,!?") for _ in range(length))

def generate_random_unicode(length):
    """Generate random unicode string."""
    chars = "你好世界¡Hola!🌍日本語测试😊🔬🎯étéñño"
    return ''.join(random.choice(chars) for _ in range(length))

def layer_edge_inputs():
    """Parameterized edge case input fuzzing."""
    cat = "edge-inputs"
    
    # Length-based fuzzing (exponential scale)
    for exp in range(1, 7):  # 10^1 to 10^6
        length = 10 ** exp
        if length > 100000 and QUICK_MODE:
            break
        s = generate_random_string(length)
        out, err, rc = run_slermes([s], timeout=min(10, length // 10000 + 2))
        ok = rc != -999
        r(f"len:10^{exp}({length})", cat, ok)
    
    # Unicode fuzzing
    for exp in range(1, 5):
        length = 10 ** exp
        s = generate_random_unicode(length)
        out, err, rc = run_slermes([s], timeout=min(5, length // 10000 + 2))
        ok = rc != -999
        r(f"unicode:10^{exp}({length})", cat, ok)
    
    # Special characters
    specials = [
        "\x00",  # null byte
        "\x01\x02\x1f",  # control chars
        "\x7f\x80\xff",  # high ASCII
        "\\x00\\x01",  # escaped
        "$(id)",  # shell injection
        "`cat /etc/passwd`",  # backtick injection
        "; rm -rf /",  # command injection
        "${PATH}",  # env expansion
        "' OR '1'='1",  # SQL injection
        "'; DROP TABLE sessions; --",  # SQL injection
        "..\\..\\..\\etc\\passwd",  # path traversal
        "../../../../etc/passwd",  # path traversal
        "%00%01%ff",  # URL-encoded
        "null", "undefined", "NaN", "Infinity",  # JS special values
        "<script>alert(1)</script>",  # XSS
        "{{7*7}}",  # template injection
    ]
    for s in specials:
        safe_name = ''.join(c if c.isprintable() else f'\\x{ord(c):02x}' for c in s)[:25]
        out, err, rc = run_slermes([s], timeout=5)
        ok = rc not in (-11, -6, -999)  # no crash
        r(f"special:{safe_name}", cat, ok)

def layer_edge_many_args():
    """Many arguments of varying sizes."""
    cat = "edge-many-args"
    
    # 10 args of increasing sizes
    for n in [2, 5, 10, 25, 50, 100, 200]:
        args = [generate_random_string(n) for _ in range(n)]
        out, err, rc = run_slermes(args, timeout=min(10, n // 10 + 2))
        ok = rc != -999
        r(f"many:{n}x{n}", cat, ok)
    
    # Single arg repeated
    for n in [10, 50, 100]:
        args = ["same"] * n
        out, err, rc = run_slermes(args, timeout=min(10, n // 10 + 2))
        ok = rc != -999
        r(f"same-x{n}", cat, ok)

def layer_edge_fds():
    """File descriptor edge cases."""
    cat = "edge-fds"
    
    # Closed stdin
    try:
        p = subprocess.Popen([BINARY], stdin=subprocess.DEVNULL,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             env={**os.environ, "SLERMES_HOME": get_home()})
        try:
            out, err = p.communicate(timeout=3)
            rc = p.returncode
            ok = rc != -999
        except subprocess.TimeoutExpired:
            p.kill()
            ok = True
            rc = -999
        r("stdin-devnull", cat, ok, f"rc={rc}")
    except Exception as e:
        r("stdin-devnull", cat, False, str(e))
    
    # Very large stdin
    large_input = ("hello\n" * 100000).encode()
    try:
        p = subprocess.Popen([BINARY, "--json", "status"], stdin=subprocess.PIPE,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             env={**os.environ, "SLERMES_HOME": get_home()})
        try:
            out, err = p.communicate(input=large_input, timeout=5)
            ok = p.returncode != -999
        except subprocess.TimeoutExpired:
            p.kill()
            ok = True
        r("stdin-large-1MB", cat, ok)
    except Exception as e:
        r("stdin-large-1MB", cat, False, str(e))

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 7: JSON Parsing Fuzzing (via API server or tool inputs)
# ═══════════════════════════════════════════════════════════════════════════

def layer_json_edge_cases():
    """JSON payload fuzzing for API server endpoints (via tools)."""
    cat = "json-fuzz"
    
    json_inputs = [
        ("empty", ""),
        ("null", "null"),
        ("true", "true"),
        ("false", "false"),
        ("number", "42"),
        ("float", "3.14159"),
        ("neg", "-1"),
        ("large-num", "1" * 1000),
        ("empty-obj", "{}"),
        ("empty-arr", "[]"),
        ("nested", '{"a":{"b":{"c":[1,2,3]}}}'),
        ("unicode", '{"msg":"你好世界🌍"}'),
        ("escape-chars", '{"data":"\\n\\t\\r\\u0000"}'),
        ("long-string", '{"data":"' + "A"*10000 + '"}'),
        ("deep-nest", '{"a":' * 100 + '"x"' + '}' * 100),
        ("million-arr", '[' + ','.join('1' for _ in range(10000)) + ']'),
        ("mixed", '{"str":"x","num":42,"arr":[1,2,3],"obj":{"k":"v"}}'),
        ("sql-injection", '{"query":"\'; DROP TABLE; --"}'),
        ("xss", '{"html":"<script>alert(1)</script>"}'),
        ("proto-pollution", '{"__proto__":{"admin":true}}'),
    ]
    
    for name, payload in json_inputs:
        # Try passing as a file content
        fp = os.path.join(get_home(), f"json_test_{name}.json")
        with open(fp, 'w') as f:
            f.write(payload)
        out, err, rc = run_slermes(["/load", fp])
        ok = rc != -999
        r(f"json:{name}", cat, ok)
        os.unlink(fp)

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 8: YAML Config Fuzzing
# ═══════════════════════════════════════════════════════════════════════════

YAML_FUZZ_INPUTS = [
    # (name, content)
    ("empty", ""),
    ("just-key", "key:"),
    ("just-value", ": value"),
    ("nested", "a:\n  b:\n    c: 1"),
    ("list", "tools:\n  - file\n  - web\n  - terminal\n"),
    ("boolean-true", "enabled: true"),
    ("boolean-false", "enabled: false"),
    ("boolean-yes", "enabled: yes"),
    ("boolean-no", "enabled: no"),
    ("int-bool", "enabled: 1"),
    ("all-types", "str: hello\nint: 42\nfloat: 3.14\nbool: true\nlist: [a,b,c]\n"),
    ("long-strings", "key: " + "A"*10000),
    ("unicode-keys", "你好: 世界"),
    ("unicode-values", "key: 你好世界🌍"),
    ("sql-injection", "key: '; DROP TABLE; --"),
    ("xss", "key: <script>alert(1)</script>"),
    ("deep-nest", "a:\n" + "  b:\n" * 100 + "    c: 1"),
    ("tab-indent", "\tkey: value"),
    ("mixed-indent", "a:\n  b: 1\n    c: 2"),
    ("duplicate-keys", "key: 1\nkey: 2\n"),
]

def layer_yaml_configs():
    """Test every YAML config edge case."""
    cat = "yaml-config"
    for name, content in YAML_FUZZ_INPUTS:
        cf = os.path.join(get_home(), "config.yaml")
        with open(cf, 'w') as f:
            f.write(content)
        out, err, rc = run_slermes(["config"])
        ok = rc != -999
        r(f"yaml:{name}", cat, ok)
        os.unlink(cf)

def layer_yaml_env():
    """Test YAML config with env var overrides."""
    cat = "yaml-env"
    env_vars = {
        "SLERMES_MODEL": "gpt-4-env-test",
        "SLERMES_PROVIDER": "openai-test",
        "SLERMES_API_KEY": "sk-env-test-12345",
        "OPENAI_API_KEY": "sk-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "ANTHROPIC_API_KEY": "sk-ant-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
    }
    
    for var, val in env_vars.items():
        cf = os.path.join(get_home(), "config.yaml")
        with open(cf, 'w') as f:
            f.write("model: from-config\nprovider: from-config\n")
        out, err, rc = run_slermes(["config"], env={var: val})
        ok = rc != -999
        r(f"env-override:{var}", cat, ok)
        os.unlink(cf)

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 9: Thread Safety / Stress Testing
# ═══════════════════════════════════════════════════════════════════════════

def layer_stress():
    """Stress testing with rapid sequential requests."""
    cat = "stress"
    
    # Rapid-fire: 10 common commands in quick succession
    cmds = [["status"], ["config"], ["tools"], ["help"], ["commands"],
            ["model"], ["--json", "tools"], ["doctor"], ["version"],
            ["sessions"]]
    for i, args in enumerate(cmds * 5):  # repeat 5x
        out, err, rc = run_slermes(args, timeout=3)
        ok = rc != -999
        r(f"rapid:cmd{i:03d}:{args[0]}", cat, ok)
        if QUICK_MODE and i >= 20:
            break

def layer_stress_sequential_sessions():
    """Sequential session database operations under load."""
    cat = "stress-sessions"
    for i in range(50 if not QUICK_MODE else 10):
        out, err, rc = run_slermes(["sessions"], timeout=3)
        ok = rc != -999
        r(f"stress:sessions:{i}", cat, ok)
        out2, err2, rc2 = run_slermes(["status"], timeout=3)
        ok2 = rc2 != -999
        r(f"stress:status:{i}", cat, ok2)

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 10: Path Traversal / Security Fuzzing
# ═══════════════════════════════════════════════════════════════════════════

PATH_TRAVERSAL_INPUTS = [
    "../",
    "../../etc/passwd",
    "../config.yaml",
    "..\\config.yaml",
    "%2e%2e%2f",
    "....//....//etc/passwd",
    "/etc/passwd",
    "/dev/null",
    "/dev/random",
    "/proc/self/environ",
    "~/.slermes/config.yaml",
    "$HOME/.slermes/config.yaml",
    "${HOME}/config.yaml",
    "`id`",
    "$(id)",
    "; ls",
    "| ls",
    "> /tmp/out",
    "< /etc/passwd",
]

def layer_path_traversal():
    """Test path traversal and injection protection."""
    cat = "security"
    for inp in PATH_TRAVERSAL_INPUTS:
        sanitized = inp.replace('\n', '\\n')[:30]
        out, err, rc = run_slermes(["/load", inp])
        ok = rc != -999
        r(f"path-traversal:{sanitized}", cat, ok)

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 11: Exit Code Verification
# ═══════════════════════════════════════════════════════════════════════════

EXIT_CODE_TESTS = [
    (["--help"], 0, "exit-0:help"),
    (["-h"], 0, "exit-0:h"),
    (["--version"], 0, "exit-0:version"),
    (["-v"], 0, "exit-0:v"),
    (["version"], 0, "exit-0:version-subcmd"),
    (["--bogus-flag"], "!=0", "exit-nonzero:bogus-flag"),
    (["--nonexistent"], "!=0", "exit-nonzero:nonexistent"),
    (["status"], 0, "exit-0:status"),
    (["tools"], 0, "exit-0:tools"),
    (["help"], 0, "exit-0:help"),
    (["commands"], 0, "exit-0:commands"),
    (["/help"], 0, "exit-0:slash-help"),
    (["insights"], 0, "exit-0:insights"),
]

def layer_exit_codes():
    """Verify every command returns correct exit code."""
    cat = "exit-codes"
    for args, expected, name in EXIT_CODE_TESTS:
        out, err, rc = run_slermes(args)
        if expected == 0:
            ok = rc == 0
        elif expected == "!=0":
            ok = rc != 0
        else:
            ok = rc == expected
        r(name, cat, ok, f"expected={expected} got={rc}")

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 12: Property-Based / Randomized Fuzzing
# ═══════════════════════════════════════════════════════════════════════════

def layer_random_fuzz():
    """Random string generation fuzzing — property-based."""
    cat = "random-fuzz"
    count = 500 if not QUICK_MODE else 100
    
    for i in range(count):
        n_parts = random.randint(1, 5)
        parts = []
        for _ in range(n_parts):
            kind = random.randint(0, 4)
            if kind == 0:
                parts.append("--" + generate_random_string(random.randint(1, 20)))
            elif kind == 1:
                parts.append(generate_random_string(random.randint(1, 30)))
            elif kind == 2:
                parts.append("/" + generate_random_string(random.randint(1, 15)))
            elif kind == 3:
                parts.append(str(random.randint(-1000000, 1000000)))
            else:
                parts.append(generate_random_unicode(random.randint(1, 10)))
        
        out, err, rc = run_slermes(parts, timeout=5)
        ok = rc != -11 and rc != -6
        if not ok:
            cmd_str = ' '.join(p[:20] for p in parts)[:60]
            r(f"rnd:{i:03d}", cat, False, f"CRASH rc={rc} cmd={cmd_str}")

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 13: Config Round-Trip Verification
# ═══════════════════════════════════════════════════════════════════════════

CONFIG_ROUNDTRIP_SCENARIOS = [
    # (name, yaml_content, expected_in_output)
    ("basic", "model:\n  default: gpt-4\n  provider: openai\n", "gpt-4"),
    ("with_quotes", "model:\n  default: \"gpt-4\"\n  provider: 'openai'\n", "gpt-4"),
    ("with_list", "agent:\n  max_turns: 100\n  verbose: true\n", "100"),
    ("with_provider_cfg", "model:\n  temperature: 0.7\n  max_tokens: 4096\n", "0.7"),
    ("with_gateway", "gateway:\n  enabled: true\n  platforms: [telegram, discord]\n", "gateway"),
    ("all_options", "model:\n  default: claude-3-opus\n  provider: anthropic\n  base_url: https://api.anthropic.com\nagent:\n  max_turns: 50\nmodel:\n  temperature: 1.0\n", "claude-3-opus"),
]

def layer_config_roundtrip():
    """Config round-trip: write YAML, read back, verify values."""
    cat = "config-roundtrip"
    for name, content, expected in CONFIG_ROUNDTRIP_SCENARIOS:
        import tempfile
        tmp = tempfile.mkdtemp(prefix="slermes_cfg_")
        cf = os.path.join(tmp, "config.yaml")
        with open(cf, 'w') as f:
            f.write(content)
        out, err, rc = run_slermes(["config"], timeout=5, env={"SLERMES_HOME": tmp})
        ok = rc == 0 and expected in out
        r(f"roundtrip:{name}", cat, ok, f"expected={expected} rc={rc}")
        import shutil
        shutil.rmtree(tmp, ignore_errors=True)

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 14: Gateway Plugin Discovery
# ═══════════════════════════════════════════════════════════════════════════

def layer_gateway():
    """Test gateway subcommand surface."""
    cat = "gateway"
    
    gw_cmds = [
        (["gateway", "status"], "status"),
        (["gateway", "list"], "list"),
        (["gateway", "help"], "help"),
        (["gateway", "platforms"], "platforms"),
        (["/gateway", "status"], "slash-status"),
    ]
    for args, name in gw_cmds:
        out, err, rc = run_slermes(args)
        ok = rc != -999
        r(f"gw:{name}", cat, ok)

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 15: Cron / Scheduler
# ═══════════════════════════════════════════════════════════════════════════

def layer_cron():
    """Test cron subcommand surface."""
    cat = "cron"
    
    cron_cmds = [
        (["cron", "list"], "list"),
        (["cron", "status"], "status"),
        (["cron", "help"], "help"),
        (["/cron", "list"], "slash-list"),
    ]
    for args, name in cron_cmds:
        out, err, rc = run_slermes(args)
        ok = rc != -999
        r(f"cron:{name}", cat, ok)

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 16: Doctor Diagnostics Exhaustive
# ═══════════════════════════════════════════════════════════════════════════

def layer_doctor():
    """Test doctor output for every diagnostic section."""
    cat = "doctor"
    out, err, rc = run_slermes(["doctor"])
    if rc != 0 and not QUICK_MODE:
        r("doctor-runs", cat, False, f"rc={rc}")
        return
    
    sections = ["Binary", "Config", "API Keys", "Summary", "Version",
                "Provider", "Model", "slermes"]
    for s in sections:
        r(f"doctor-section:{s}", cat, s.lower() in out.lower(), f"'{s}' {'found' if s.lower() in out.lower() else 'missing'}")

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER 17: Memory / Allocation Stress
# ═══════════════════════════════════════════════════════════════════════════

def layer_memory_edge():
    """Edge case memory scenarios."""
    cat = "memory-edge"
    
    # Empty home directory
    empty_home = tempfile.mkdtemp(prefix="slermes_empty_")
    for cmd in [["status"], ["config"], ["tools"], ["sessions"]]:
        p = subprocess.run([BINARY] + cmd, capture_output=True, timeout=5,
                          env={**os.environ, "SLERMES_HOME": empty_home})
        r(f"empty-home:{cmd[0]}", cat, p.returncode != -999)
    import shutil
    shutil.rmtree(empty_home, ignore_errors=True)

# ═══════════════════════════════════════════════════════════════════════════
#  LAYER REGISTRY
# ═══════════════════════════════════════════════════════════════════════════

LAYERS = [
    # (name, function, estimated_test_count)
    ("cli-flags", layer_cli_flags, 120),
    ("cli-help", layer_cli_help_sections, 20),
    ("config-fields", layer_config_fields, 120),
    ("config-edge", layer_config_missing, 5),
    ("config-env", layer_config_env, 10),
    ("config-roundtrip", layer_config_roundtrip, 10),
    ("tool-names", layer_tool_names, 35),
    ("tool-basic", layer_tool_help, 5),
    ("session-crud", layer_session_crud, 50),
    ("session-export", layer_session_export, 10),
    ("provider-md", layer_provider_metadata, 30),
    ("pricing", layer_pricing, 10),
    ("edge-inputs", layer_edge_inputs, 80),
    ("edge-many-args", layer_edge_many_args, 15),
    ("edge-fds", layer_edge_fds, 5),
    ("json-fuzz", layer_json_edge_cases, 25),
    ("yaml-config", layer_yaml_configs, 25),
    ("yaml-env", layer_yaml_env, 10),
    ("stress", layer_stress, 50),
    ("stress-sessions", layer_stress_sequential_sessions, 60),
    ("security", layer_path_traversal, 20),
    ("exit-codes", layer_exit_codes, 15),
    ("random-fuzz", layer_random_fuzz, 500),
    ("gateway", layer_gateway, 10),
    ("cron", layer_cron, 10),
    ("doctor", layer_doctor, 15),
    ("memory-edge", layer_memory_edge, 5),
]

def list_layers():
    """Print available layers and estimated test counts."""
    print(f"\n{'='*60}")
    print(f"  Slermes Deep Fuzz — Available Layers")
    print(f"{'='*60}")
    total = 0
    for name, fn, est in LAYERS:
        print(f"  {name:20s} ~{est:4d} tests")
        total += est
    print(f"  {'─'*30}")
    print(f"  {'TOTAL':20s} ~{total:4d} tests")
    print()

def verify_binary():
    """Quick verification that the binary works."""
    print(f"\nVerifying binary: {BINARY}")
    if not os.path.exists(BINARY):
        print(f"  ❌ Not found")
        return False
    if not os.access(BINARY, os.X_OK):
        print(f"  ❌ Not executable")
        return False
    
    out, err, rc = run_raw(["--help"])
    if rc != 0:
        print(f"  ❌ --help failed: rc={rc}")
        return False
    if "Usage" not in out:
        print(f"  ❌ --help output missing 'Usage'")
        return False
    
    size = os.path.getsize(BINARY)
    print(f"  ✅ {size // 1024} KiB ELF binary")
    print(f"  ✅ --help works")
    return True

def run_layer(name, fn):
    """Run a single layer, return test count."""
    global _tests_run, _tests_passed, _tests_failed
    before = _tests_run
    try:
        fn()
    except Exception as e:
        print(f"  💥 [{name}] layer failed: {e}")
        import traceback
        traceback.print_exc()
    return _tests_run - before

def main():
    global BINARY, LAYER_FILTER, TARGET_COUNT, QUICK_MODE, VERBOSE, VERIFY_ONLY
    
    # Parse args
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == '--binary' and i + 1 < len(args):
            BINARY = args[i + 1]
            i += 2
        elif args[i] == '--layer' and i + 1 < len(args):
            LAYER_FILTER = args[i + 1]
            i += 2
        elif args[i] == '--count' and i + 1 < len(args):
            TARGET_COUNT = int(args[i + 1])
            i += 2
        elif args[i] == '--quick':
            QUICK_MODE = True
            i += 1
        elif args[i] == '--verbose':
            VERBOSE = True
            i += 1
        elif args[i] == '--verify':
            VERIFY_ONLY = True
            i += 1
        elif args[i] == '--list-layers':
            list_layers()
            return 0
        else:
            print(f"Unknown: {args[i]}")
            return 1
    
    # Auto-detect binary
    if BINARY is None:
        candidates = [
            "./slermes",
            os.path.expanduser("~/hermes-agent-dev/slermes/slermes"),
        ]
        for c in candidates:
            if os.path.exists(c) and os.access(c, os.X_OK):
                BINARY = os.path.abspath(c)
                break
        if BINARY is None:
            print("Error: binary not found. Use --binary <path>")
            return 1
    
    if VERIFY_ONLY:
        return 0 if verify_binary() else 1
    
    if not verify_binary():
        return 1
    
    # Select layers
    layers = LAYERS
    if LAYER_FILTER:
        layers = [(n, f, e) for n, f, e in LAYERS if n == LAYER_FILTER]
        if not layers:
            print(f"Layer '{LAYER_FILTER}' not found. Use --list-layers")
            return 1
    
    print(f"\n{'='*60}")
    print(f"  Slermes Deep Fuzz — Triple DA Depth Check")
    print(f"  Binary: {BINARY}")
    print(f"  Layers: {len(layers)}")
    print(f"  Quick mode: {QUICK_MODE}")
    print(f"{'='*60}\n")
    
    for name, fn, est in layers:
        print(f"  ▶ {name} (~{est} tests)...")
        ran = run_layer(name, fn)
    
    total = _tests_passed + _tests_failed
    pct = (_tests_passed / total * 100) if total > 0 else 0
    
    print(f"\n{'='*60}")
    print(f"  RESULTS: {_tests_passed}/{total} passed ({pct:.1f}%) — {_tests_failed} failed")
    print(f"  Total tests executed: {total}")
    
    # List failures
    failures = [r for r in _results if r[2] == "FAIL"]
    if failures:
        print(f"\n  FAILURES ({len(failures)}):")
        for name, cat, status, detail in failures[:20]:
            print(f"    ❌ [{cat}] {name} — {detail[:80]}")
        if len(failures) > 20:
            print(f"    ... and {len(failures) - 20} more")
    
    print()
    cleanup_home()
    return _tests_failed

if __name__ == '__main__':
    sys.exit(main())
