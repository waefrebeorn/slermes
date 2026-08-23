"""twin for model_normalize_detect_vendor.

Imports LIVE hermes_cli/model_normalize.py::detect_vendor and traces the SAME
branch ladder as the C harness (empty check, slash check, exact-token loop,
startswith loop). The table walk order mirrors the C port's VENDOR_PREFIXES
order, which matches the LIVE dict insertion order.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_model_normalize", os.path.join(DEV, "hermes_cli", "model_normalize.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_model_normalize"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

# Same order as the C harness's TOKS/VENDS tables (= LIVE dict insertion order).
TOKS = ["claude", "gpt", "o1", "o3", "o4", "gemini", "gemma", "deepseek", "glm",
        "kimi", "minimax", "grok", "qwen", "mimo", "trinity", "nemotron",
        "llama", "step"]
VENDS = ["anthropic", "openai", "openai", "openai", "openai", "google", "google",
         "deepseek", "z-ai", "moonshotai", "minimax", "x-ai", "qwen", "xiaomi",
         "arcee-ai", "nvidia", "meta-llama", "stepfun"]


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("detect_vendor")
        name_lower = raw.lower()          # C lowercases its working copy up front
        c.branch(name_lower != "")        # b0: empty -> None
        result = ""
        done = False
        if not name_lower:
            done = True
        c.branch("/" not in name_lower)   # b1: '/' in name?
        if "/" in name_lower and not done:
            prefix = name_lower.split("/", 1)[0]
            result = prefix if prefix else ""
            done = True
        if not done:
            first_token = name_lower.split("-")[0]
            hit = -1
            for i, tok in enumerate(TOKS):
                c.loop()
                if first_token == tok:
                    hit = i
                    break
            c.branch(hit >= 0)            # b2: exact token hit
            if hit >= 0:
                result = VENDS[hit]
            else:
                hit = -1
                for i, tok in enumerate(TOKS):
                    c.loop()
                    if name_lower.startswith(tok):
                        hit = i
                        break
                c.branch(hit >= 0)        # b3: startswith hit
                if hit >= 0:
                    result = VENDS[hit]
        # cross-check against the LIVE fn so a silent divergence fails loudly
        live = mod.detect_vendor(raw)
        assert (live or "") == result, "LIVE diverged from traced ladder on %r: %r vs %r" % (raw, live, result)
        c.valstr(result)
        cups.append(c.finalize())
    return cups
