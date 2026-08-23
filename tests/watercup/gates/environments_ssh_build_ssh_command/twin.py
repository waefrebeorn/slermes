"""twin for environments_ssh_build_ssh_command — drives the LIVE
tools/environments/ssh.py::SSHEnvironment._build_ssh_command on an
uninitialized instance (the constructor performs a real SSH dial)."""
import os, sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


def _load_live():
    try:
        sys.path.insert(0, DEV)
        spec = ilu.spec_from_file_location(
            "wc_live_sshenv", os.path.join(DEV, "tools", "environments", "ssh.py"))
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_live_sshenv"] = mod
        spec.loader.exec_module(mod)
        return mod.SSHEnvironment, mod._SSH_MULTIPLEX
    except Exception:
        return None, None


_SSHCls, _MULTIPLEX = _load_live()


def _fallback(user, host, port, key_path, csock):
    cmd = ["ssh",
           "-o", f"ControlPath={csock}",
           "-o", "ControlMaster=auto",
           "-o", "ControlPersist=300",
           "-o", "BatchMode=yes",
           "-o", "StrictHostKeyChecking=accept-new",
           "-o", "ConnectTimeout=10"]
    if port != 22:
        cmd.extend(["-p", str(port)])
    if key_path:
        cmd.extend(["-i", key_path])
    cmd.append(f"{user}@{host}")
    return " ".join(cmd)


def trace(raw):
    raw = raw.replace("\\t", "\t")
    parts = raw.split("\t") + [""] * 5
    user, host = parts[0], parts[1]
    port = int(parts[2] or "22")
    key_path, csock = parts[3], parts[4]

    c = Cup("_build_ssh_command")
    c.branch(bool(_MULTIPLEX))                  # B1
    c.branch(port != 22)                        # B2
    c.branch(bool(key_path))                    # B3

    if _SSHCls is not None:
        env = object.__new__(_SSHCls)
        env.host = host
        env.user = user
        env.port = port
        env.key_path = key_path
        env.control_socket = csock
        result = " ".join(_SSHCls._build_ssh_command(env))
    else:
        result = _fallback(user, host, port, key_path, csock)
    c.valstr(result)
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
