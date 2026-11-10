#!/usr/bin/env python3
"""Oracle for the micro pure helpers of port_micro_env.c (5 functions across 3
live modules). The source modules do not import cleanly (heavy gateway graph), so
the helpers are reconstructed faithfully from their live source:

  strip:  {family:"strip", out:[]}           terminal_env_provider.Base.strip_env_keys
                                             (a method of Base -> empty frozenset [])

  desc:   {family:"desc", name:<str>, out:str}
                                             Base.env_description (property using
                                             self.display_name -> the harness name)
                                             -> "a {name} environment (likely Linux)"

  aaen:   {family:"aaen", prefix:<str>, out:[str]}
                                             access_policy_mixin._allow_all_env_names
                                             -> ["GATEWAY_ALLOW_ALL_USERS",
                                                 "{PREFIX}_ALLOW_ALL_USERS"]

  norm:   {family:"norm", name:<str|null>, out:str}
                                             host_topology._normalized(name) ->
                                             "" if falsy else normalize_profile_name
  serves: {family:"serves", profiles:<list>, profile:<str>, out:0|1}
                                             HostGatewayTopology.serves — normalized
                                             wanted in {normalized(p) for p in profiles}
"""
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def normalize_profile_name(name):
    if not isinstance(name, str):
        raise ValueError("profile name must be a string")
    stripped = name.strip()
    if not stripped:
        raise ValueError("profile name cannot be empty")
    if stripped.casefold() == "default":
        return "default"
    return stripped.lower()


def _normalized(name):
    if not name:
        return ""
    return normalize_profile_name(name)


def _env_description(display_name):
    return f"a {display_name} environment (likely Linux)"


def _allow_all_env_names(prefix):
    return ["GATEWAY_ALLOW_ALL_USERS", f"{prefix}_ALLOW_ALL_USERS"]


def _serves(profiles, profile_name):
    wanted = _normalized(profile_name)
    return bool(wanted) and wanted in {_normalized(p) for p in (profiles or [])}


def main() -> int:
    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fam = c.get("family")
        if fam == "strip":
            want = []
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH strip C={got} PY={want}")
        elif fam == "desc":
            want = _env_description(c.get("name"))
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH desc name={c.get('name')!r} C={got!r} PY={want!r}")
        elif fam == "aaen":
            want = _allow_all_env_names(c.get("prefix") or "")
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH aaen prefix={c.get('prefix')!r} C={got} PY={want}")
        elif fam == "norm":
            try:
                want = _normalized(c.get("name"))
            except ValueError:
                want = ""
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH norm name={c.get('name')!r} C={got!r} PY={want!r}")
        elif fam == "serves":
            profiles = c.get("profiles")
            want = 1 if _serves(profiles, c.get("profile")) else 0
            got = int(c.get("out"))
            if want != got:
                mism += 1
                print(f"MISMATCH serves profiles={profiles} profile={c.get('profile')!r} C={got} PY={want}")
    if total == 0:
        print("ORACLE micro_env: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE micro_env: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
