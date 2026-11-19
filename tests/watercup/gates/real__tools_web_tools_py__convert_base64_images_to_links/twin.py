"""twin for tools/web_tools.py:convert_base64_images_to_links (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/web_tools.py"
    PY_FN = "convert_base64_images_to_links"
    C_FN = "convert_base64_images_to_links"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
