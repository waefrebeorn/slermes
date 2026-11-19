#!/usr/bin/env python3
"""gen_tail_revive2_cases.py — regenerate tests/fixtures/tail_revive2.jsonl.

Case shape: {"func": <port name>, "in": {..., "io": {seam: script}}}.
Script semantics (shared with the harness/oracle): plain value = constant,
bare list = per-call queue, {"__script__": true, "ret": v} = constant
wrapper (REQUIRED for list-valued results), {"__script__": true,
"raise": {"type","message"}} = exception. Run from the repo root:
  python tests/gen_tail_revive2_cases.py
"""
import json
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret):
    return {"__script__": True, "ret": ret}


def raise_(ty, msg="", **extra):
    return {"__script__": True, "raise": {"type": ty, "message": msg, **extra}}


def timeout_expired(cmd="x", timeout=5):
    msg = str(subprocess.TimeoutExpired(cmd, timeout))
    return raise_("TimeoutExpired", msg, cmd=cmd, timeout=timeout)


# ── is_unattended_platform_approval_context ─────────────────────────────────
for plat in ("webhook", "msgraph_webhook", "api_server", "telegram", ""):
    add("is_unattended_platform_approval_context", io={"session_env": plat})
add("is_unattended_platform_approval_context", io={"session_env": None})
add("is_unattended_platform_approval_context",
    io={"session_env": raise_("SystemExit", "boom")})
add("is_unattended_platform_approval_context",
    io={"session_env": script([1, 2])})  # unhashable list -> TypeError

# ── is_single_query_approval_context ────────────────────────────────────────
add("is_single_query_approval_context", io={"finite_turn_required": True})
add("is_single_query_approval_context",
    io={"finite_turn_required": False, "session_env": "1"})
add("is_single_query_approval_context",
    io={"finite_turn_required": False, "session_env": " TRUE "})
add("is_single_query_approval_context",
    io={"finite_turn_required": False, "session_env": "0"})
add("is_single_query_approval_context", finite_import=False,
    io={"session_env": "yes"})
add("is_single_query_approval_context", finite_import=False,
    io={"session_env": ""})
add("is_single_query_approval_context",
    io={"finite_turn_required": raise_("ImportError", "call raised")})
add("is_single_query_approval_context",
    io={"finite_turn_required": raise_("RuntimeError", "x")})
add("is_single_query_approval_context",
    io={"finite_turn_required": False, "session_env": raise_("SystemExit", "sq")})
add("is_single_query_approval_context",
    io={"finite_turn_required": 1, "session_env": "on"})  # 1 is True -> False
add("is_single_query_approval_context",
    io={"finite_turn_required": None, "session_env": "nope"})

# ── should_fall_through_to_cli_approval ─────────────────────────────────────
add("should_fall_through_to_cli_approval", is_cli=True, approval_callback={}, notify_cb=None)
add("should_fall_through_to_cli_approval", is_cli=True, approval_callback=None, notify_cb=None)
add("should_fall_through_to_cli_approval", is_cli=True, approval_callback={}, notify_cb={})
add("should_fall_through_to_cli_approval", is_cli=False, approval_callback={}, notify_cb=None)
add("should_fall_through_to_cli_approval", is_cli=0, approval_callback={}, notify_cb=None)
add("should_fall_through_to_cli_approval", is_cli="", approval_callback={}, notify_cb=None)
add("should_fall_through_to_cli_approval", is_cli="yes", approval_callback={}, notify_cb=None)

# ── is_shell_token_spliced_gateway_lifecycle ────────────────────────────────
add("is_shell_token_spliced_gateway_lifecycle", command="spliced verb example",
    io={"lifecycle_guard": True})
add("is_shell_token_spliced_gateway_lifecycle", command="git status",
    io={"lifecycle_guard": False})
add("is_shell_token_spliced_gateway_lifecycle", command="x", guard_import=False)
add("is_shell_token_spliced_gateway_lifecycle", command="x",
    io={"lifecycle_guard": raise_("RuntimeError", "guard down")})
add("is_shell_token_spliced_gateway_lifecycle", command="x",
    io={"lifecycle_guard": raise_("ImportError", "no cron")})
