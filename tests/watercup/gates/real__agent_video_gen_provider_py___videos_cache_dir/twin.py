"""twin for agent/video_gen_provider.py:_videos_cache_dir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/video_gen_provider.py"
    PY_FN = "_videos_cache_dir"
    C_FN = "_videos_cache_dir"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
