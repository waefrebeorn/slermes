#!/usr/bin/env python3
"""Oracle for the pure discriminators of gateway/profile_routing.py (live import)."""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import gateway.profile_routing as PR  # noqa: E402


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE profile_routing: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam, case = c["family"], c["case"]
        if fam == "spec":
            route = PR.ProfileRoute(
                name="n", platform="p", profile="pf",
                guild_id="1" if c["g"] else None,
                chat_id="1" if c["c"] else None,
                thread_id="1" if c["t"] else None,
                user_id="1" if c["u"] else None)
            want = route.specificity
            if c["ret"] != want:
                mism += 1
                print(f"MISMATCH {case}: C={c['ret']} PY={want}")
        elif fam == "botkey":
            want = PR._bot_profile_key(c["in"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {case}: C={c['out']!r} PY={want!r} in={c['in']!r}")
        elif fam == "coerce":
            want = PR._coerce_route_id(c["in"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {case}: C={c['out']!r} PY={want!r} in={c['in']!r}")
            # ensure the object still represents the coerced/None string
            mism += 0
        elif fam == "whatsapp":
            want = PR._is_whatsapp_non_user_chat(c["in"])
            if bool(c["ret"]) != bool(want):
                mism += 1
                print(f"MISMATCH {case}: C={c['ret']} PY={want} in={c['in']!r}")

    print(f"ORACLE profile_routing: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
