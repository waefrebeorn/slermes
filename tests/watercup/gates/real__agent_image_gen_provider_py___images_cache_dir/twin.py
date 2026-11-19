"""twin for agent/image_gen_provider.py:_images_cache_dir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/image_gen_provider.py"
    PY_FN = "_images_cache_dir"
    C_FN = "_images_cache_dir"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
