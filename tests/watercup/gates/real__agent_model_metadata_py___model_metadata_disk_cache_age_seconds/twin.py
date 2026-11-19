"""twin for agent/model_metadata.py:_model_metadata_disk_cache_age_seconds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"
    PY_FN = "_model_metadata_disk_cache_age_seconds"
    C_FN = "cli_agent_model_metadata__model_metadata_disk_cache_age_seconds"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
