"""twin for agent/video_gen_provider.py:_create_and_poll (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/video_gen_provider.py"
    PY_FN = "_create_and_poll"
    C_FN = "agent_video_gen_provider_u_create_and_poll"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
