#!/usr/bin/env python3
"""Generate tests/fixtures/skills_hub_flow2.jsonl for the UNCLAIMED-remainder
FLOW port of hermes_cli/skills_hub.py.

Each line is {"func": <name>, "in": {...}}. The harness emits {"func","in",
"out"} and the oracle computes `out` from the LIVE quarry defs. All functions
have >= 6 cases; inputs stay inside the fixture domain documented in the port
header (ASCII text, string args, present direct-read object fields).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "skills_hub_flow2.jsonl"
cases = []


def case(func, **inp):
    cases.append({"func": func, "in": inp})


# ── _trust_cell ─────────────────────────────────────────────────────────────
case("_trust_cell", trust_level="builtin", source="official")
case("_trust_cell", trust_level="trusted", source="github")
case("_trust_cell", trust_level="community", source="official")
case("_trust_cell", trust_level="local", source="well-known")
case("_trust_cell", trust_level="mystery", source="clawhub")
case("_trust_cell", trust_level="builtin", source="official", official_label="official optional")

# ── _truncate ───────────────────────────────────────────────────────────────
case("_truncate", text="abcdefghij", width=5)
case("_truncate", text="abc", width=10)
case("_truncate", text="abcd", width=4)
case("_truncate", text="anything", width=0)
case("_truncate", text="abcdef", width=-2)
case("_truncate", text="", width=3)

# ── _ident_col ──────────────────────────────────────────────────────────────
case("_ident_col", style="bold cyan")
case("_ident_col", style="dim")
case("_ident_col", style="")
case("_ident_col", style="red")
case("_ident_col", style="bold")

# ── _row ────────────────────────────────────────────────────────────────────
_r = {"name": "alpha", "identifier": "@org/alpha", "source": "official", "installs": 0}
case("_row", r=_r, fields=["name", "identifier", "source"])
case("_row", r=_r, fields=["name"])
case("_row", r=_r, fields=[])
case("_row", r={"name": "beta", "note": None}, fields=["name", "note"])
case("_row", r={"name": "gamma", "flag": True, "count": 7}, fields=["flag", "count"])

# ── _slug_key ───────────────────────────────────────────────────────────────
case("_slug_key", text="Blender Bpy Enhanced")
case("_slug_key", text="my-skill_2.0")
case("_slug_key", text="  A B  ")
case("_slug_key", text="123")
case("_slug_key", text="")
case("_slug_key", text=None)

# ── _identifier_slug ────────────────────────────────────────────────────────
case("_identifier_slug", identifier="skills-sh/org/repo/blender-animation")
case("_identifier_slug", identifier="@org/my-skill")
case("_identifier_slug", identifier="plain")
case("_identifier_slug", identifier="trailing/")
case("_identifier_slug", identifier="")
case("_identifier_slug", identifier=None)

# ── _exact_name_hits ────────────────────────────────────────────────────────
_res = [
    {"name": "Blender Bpy Enhanced", "identifier": "@org/blender-bpy-enhanced", "source": "clawhub"},
    {"name": "my-skill", "identifier": "skills-sh/org/repo/my-skill", "source": "official"},
    {"name": "Other", "identifier": "@org/other", "source": "community"},
]
case("_exact_name_hits", name="my-skill", results=_res)          # exact display name
case("_exact_name_hits", name="Blender Bpy Enhanced", results=_res)  # exact
case("_exact_name_hits", name="blender-bpy-enhanced", results=_res)  # slug of display name
case("_exact_name_hits", name="my-skill", results=_res[:1] + _res[2:])  # identifier slug hit
case("_exact_name_hits", name="does-not-exist", results=_res)   # no match
case("_exact_name_hits", name="!!!", results=_res)              # empty slug key
case("_exact_name_hits", name="Other", results=[_res[0], dict(_res[2])])  # one exact

# ── _hub_rate_limited ───────────────────────────────────────────────────────
case("_hub_rate_limited", sources=[])
case("_hub_rate_limited", sources=[{"is_rate_limited": True}])
case("_hub_rate_limited", sources=[{"github": {"is_rate_limited": True}}])
case("_hub_rate_limited", sources=[{"is_rate_limited": False, "github": {"is_rate_limited": False}}])
case("_hub_rate_limited", sources=[{"is_rate_limited": False}, {"github": {"is_rate_limited": True}}])
case("_hub_rate_limited", sources=[{}])

# ── _hub_rejected_credentials ───────────────────────────────────────────────
case("_hub_rejected_credentials", sources=[])
case("_hub_rejected_credentials", sources=[{"auth": {"rejected": ["token-a"]}}])
case("_hub_rejected_credentials", sources=[{"auth": {"rejected": []}}, {"auth": {"rejected": ["token-b", "token-c"]}}])
case("_hub_rejected_credentials", sources=[{}])
case("_hub_rejected_credentials", sources=[{"auth": {"rejected": "not-a-list"}}])

# ── _upstream_repo ──────────────────────────────────────────────────────────
case("_upstream_repo", bundle={"source": "official", "metadata": {"upstream_repo": "NousResearch/hermes-agent"}})
case("_upstream_repo", bundle={"source": "official", "metadata": {"note": "vendored"}})
case("_upstream_repo", bundle={"source": "official"})
case("_upstream_repo", bundle={"source": "github", "metadata": {"upstream_repo": "x/y"}})
case("_upstream_repo", bundle={"source": "official", "metadata": {"upstream_repo": None}})

# ── _opt_value ──────────────────────────────────────────────────────────────
case("_opt_value", args=["install", "foo"], flag="--category", default="")
case("_opt_value", args=["--category", "tools", "foo"], flag="--category", default="")
case("_opt_value", args=["--page", "1", "--page", "9"], flag="--page", default="3")
case("_opt_value", args=["--page", "1", "--page", "9"], flag="--page", default="3", last=True)
case("_opt_value", args=["--page"], flag="--page", default="7")
case("_opt_value", args=[], flag="--source", default="all")

# ── _int_or ─────────────────────────────────────────────────────────────────
case("_int_or", text="42", default=10)
case("_int_or", text="-7", default=10)
case("_int_or", text=" 12 ", default=10)
case("_int_or", text="abc", default=10)
case("_int_or", text="3.5", default=10)
case("_int_or", text="", default=10)

# ── _opt_int ────────────────────────────────────────────────────────────────
case("_opt_int", args=["--page", "3"], flag="--page", default=1)
case("_opt_int", args=["--limit", "abc"], flag="--limit", default=25)
case("_opt_int", args=["--page", "1", "--page", "9"], flag="--page", default=1)
case("_opt_int", args=[], flag="--page", default=1)
case("_opt_int", args=["--page", "-2"], flag="--page", default=1)
case("_opt_int", args=["--page", " 4 "], flag="--page", default=1)

# ── _first_positional ───────────────────────────────────────────────────────
case("_first_positional", args=["install", "foo"])
case("_first_positional", args=["--force"])
case("_first_positional", args=[])
case("_first_positional", args=["--source", "all", "x"])
case("_first_positional", args=["foo"])

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in cases:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

# sanity: >=5 cases per func
from collections import Counter
counts = Counter(c["func"] for c in cases)
low = {k: v for k, v in counts.items() if v < 5}
print(f"wrote {len(cases)} cases across {len(counts)} funcs -> {OUT}")
if low:
    raise SystemExit(f"below 5 per func: {low}")
