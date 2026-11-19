"""twin for tools/xai_http.py:read_xai_imagine_storage_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/xai_http.py"
    PY_FN = "read_xai_imagine_storage_config"
    C_FN = "read_xai_imagine_storage_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