add("is_shell_token_spliced_gateway_lifecycle", command="x",
    io={"lifecycle_guard": raise_("SystemExit", "bye")})
add("is_shell_token_spliced_gateway_lifecycle", command="",
    io={"lifecycle_guard": False})
add("is_shell_token_spliced_gateway_lifecycle", command="x",
    io={"lifecycle_guard": raise_("KeyboardInterrupt", "ki")})

# ── argv_is_gateway_run ─────────────────────────────────────────────────────
for argv in (["hermes", "gateway", "run"], ["gateway", "run", "--x"],
             ["gateway", "start"], [], ["gateway"], ["run", "gateway"],
             [1, "gateway", "run"], ["gateway", 2, "run"], ["x", "gateway", "run"]):
    add("argv_is_gateway_run", argv=argv)

# ── desktop_linux_userns_sandbox_available ──────────────────────────────────
add("desktop_linux_userns_sandbox_available", platform="win32")
add("desktop_linux_userns_sandbox_available", platform="darwin")
add("desktop_linux_userns_sandbox_available", platform="linux",
    io={"shutil.which": None})
add("desktop_linux_userns_sandbox_available", platform="linux",
    io={"shutil.which": ""})
add("desktop_linux_userns_sandbox_available", platform="linux",
    io={"shutil.which": "/usr/bin/unshare", "subprocess.run": {"returncode": 0}})
add("desktop_linux_userns_sandbox_available", platform="linux",
    io={"shutil.which": "/usr/bin/unshare", "subprocess.run": {"returncode": 1}})
add("desktop_linux_userns_sandbox_available", platform="linux",
    io={"shutil.which": "/usr/bin/unshare",
        "subprocess.run": raise_("OSError", "no exec")})
add("desktop_linux_userns_sandbox_available", platform="linux",
    io={"shutil.which": "/usr/bin/unshare", "subprocess.run": timeout_expired()})
add("desktop_linux_userns_sandbox_available", platform="linux",
    io={"shutil.which": "/usr/bin/unshare",
        "subprocess.run": raise_("RuntimeError", "z")})
add("desktop_linux_userns_sandbox_available", platform="linux",
    io={"shutil.which": raise_("SystemExit", "which")})
add("desktop_linux_userns_sandbox_available", platform="linux",
    io={"shutil.which": "/usr/bin/unshare", "subprocess.run": None})

# ── desktop_linux_needs_disable_setuid_sandbox ──────────────────────────────
REG = 0o100000
SUID = 0o4755
add("desktop_linux_needs_disable_setuid_sandbox", platform="win32",
    packaged_executable="/opt/hermes/hermes")
add("desktop_linux_needs_disable_setuid_sandbox", platform="linux",
    packaged_executable="/opt/hermes/hermes",
    io={"sandbox_helper_lstat": {"sandbox": "/opt/hermes/chrome-sandbox", "st": None}})
add("desktop_linux_needs_disable_setuid_sandbox", platform="linux",
    packaged_executable="/opt/hermes/hermes",
    io={"sandbox_helper_lstat": {"sandbox": "/opt/hermes/chrome-sandbox",
                                 "st": {"st_mode": REG | SUID, "st_uid": 0}}})
add("desktop_linux_needs_disable_setuid_sandbox", platform="linux",
    packaged_executable="/opt/hermes/hermes",
    io={"sandbox_helper_lstat": {"sandbox": "/opt/hermes/chrome-sandbox",
                                 "st": {"st_mode": REG | 0o755, "st_uid": 0}}})
add("desktop_linux_needs_disable_setuid_sandbox", platform="linux",
    packaged_executable="/opt/hermes/hermes",
    io={"sandbox_helper_lstat": {"sandbox": "/opt/hermes/chrome-sandbox",
                                 "st": {"st_mode": REG | 0o755, "st_uid": 1000}}})
add("desktop_linux_needs_disable_setuid_sandbox", platform="linux",
    packaged_executable="/opt/hermes/hermes",
    io={"sandbox_helper_lstat": {"sandbox": "/opt/hermes/chrome-sandbox",
                                 "st": {"st_mode": 0o040755, "st_uid": 0}}})
