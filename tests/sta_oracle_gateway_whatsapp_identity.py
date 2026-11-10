#!/usr/bin/env python3
"""Oracle for the pure JID helpers of gateway/whatsapp_identity.py (live import)."""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import gateway.whatsapp_identity as WI  # noqa: E402


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
        print("ORACLE whatsapp_identity: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam, case, inp, got = c["family"], c["case"], c["in"], c["out"]
        if fam == "normalize":
            want = WI.normalize_whatsapp_identifier(inp)
        elif fam == "tojid":
            want = WI.to_whatsapp_jid(inp)
        elif fam == "mention":
            want = WI.normalize_whatsapp_mention_jid(inp)
        else:
            continue
        if got != want:
            mism += 1
            print(f"MISMATCH {case}: C={got!r} PY={want!r} in={inp!r}")

    print(f"ORACLE whatsapp_identity: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
