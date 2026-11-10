#!/usr/bin/env python3
"""Slermes-omarchy agent-integration parity gate.

Builds a synthetic state.db with the exact hermes_state_open.c schema, runs the
native C11 collector (src/agent/omarchy_usage.c) against it, and -- when an
omarchy checkout is reachable -- compares every stat field to omarchy's own
`omarchy-agent-usage-hermes` collector on the SAME database.  The two must agree
on every field of the agents-panel contract; this is what makes Slermes appear
to an omarchy agents panel exactly where Hermes appears.

Usage:
    python3 scripts/test_omarchy_usage.py [path/to/omarchy] [path/to/gcc]
Exit 0 = parity holds (or omarchy not given: the C record itself is sane).
"""
import json, os, shutil, sqlite3, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
omarchy = sys.argv[1] if len(sys.argv) > 1 else None
CC = sys.argv[2] if len(sys.argv) > 2 else "gcc"

fail = 0
def check(label, cond):
    global fail
    print(("  PASS " if cond else "  FAIL ") + label)
    if not cond: fail += 1

def build_synthetic(db):
    sqlite3.connect(db).close()
    con = sqlite3.connect(db)
    con.executescript("""
      CREATE TABLE sessions (id TEXT PRIMARY KEY, started_at REAL NOT NULL,
        model TEXT, billing_provider TEXT, billing_base_url TEXT);
      CREATE TABLE session_model_usage (session_id TEXT, model TEXT,
        billing_provider TEXT NOT NULL DEFAULT '',
        billing_base_url TEXT NOT NULL DEFAULT '',
        input_tokens INTEGER DEFAULT 0, output_tokens INTEGER DEFAULT 0,
        cache_read_tokens INTEGER DEFAULT 0, cache_write_tokens INTEGER DEFAULT 0);
      CREATE TABLE messages (id INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id TEXT NOT NULL, role TEXT NOT NULL, timestamp REAL NOT NULL);
    """)
    now, three = time.time(), time.time() - 3*86400
    con.executemany("INSERT INTO sessions (id,started_at,model,billing_provider) VALUES (?,?,?,?)",
        [("s1", now, "model-a", "nous"), ("s2", now, "model-b", "nous"),
         ("s3", three, "model-a", "deepseek")])
    con.executemany("INSERT INTO session_model_usage (session_id,model,billing_provider,"
        "billing_base_url,input_tokens,output_tokens,cache_read_tokens,cache_write_tokens) VALUES (?,?,?,?,?,?,?,?)",
        [("s1","model-a","nous","",100,20,30,40), ("s2","model-b","nous","",0,50,25,0),
         ("s3","model-a","deepseek","",10,0,0,0)])
    con.executemany("INSERT INTO messages (session_id,role,timestamp) VALUES (?,?,?)",
        [("s1","user",now),("s1","user",now),("s1","assistant",now),
         ("s1","user",now),("s3","user",three)])
    con.commit(); con.close()

def c_record(home):
    with tempfile.TemporaryDirectory() as t:
        o = os.path.join(t, "otest")
        r = subprocess.run([CC,"-O2","-std=c11","-Isrc",
            os.path.join(ROOT,"src/agent/test_omarchy_usage_main.c"),
            os.path.join(ROOT,"src/agent/omarchy_usage.c"),
            "-lsqlite3","-ldl","-lm","-o",o], capture_output=True, text=True, cwd=ROOT)
        assert r.returncode == 0, r.stderr[-800:]
        r = subprocess.run([o, home], capture_output=True, text=True)
        return json.loads(r.stdout)

def omarchy_record(home):
    env = dict(os.environ)
    env.pop("HERMES_HOME", None); env["HOME"] = home
    env["OMARCHY_PATH"] = omarchy
    r = subprocess.run(["python3", os.path.join(omarchy,"bin","omarchy-agent-usage-hermes"),
                        "--force"], capture_output=True, text=True, env=env, cwd=omarchy)
    assert r.returncode == 0, r.stderr[-800:]
    return json.loads(r.stdout)

tmp = tempfile.mkdtemp(prefix="omusage_")
home = os.path.join(tmp, "home"); os.makedirs(os.path.join(home, ".hermes"))
db = os.path.join(home, ".hermes", "state.db")

build_synthetic(db)
c = c_record(os.path.join(home, ".hermes"))

print("C record sanity against the synthetic database:")
check("todayTotalTokens=265 (100+20+30+40 + 0+50+25+0)", c["todayTotalTokens"] == 265)
check("todaySessions=2 (deepseek s3 excluded)", c["todaySessions"] == 2)
check("totalSessions=2 (deepseek s3 excluded)", c["totalSessions"] == 2)
check("todayPrompts=3 (s3/assistant excluded)", c["todayPrompts"] == 3)
check("activeDays=1 (deepseek day excluded)", c["activeDays"] == 1)
check("modelUsage[model-a].inputTokens=100", c["modelUsage"].get("model-a",{}).get("inputTokens") == 100)
check("modelUsage[model-b].outputTokens=50", c["modelUsage"].get("model-b",{}).get("outputTokens") == 50)
check("7 recentDays", len(c["recentDays"]) == 7)
check("record declares ready:true, id=hermes", c.get("ready") is True and c.get("id") == "hermes")

if omarchy and os.path.isdir(os.path.join(omarchy, "bin")):
    print("A/B parity vs omarchy's own collector on the same state.db:")
    o = omarchy_record(home)
    for k in ["todayPrompts","todaySessions","todayTotalTokens","todayTokensByModel",
              "totalPrompts","totalSessions","activeDays","activeDates","modelUsage",
              "totalTokens","todayTokens"]:
        if k not in o: continue
        check(f"{k} matches", json.dumps(c.get(k), sort_keys=True) == json.dumps(o.get(k), sort_keys=True))
else:
    print("(omarchy checkout not provided; C self-checks above are the gate)")

shutil.rmtree(tmp, ignore_errors=True)
print("\n==== omarchy-usage parity:", "PASS" if not fail else f"{fail} FAILED", "====")
sys.exit(1 if fail else 0)
