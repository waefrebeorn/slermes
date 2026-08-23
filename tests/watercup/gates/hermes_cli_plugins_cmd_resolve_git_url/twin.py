"""twin for hermes_cli_plugins_cmd_resolve_git_url.

Traces the LIVE hermes_cli/plugins_cmd.py::_resolve_git_url with the SAME
rung order as the C harness:
  B1 is-url / B2 github-browser / B3 tree-subdir /
  B4 '#' fragment / B5 .git/ boundary / B6 shorthand /
  VSTR("<url>|<subdir>" or "INVALID" for ValueError).
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_plugins_cmd", os.path.join(DEV, "hermes_cli", "plugins_cmd.py"))
_pcmd = ilu.module_from_spec(_spec)
sys.modules["wc_live_hermes_cli_plugins_cmd"] = _pcmd
try:
    _spec.loader.exec_module(_pcmd)
    _live_fn = _pcmd._resolve_git_url
except Exception:
    raise

SEGMENTS = {"actions", "blob", "commit", "commits", "issues",
            "pull", "pulls", "releases", "tree", "wiki"}


def trace(identifier):
    c = Cup("pcmd_resolve_git_url")
    is_url = identifier.startswith(("https://", "http://", "git@", "ssh://", "file://"))
    c.branch(is_url)                                              # B1

    gh_hit = False
    tree_sub = False
    has_hash = False
    has_git_boundary = False
    shorthand = False

    if is_url:
        if identifier.startswith("https://github.com/"):
            path = identifier[len("https://github.com/"):]
            path = path.split("?", 1)[0].split("#", 1)[0].strip("/")
            parts = [p for p in path.split("/") if p]
            gh_hit = len(parts) >= 3 and all(parts[:2]) and parts[2] in SEGMENTS
            c.branch(gh_hit)                                      # B2
            if gh_hit:
                tree_sub = parts[2] == "tree" and len(parts) >= 5
                c.branch(tree_sub)                                # B3
            else:
                c.branch(False)
        else:
            c.branch(False)                                       # B2
            c.branch(False)                                       # B3
        has_hash = "#" in identifier
        has_git_boundary = ".git/" in identifier
        c.branch(has_hash)                                        # B4
        c.branch(has_git_boundary)                                # B5
        c.branch(False)                                           # B6 (not shorthand)
    else:
        c.branch(False)                                           # B2
        c.branch(False)                                           # B3
        parts = [p for p in identifier.strip("/").split("/") if p]
        shorthand = len(parts) >= 2
        c.branch(False)                                           # B4
        c.branch(False)                                           # B5
        c.branch(shorthand)                                       # B6
        if not shorthand:
            c.valstr("INVALID")
            return c.finalize()

    try:
        git_url, subdir = _live_fn(identifier)
    except ValueError:
        c.valstr("INVALID")
        return c.finalize()
    c.valstr("%s|%s" % (git_url, subdir if subdir is not None else "-"))
    return c.finalize()


def run(inputs):
    return [trace(t) for t in inputs]