add("desktop_linux_needs_disable_setuid_sandbox", platform="linux",
    packaged_executable="/opt/hermes/hermes",
    io={"sandbox_helper_lstat": {"sandbox": "/opt/hermes/chrome-sandbox",
                                 "st": {"st_mode": REG | SUID, "st_uid": 1000}}})
add("desktop_linux_needs_disable_setuid_sandbox", platform="linux",
    packaged_executable="/opt/hermes/hermes",
    io={"sandbox_helper_lstat": raise_("SystemExit", "lstat")})

# ── is_plugin_id ────────────────────────────────────────────────────────────
for raw in ("", ".", "..", "a", "plug/in", "a/./b", "a/../b", "../a", "/a",
            "C:a", "C:/a", "C:..", "//server/share", "a/.", ".hidden",
            "a\\\\b", "a/..b", "plug/in/", "a/.../b", "....", "C:", "a b"):
    add("is_plugin_id", raw=raw)

# ── accepts_var_kwargs ──────────────────────────────────────────────────────
for params in (["VAR_KEYWORD"], ["POSITIONAL_OR_KEYWORD", "VAR_KEYWORD"],
               ["POSITIONAL_OR_KEYWORD"], [], ["VAR_POSITIONAL", "KEYWORD_ONLY"]):
    add("accepts_var_kwargs", io={"inspect.signature": {"parameters": params}})
add("accepts_var_kwargs", io={"inspect.signature": raise_("TypeError", "not callable")})
add("accepts_var_kwargs", io={"inspect.signature": raise_("ValueError", "no sig")})
add("accepts_var_kwargs", io={"inspect.signature": raise_("RuntimeError", "boom")})

# ── _has_manifest ───────────────────────────────────────────────────────────
P = "/plugins/x"
add("_has_manifest", path=P, io={"path.exists": [True]})
add("_has_manifest", path=P, io={"path.exists": [False], "path.is_symlink": [True]})
add("_has_manifest", path=P, io={"path.exists": [False, True], "path.is_symlink": [False]})
add("_has_manifest", path=P,
    io={"path.exists": [False, False, False], "path.is_symlink": [False, False, True]})
add("_has_manifest", path=P,
    io={"path.exists": [False, False, False], "path.is_symlink": [False, False, False]})
add("_has_manifest", path=P, io={"path.exists": [raise_("OSError", "perm")]})
add("_has_manifest", path=P,
    io={"path.exists": [False], "path.is_symlink": [raise_("SystemExit", "bye")]})

# ── holds_plugin ────────────────────────────────────────────────────────────
add("holds_plugin", path=P, io={"path.is_dir": False})
add("holds_plugin", path=P, io={"path.is_dir": True, "has_manifest": True})
add("holds_plugin", path=P,
    io={"path.is_dir": True, "has_manifest": False, "category_plugins": script(["p1"])})
add("holds_plugin", path=P,
    io={"path.is_dir": True, "has_manifest": False, "category_plugins": script([])})
add("holds_plugin", path=P, io={"path.is_dir": raise_("RuntimeError", "dir")})
add("holds_plugin", path=P,
    io={"path.is_dir": True, "has_manifest": raise_("SystemExit", "hm")})
add("holds_plugin", path=P,
    io={"path.is_dir": True, "has_manifest": False,
        "category_plugins": raise_("ValueError", "cat")})

# ── pid_is_hermes ───────────────────────────────────────────────────────────
def tag(kind, value=None):
    return {"kind": kind, "value": value}

add("pid_is_hermes", pid=tag("int", 123), is_windows=False, expected_start_time=None)
add("pid_is_hermes", pid=tag("bool", True), is_windows=False, expected_start_time=None)
add("pid_is_hermes", pid=tag("int", 0), is_windows=False, expected_start_time=None)
add("pid_is_hermes", pid=tag("int", -5), is_windows=False, expected_start_time=None)
add("pid_is_hermes", pid=tag("float", 123.0), is_windows=True, expected_start_time=None)
add("pid_is_hermes", pid=tag("str", "123"), is_windows=True, expected_start_time=None)
add("pid_is_hermes", pid=tag("null"), is_windows=True, expected_start_time=None)
add("pid_is_hermes", pid=tag("int", 123), is_windows=True, expected_start_time=None,
    io={"process_start_time": 1000, "process_command_is_hermes": True})
