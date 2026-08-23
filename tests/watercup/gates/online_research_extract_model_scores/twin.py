"""twin for online_research_extract_model_scores.

Traces the LIVE tools/online_research.py::_extract_model_scores with the
SAME rung ladder as the C harness:

  per-result L / per-model L / per-pattern L + B1 hit
  / context-window percent scan: per-index L, B2 digit, B3 '%', B4 range,
  V best-so-far / B5 no-valid-pct -> 0.5 boost -> V "0.5".

Input: JSON [[title, snippet], ...].
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

MODEL_PATTERNS = {
    "moonshotai/kimi-k2.6": ["kimi-k2.6", "kimi k2.6", "kimi k2"],
    "z-ai/glm-5": ["glm-5", "glm 5", "z.ai glm"],
    "minimaxai/minimax-m2.5": ["minimax-m2.5", "minimax m2.5", "minimax 2.5"],
    "nvidia/nemotron-3-ultra-550b-a55b": ["nemotron-3-ultra", "nemotron 3 ultra", "550b"],
    "nvidia/nemotron-4-340b-instruct": ["nemotron-4-340b", "nemotron 4 340b"],
    "nvidia/llama-3.1-nemotron-ultra-253b-v1": ["nemotron-ultra-253b", "nemotron ultra 253b"],
    "nvidia/nemotron-3-super-120b-a12b": ["nemotron-3-super", "nemotron 3 super", "120b"],
    "qwen/qwen3.5-397b-a17b": ["qwen3.5-397b", "qwen 3.5 397b"],
    "deepseek-ai/deepseek-v3.2": ["deepseek-v3.2", "deepseek v3.2"],
    "openai/gpt-oss-120b": ["gpt-oss-120b", "gpt oss 120b"],
    "meta/llama-3.1-405b-instruct": ["llama-3.1-405b", "llama 405b"],
}


def trace(c, results):
    scores = {}
    for title, snippet in results:
        c.loop()
        text = f"{title} {snippet}".lower()
        for model_id, patterns in MODEL_PATTERNS.items():
            c.loop()
            for pattern in patterns:
                c.loop()
                pos = text.find(pattern)
                c.branch(pos != -1)                        # B1
                if pos == -1:
                    continue
                cs = max(0, pos - 100)
                ce = min(len(text), pos + len(pattern) + 100)
                found_pct = False
                i = cs
                while i < ce:
                    c.loop()
                    is_dig = text[i].isdigit()
                    c.branch(is_dig)                       # B2
                    if not is_dig:
                        i += 1
                        continue
                    j = i
                    while j < ce and (text[j].isdigit() or text[j] == "."):
                        j += 1
                    num = text[i:j]
                    pct = j < ce and text[j] == "%"
                    c.branch(pct)                          # B3
                    if not pct:
                        i += 1
                        continue
                    try:
                        val = float(num)
                    except ValueError:
                        val = -1.0
                    valid = 0 <= val <= 100
                    c.branch(valid)                        # B4
                    if valid:
                        nv = val / 100.0
                        prev = scores.get(model_id, 0)
                        scores[model_id] = max(prev, nv)
                        found_pct = True
                        c.valstr("%.17g" % scores[model_id])
                        i = j + 1
                    else:
                        i += 1
                c.branch(not found_pct)                    # B5
                if not found_pct:
                    prev = scores.get(model_id, 0)
                    scores[model_id] = max(prev, 0.5)
                    c.valstr("0.5")
                break


def trace_top(results):
    c = Cup("extract_model_scores")
    trace(c, results)
    return c.finalize()


def run(inputs):
    import json
    return [trace_top(json.loads(s)) for s in inputs]


if __name__ == "__main__":
    # sanity vs LIVE fn (guarded — module pulls optional deps)
    try:
        import importlib.util as ilu
        spec = ilu.spec_from_file_location(
            "wc_live_online_research", os.path.join(DEV, "tools", "online_research.py"))
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_live_online_research"] = mod
        spec.loader.exec_module(mod)
        r = mod.OnlineResearcher.__new__(mod.OnlineResearcher)
        live = r._extract_model_scores(
            [mod.ResearchResult(title="Kimi K2", snippet="58.6%", url="", source="")],
            "q")
        print("live ok:", live)
    except Exception as e:
        print("live import skipped:", e)
