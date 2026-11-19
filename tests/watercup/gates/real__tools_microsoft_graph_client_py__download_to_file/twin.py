"""twin for tools/microsoft_graph_client.py:download_to_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/microsoft_graph_client.py"
    PY_FN = "download_to_file"
    C_FN = "download_to_file"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
