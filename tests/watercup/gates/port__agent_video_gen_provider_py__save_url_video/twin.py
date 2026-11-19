"""twin for agent/video_gen_provider.py:save_url_video (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/video_gen_provider.py"
    PY_FN = "save_url_video"
    C_FN = "agent_video_gen_provider_save_url_video"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