add("pid_is_hermes", pid=tag("int", 123), is_windows=True, expected_start_time=1000,
    io={"process_start_time": 1000, "process_command_is_hermes": True})
add("pid_is_hermes", pid=tag("int", 123), is_windows=True, expected_start_time=999,
    io={"process_start_time": 1000})
add("pid_is_hermes", pid=tag("int", 123), is_windows=True, expected_start_time=None,
    io={"process_start_time": None})
add("pid_is_hermes", pid=tag("int", 123), is_windows=True, expected_start_time=None,
    io={"process_start_time": raise_("OSError", "gone")})
add("pid_is_hermes", pid=tag("int", 123), is_windows=True, expected_start_time=None,
    io={"process_start_time": 1000, "process_command_is_hermes": raise_("RuntimeError", "x")})
add("pid_is_hermes", pid=tag("int", 123), is_windows=True, expected_start_time=None,
    io={"process_start_time": 1000, "process_command_is_hermes": raise_("SystemExit", "pid")})
add("pid_is_hermes", pid=tag("int", 123), is_windows=False, expected_start_time=500,
    io={"process_start_time": 500})
add("pid_is_hermes", pid=tag("int", 123), is_windows=False, expected_start_time=500,
    io={"process_start_time": raise_("ValueError", "st")})
add("pid_is_hermes", pid=tag("int", 123), is_windows=False, expected_start_time=500,
    io={"process_start_time": None})

# ── process_command_is_hermes ───────────────────────────────────────────────
add("process_command_is_hermes", pid=7,
    io={"psutil.Process": raise_("NoSuchProcess", "no such process")})
add("process_command_is_hermes", pid=7,
    io={"psutil.Process": True,
        "psutil.cmdline": script(["python", "hermes_cli/main.py"]),
        "psutil.exe": ""})
add("process_command_is_hermes", pid=7,
    io={"psutil.Process": True,
        "psutil.cmdline": script(["C:\\Users\\x\\.hermes\\bin\\hermes.exe"]),
        "psutil.exe": None})
add("process_command_is_hermes", pid=7,
    io={"psutil.Process": True, "psutil.cmdline": script(["shermesa"]), "psutil.exe": ""})
add("process_command_is_hermes", pid=7,
    io={"psutil.Process": True, "psutil.cmdline": None, "psutil.exe": "/usr/bin/hermes"})
add("process_command_is_hermes", pid=7,
    io={"psutil.Process": True, "psutil.cmdline": script([]), "psutil.exe": ""})
add("process_command_is_hermes", pid=7,
    io={"psutil.Process": True, "psutil.cmdline": raise_("OSError", "cl")})
add("process_command_is_hermes", pid=7,
    io={"psutil.Process": True, "psutil.cmdline": script(["x"]),
        "psutil.exe": raise_("SystemExit", "exe")})
add("process_command_is_hermes", pid=7,
    io={"psutil.Process": True, "psutil.cmdline": script([123])})
add("process_command_is_hermes", pid=7,
    io={"psutil.Process": raise_("SystemExit", "proc")})
add("process_command_is_hermes", pid=7,
    io={"psutil.Process": True, "psutil.cmdline": script(["notepad.exe"]),
        "psutil.exe": "C:\\hermes\\x.exe"})

# ── text_names_hermes ───────────────────────────────────────────────────────
for text in ("hermes", "C:\\x\\hermes.exe run", "shermesa", ".hermes",
             "x=hermes;y", "HERMES", "hermes\u00a0x", "", "a/hermes/b",
             "hermesx", "xhermes", "her'mes", "hermes\u2028y", "\x1chermes"):
    add("text_names_hermes", text=text)

# ── serialized_messages_bytes ───────────────────────────────────────────────
add("serialized_messages_bytes", messages=[])
add("serialized_messages_bytes", messages=None)
add("serialized_messages_bytes", messages={})
add("serialized_messages_bytes", messages=[{"role": "user", "content": "hi"}])
add("serialized_messages_bytes",
    messages=[{"role": "assistant", "content": "h\u00e9llo\n\t\"q\"\u0001"}])
