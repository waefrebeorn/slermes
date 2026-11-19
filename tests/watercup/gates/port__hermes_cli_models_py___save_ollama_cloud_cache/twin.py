"""twin for hermes_cli/models.py:_save_ollama_cloud_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "_save_ollama_cloud_cache"
    C_FN = "models_save_ollama_cloud_cache"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
