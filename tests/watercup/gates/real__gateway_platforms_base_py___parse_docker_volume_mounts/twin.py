"""twin for gateway/platforms/base.py:_parse_docker_volume_mounts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_parse_docker_volume_mounts"
    C_FN = "_parse_docker_volume_mounts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