add("serialized_messages_bytes", messages=[1, 2, 3])
add("serialized_messages_bytes", messages=[{"a": [True, False, None]}])
add("serialized_messages_bytes", messages="x")
add("serialized_messages_bytes",
    messages=[{"k": "\u2028\u00e9\ud83d\ude00", "n": -12}])

# ── looks_like_image_content_rejection ──────────────────────────────────────
for eb in ("Only 'text' content type is supported",
           "Model does not support image input", "everything is fine", None, "",
           "UNKNOWN VARIANT `IMAGE_URL`, EXPECTED `TEXT`",
           "is not a multimodal model", 0, "unexpected item type in content",
           "image_url'. Expected one of", "no endpoints found that support image input"):
    add("looks_like_image_content_rejection", error_body=eb)

# ── predicted_decode_tok_s ──────────────────────────────────────────────────
def entry(**kw):
    base = {"id": "qwen3.8-27b", "mtp": True, "mtp_draft_depth": 2,
            "decode_fraction": 1.0}
    base.update(kw)
    return base

def variant(size=1000, quant="UD-Q4_K_M"):
    return {"quant": quant, "files": [{"path": "m.gguf", "size_bytes": size}]}

def budget(**kw):
    base = {"uma": True, "platform": "win32",
            "gpu_name": "NVIDIA RTX Spark N1X", "gpu_pci_id": None}
    base.update(kw)
    return base

add("predicted_decode_tok_s", entry=entry(), variant=variant(), budget=budget())
add("predicted_decode_tok_s", entry=entry(), variant=variant(), budget=budget(),
    spilled=True)
add("predicted_decode_tok_s", entry=entry(), variant=variant(),
    budget=budget(gpu_name="Other (desc)", gpu_pci_id=0x2E0010DE))
add("predicted_decode_tok_s", entry=entry(), variant=variant(),
    budget=budget(gpu_pci_id=0x123410DE))
add("predicted_decode_tok_s", entry=entry(), variant=variant(), budget=budget(uma=False))
add("predicted_decode_tok_s", entry=entry(mtp=False), variant=variant(), budget=budget())
add("predicted_decode_tok_s", entry=entry(decode_fraction=0.0), variant=variant(),
    budget=budget())
add("predicted_decode_tok_s", entry=entry(),
    variant=variant(), budget=budget(gpu_name="NVIDIA RTX Spark N1X (12GB)"))
add("predicted_decode_tok_s", entry=entry(mtp_draft_depth=3), variant=variant(),
    budget=budget())
add("predicted_decode_tok_s", entry=entry(), variant=variant(size=0), budget=budget(),
    backend="cuda")

# ── download_bytes ──────────────────────────────────────────────────────────
def dentry(**kw):
    base = {"id": "e", "mmproj": None, "draft": None, "mtp_head": None}
    base.update(kw)
    return base

def af(size):
    return {"path": "a.gguf", "size_bytes": size}

add("download_bytes", entry=dentry(),
    variant={"quant": "q", "files": [af(100), af(200)]})
add("download_bytes", entry=dentry(mmproj=af(50)),
    variant={"quant": "q", "files": [af(100), af(200)]})
add("download_bytes", entry=dentry(mmproj=af(50), draft=af(70), mtp_head=af(90)),
    variant={"quant": "q", "files": [af(100), af(200)]})
add("download_bytes", entry=dentry(), variant={"quant": "q", "files": []})
add("download_bytes", entry=dentry(mtp_head=af(5)), variant={"quant": "q", "files": []})
add("download_bytes", entry=dentry(draft=af(11)),
    variant={"quant": "q", "files": [af(1)]})

# ── weights_bytes ───────────────────────────────────────────────────────────
add("weights_bytes", variant={"quant": "q", "files": [af(1000), af(2000)], "lazy_bytes": 300})
add("weights_bytes", variant={"quant": "q", "files": [af(1000), af(2000)], "lazy_bytes": 0})
add("weights_bytes", variant={"quant": "q", "files": [], "lazy_bytes": 0})
add("weights_bytes", variant={"quant": "q", "files": [af(100)], "lazy_bytes": 500})
add("weights_bytes", variant={"quant": "q", "files": [af(42)]})

