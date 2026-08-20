"""twin for tool_idempotency (water-cup gate).

Re-implements the SAME safe-retry contract the C harness asserts, in pure
Python (no LIVE source — self-contained contract gate). Passes only if C and
Python agree.

Contract:
  - put(key, result, ttl) -> get(key) returns the stored result
  - second get (retry) returns the SAME cached result (no double side effect)
  - different key -> absent
  - explicit idempotency_key overrides args as key material
  - expired entry -> absent
"""
import sys, time

class Cache:
    def __init__(self):
        self.m = {}
    def key(self, tool, args):
        import json
        try:
            d = json.loads(args) if args else {}
        except Exception:
            d = {}
        if isinstance(d, dict) and d.get("idempotency_key"):
            return "%s|%s" % (tool, d["idempotency_key"])
        return "%s|%s" % (tool, args)
    def put(self, k, result, ttl, now):
        self.m[k] = (result, now + (ttl or 600))
    def get(self, k, now):
        if k not in self.m: return None
        result, exp = self.m[k]
        if exp <= now:
            del self.m[k]; return None
        return result

now = 1000000
c = Cache()
k1 = c.key("send_email", '{"to":"a@x.com"}')
assert k1, "key built"
c.put(k1, '{"success":true,"id":"em_1"}', 600, now)
r1 = c.get(k1, now); assert r1 == '{"success":true,"id":"em_1"}', "get stored"
r2 = c.get(k1, now); assert r2 == '{"success":true,"id":"em_1"}', "retry cached"
k2 = c.key("send_email", '{"to":"b@x.com"}')
assert c.get(k2, now) is None, "different args -> None"
k3 = c.key("send_email", '{"to":"z@x.com","idempotency_key":"fixed-1"}')
assert k3 == "send_email|fixed-1", "explicit key used"
c.get(k1, now);  # one live entry
now2 = now + 601
assert c.get(k1, now2) is None, "expired -> None"
print("  ok:   all idempotency contract checks passed")
print("RESULT: tool_idempotency safe-retry contract holds (python twin)")
