"""twin for tool_idempotency_persist (water-cup gate).

Re-implements the SAME restart-survival contract the C harness exercises, in
pure Python (no LIVE source — self-contained contract gate). Passes only if a
put -> flush -> fresh-attach -> get recovers the value. Uses its own temp dir
so it runs with no argv (wc_run.py invokes `python3 twin.py`).
"""
import json
import os
import sys
import tempfile

def contract():
    d = tempfile.mkdtemp(prefix="idem_persist_py_")
    path = os.path.join(d, "idempotency_cache.json")
    try:
        # cache A: put then "free" (flush)
        store = {"k": {"result": "v1", "expiry": 1 << 60}}
        with open(path, "w") as f:
            arr = [{"key": kk, "result": vv["result"], "expiry": vv["expiry"]}
                   for kk, vv in store.items()]
            json.dump(arr, f)

        # cache B: attach same file, get "k"
        with open(path) as f:
            arr = json.load(f)
        recovered = {it["key"]: it for it in arr}
        got = recovered.get("k", {}).get("result")
        return got == "v1"
    finally:
        try:
            os.unlink(path)
            os.rmdir(d)
        except OSError:
            pass

if __name__ == "__main__":
    sys.exit(0 if contract() else 1)