# ── ub_logits_bytes ─────────────────────────────────────────────────────────
add("ub_logits_bytes", n_vocab=tag("int", 151936), mtp_capable=False)
add("ub_logits_bytes", n_vocab=tag("int", 151936), mtp_capable=True)
add("ub_logits_bytes", n_vocab=tag("int", 151936), mtp_capable=True, mtp_prefill=True)
add("ub_logits_bytes", n_vocab=tag("int", -5), mtp_capable=False)
add("ub_logits_bytes", n_vocab=tag("float", 151936.9), mtp_capable=False)
add("ub_logits_bytes", n_vocab=tag("bool", True), mtp_capable=False)
add("ub_logits_bytes", n_vocab=tag("float", -2.5), mtp_capable=True, mtp_prefill=True)
add("ub_logits_bytes", n_vocab=tag("int", 0), mtp_capable=True, mtp_prefill=True)

# ── is_routable_provider ────────────────────────────────────────────────────
add("is_routable_provider", provider=None)
add("is_routable_provider", provider="  ")
add("is_routable_provider", provider="AUTO")
add("is_routable_provider", provider="Custom")
add("is_routable_provider", provider=" custom ")
add("is_routable_provider", provider="nous",
    io={"load_config": {"providers": {"nous": {}}},
        "get_compatible_custom_providers": {},
        "resolve_provider_full": {"name": "nous"}})
add("is_routable_provider", provider="nous",
    io={"load_config": {}, "get_compatible_custom_providers": {},
        "resolve_provider_full": None})
add("is_routable_provider", provider="nous",
    io={"load_config": {}, "get_compatible_custom_providers": {},
        "resolve_provider_full": raise_("RuntimeError", "resolve")})
add("is_routable_provider", provider="nous",
    io={"load_config": raise_("ValueError", "cfg")})
add("is_routable_provider", provider="nous",
    io={"load_config": raise_("SystemExit", "cfg")})
add("is_routable_provider", provider="nous",
    io={"load_config": {}, "get_compatible_custom_providers": {},
        "resolve_provider_full": raise_("ImportError", "no providers module")})
add("is_routable_provider", provider="nous",
    io={"load_config": {}, "get_compatible_custom_providers": raise_("SystemExit", "cc")})
add("is_routable_provider", provider="nous", io={"load_config": None})

# ── is_external_process_provider ────────────────────────────────────────────
add("is_external_process_provider", provider=None)
add("is_external_process_provider", provider="")
add("is_external_process_provider", provider=" Claude_CLI ",
    io={"registry_get": {"auth_type": "external_process"}})
add("is_external_process_provider", provider="claude_cli",
    io={"registry_get": {"auth_type": "api_key"}})
add("is_external_process_provider", provider="claude_cli",
    io={"registry_get": None, "get_provider_profile": {"auth_type": "external_process"}})
add("is_external_process_provider", provider="claude_cli",
    io={"registry_get": None, "get_provider_profile": None})
add("is_external_process_provider", provider="claude_cli",
    io={"registry_get": None, "get_provider_profile": {}})
add("is_external_process_provider", provider="claude_cli",
    io={"registry_get": raise_("RuntimeError", "reg"),
        "get_provider_profile": {"auth_type": "external_process"}})
add("is_external_process_provider", provider="claude_cli",
    io={"registry_get": raise_("SystemExit", "reg")})
add("is_external_process_provider", provider="claude_cli",
    io={"registry_get": None, "get_provider_profile": raise_("ImportError", "no providers")})
add("is_external_process_provider", provider="claude_cli",
    io={"registry_get": {"no_auth": 1},
        "get_provider_profile": {"auth_type": "external_process"}})
add("is_external_process_provider", provider="claude_cli",
    io={"registry_get": {"auth_type": None}})


def main():
    out = REPO / "tests" / "fixtures" / "tail_revive2.jsonl"
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    assert all(v >= 5 for v in counts.values()), counts
    print(f"wrote {out}: {len(CASES)} cases across {len(counts)} funcs")
    for k, v in sorted(counts.items()):
        print(f"  {k}: {v}")


if __name__ == "__main__":
    main()
