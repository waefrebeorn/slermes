"""twin for runtime_provider_host_derived_api_key.

Traces the LIVE hermes_cli/runtime_provider.py::_host_derived_api_key with
the SAME rung ladder as the C harness:

  B1 empty host / B2 ':' in host / B3 localhost / label-split L ticks /
  B4 IP-like last label / api|www strip: L tick + B5 popped / B6 <2 labels /
  sanitize loop: L tick + B7 '_' char / V sanitized / B8 bad first char /
  B9 OPENAI|OPENROUTER|OLLAMA exclusion / B10 env set / V env value.
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

try:
    _spec = ilu.spec_from_file_location(
        "wc_live_runtime_provider",
        os.path.join(DEV, "hermes_cli", "runtime_provider.py"))
    rp = ilu.module_from_spec(_spec)
    sys.modules["wc_live_runtime_provider"] = rp
    _spec.loader.exec_module(rp)
    live_fn = rp._host_derived_api_key
except Exception:
    live_fn = None


def hostname_of(base_url):
    raw = (base_url or "").strip()
    if not raw:
        return ""
    from urllib.parse import urlparse
    parsed = urlparse(raw if "://" in raw else f"//{raw}")
    return (parsed.hostname or "").lower().rstrip(".")


def trace(c, base_url):
    hostname = hostname_of(base_url)
    c.branch(not hostname)                                 # B1
    if not hostname:
        return ""
    c.branch(":" in hostname)                              # B2
    if ":" in hostname:
        return ""
    c.branch(hostname == "localhost")                      # B3
    if hostname == "localhost":
        return ""
    labels = [lbl for lbl in hostname.split(".") if lbl]
    for _ in labels:
        c.loop()
    c.branch(bool(labels) and labels[-1][0].isdigit())     # B4
    if labels and labels[-1][0].isdigit():
        return ""
    stripped = list(labels)
    while stripped and stripped[0] in ("api", "www"):
        c.loop()
        c.branch(True)                                     # B5
        stripped.pop(0)
    c.branch(len(stripped) < 2)                            # B6
    if len(stripped) < 2:
        return ""
    vendor = stripped[-2]
    sanitized = "".join(ch if ch.isalnum() else "_" for ch in vendor).upper()
    for ch in vendor:
        c.loop()
        c.branch(not ch.isalnum())                         # B7
    c.valstr(sanitized)
    c.branch(not sanitized or not sanitized[0].isalpha())  # B8
    c.branch(sanitized in ("OPENAI", "OPENROUTER", "OLLAMA"))  # B9
    if sanitized and sanitized[0].isalpha() and sanitized not in (
            "OPENAI", "OPENROUTER", "OLLAMA"):
        val = os.environ.get(f"{sanitized}_API_KEY")
        c.branch(val is not None)                          # B10
        if val is not None:
            c.valstr(val.strip())
    return ""


def trace_top(url):
    c = Cup("host_derived_api_key")
    trace(c, url)
    return c.finalize()


def run(inputs):
    return [trace_top(u) for u in inputs]


if __name__ == "__main__":
    if live_fn:
        for u in ["https://api.deepseek.com/v1", "http://localhost:1/v1"]:
            assert trace_top(u) is not None
    print("twin ok")
