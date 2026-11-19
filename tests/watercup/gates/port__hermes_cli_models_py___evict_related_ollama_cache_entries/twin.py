"""twin for hermes_cli/models.py:_evict_related_ollama_cache_entries (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "_evict_related_ollama_cache_entries"
    C_FN = "models_evict_related_ollama_cache_entries"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
