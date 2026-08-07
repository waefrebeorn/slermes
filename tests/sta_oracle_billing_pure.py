"""Oracle for agent/billing_view.py pure helpers."""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT") or os.path.expanduser("~/.hermes/hermes-agent")
sys.path.insert(0, DEV_ROOT)

from agent.billing_view import _parse_payment_method


def run(c):
    op = c.get("op")
    if op == "ts_parse_payment_method":
        raw = c.get("value")
        result = _parse_payment_method(raw)
        if result is None:
            return "{}"
        # Dataclass → dict, preserving field order (asdict order = dataclass order)
        fields = getattr(result, "__dataclass_fields__", {})
        out = {}
        for fname in fields:
            v = getattr(result, fname)
            if v is None:
                out[fname] = None
            else:
                out[fname] = v
        return json.dumps(out, sort_keys=True)
    return None


def main():
    fixture = sys.argv[1]
    cases = json.load(open(fixture))
    for c in cases:
        r = run(c)
        if r is None:
            print("none")
        else:
            print(r)


if __name__ == "__main__":
    main()
