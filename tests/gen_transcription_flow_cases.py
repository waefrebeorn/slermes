#!/usr/bin/env python3
"""Generator for tests/fixtures/transcription_flow.jsonl.

Emits >=5 cases per ported function of the UNCLAIMED transcription surface
(tools/transcription_common.py + transcription_chunking.py +
transcription_whisper_cpp.py, ported as src/tools/transcription_flow.c).
Each case is {"func": <name>, "in": {...args..., "state": {"verified_files":
{path: [digest,ino,size,mtime_ns,ctime_ns]}}, "io": {seam: script},
"pm_ok": bool, "pm_downloader_ok": bool}}.

Script shapes (shared with the C universal seam tr_call and the oracle):
  absent key            -> None
  {"$seq": [...]}       -> per-call queue (exhaustion raises RuntimeError)
  {"__raise__": cls, "msg": m} -> raises a REAL exception of that class
  any other JSON value  -> reused for every call (arrays are LIST results)

Domain (documented in the port header): ASCII str()/repr() operands; positive
CalledProcessError returncodes; 2-element array limits (null/int for the
unpack/index TypeError branches); integral-int or non-integral float numbers in
type-name positions; format positions avoid exact binary rounding ties; seam
attribute carriers present; TemporaryDirectory scripts non-empty strings;
bytes inputs as {"__bytes__": hex}; POSIX paths. BaseException-trio scripted
raises exercise the passthrough past `except Exception`; STTResponseError
(custom ValueError subclass) exercises the family catch in _config_number.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "transcription_flow.jsonl")
CASES = []

MB = 1024 * 1024
HOME = "/h"
MD = HOME + "/cache/whisper.cpp"
DEST1 = MD + "/ggml-base.bin"
DEST2 = MD + "/ggml-silero-v6.2.0.bin"
URL1 = ("https://huggingface.co/ggerganov/whisper.cpp/resolve/"
        "5359861c739e955e79d9a303bcbc70fb988958b1/ggml-base.bin")
URL2 = ("https://huggingface.co/ggml-org/whisper-vad/resolve/"
        "9ffd54a1e1ee413ddf265af9913beaf518d1639b/ggml-silero-v6.2.0.bin")
SHA1 = "60ed5bc3dd14eea856493d334349b405782ddcaf0028d4b5df4088345fba2efe"
SHA2 = "2aa269b785eeb53a82983a20501ddf7c1d9c48e33ab63a41391ac6c9f7fb6987"
SIG1 = [SHA1, 11, 147456000, 1700000000000000, 1700000000000001]
SIG2 = [SHA2, 12, 2048000, 1700000000000002, 1700000000000003]
STAT1 = {"st_ino": 11, "st_size": 147456000, "st_mtime_ns": 1700000000000000,
         "st_ctime_ns": 1700000000000001}
STAT2 = {"st_ino": 12, "st_size": 2048000, "st_mtime_ns": 1700000000000002,
         "st_ctime_ns": 1700000000000003}


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def io(**scripts):
    return scripts


def seq(*vals):
    return {"$seq": list(vals)}


def raise_(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def src1():
    return {"url": URL1, "dest": DEST1, "sha256": SHA1}


def src2():
    return {"url": URL2, "dest": DEST2, "sha256": SHA2}


def vf(**entries):
    return {"verified_files": entries}


# ── _error_result ───────────────────────────────────────────────────────────
case("_error_result", error="boom")
case("_error_result", error="x", extra_kw={"provider": "groq"})
case("_error_result", error="x", extra_kw={"no_speech": True, "provider": "openai"})
case("_error_result", error="x", extra_kw={"success": True})  # overwrite keeps position
case("_error_result", error="x", extra_kw={"error": "y"})  # binding collision
case("_error_result", error="")

# ── normalize_xai_stt_model ─────────────────────────────────────────────────
case("normalize_xai_stt_model", model="grok-voice-transcribe-1.0")
case("normalize_xai_stt_model", model=None, io=io(**{"os.getenv": "custom-model"}))
case("normalize_xai_stt_model", model="", io=io(**{"os.getenv": ""}))
case("normalize_xai_stt_model", model="grok-stt", io=io(**{"os.getenv": "  pinned  "}))
case("normalize_xai_stt_model", model="  grok-stt  ", io=io(**{"os.getenv": None}))
case("normalize_xai_stt_model", model="grok-stt")  # absent getenv -> None -> .strip raises
case("normalize_xai_stt_model", model=0, io=io(**{"os.getenv": "m1"}))
case("normalize_xai_stt_model", model="grok-stt", io=io(**{"os.getenv": 5}))
case("normalize_xai_stt_model", model="  ", io=io(**{"os.getenv": "x"}))

# ── _ok_result ──────────────────────────────────────────────────────────────
case("_ok_result", transcript="hello world", provider="openai")
case("_ok_result", transcript="", provider="")
case("_ok_result", transcript="one two", provider="groq")
case("_ok_result", transcript="x", provider="local_command")
case("_ok_result", transcript="  spaced  ", provider="xai")

# ── _lazy_ensure_quietly ────────────────────────────────────────────────────
case("_lazy_ensure_quietly", extra="faster-whisper", io=io(**{"pm.ensure_import": None}))
case("_lazy_ensure_quietly", extra="silero", io=io(**{"pm.ensure_import": True}))
case("_lazy_ensure_quietly", extra="x", io=io(**{"pm.ensure_import": raise_("RuntimeError", "no net")}))
case("_lazy_ensure_quietly", extra="x", io=io(**{"pm.ensure_import": raise_("ValueError", "bad")}))
case("_lazy_ensure_quietly", extra="x", io=io(**{"pm.ensure_import": raise_("KeyboardInterrupt")}))
case("_lazy_ensure_quietly", extra="x", io=io(**{"pm.ensure_import": raise_("SystemExit", "1")}))
case("_lazy_ensure_quietly", extra="x", pm_ok=False)  # import halt swallowed
case("_lazy_ensure_quietly", extra="x")  # absent script -> None

# ── _process_error_detail ───────────────────────────────────────────────────
case("_process_error_detail", exc={"returncode": 1, "cmd": "ffmpeg", "stderr": "  err detail  ", "stdout": "out"})
case("_process_error_detail", exc={"returncode": 1, "cmd": "ffmpeg", "stderr": "", "stdout": " out "})
case("_process_error_detail", exc={"returncode": 1, "cmd": "ffmpeg"})
case("_process_error_detail", exc={"returncode": 1, "cmd": "ffmpeg", "stderr": {"__bytes__": "6f6bff"}})
case("_process_error_detail", exc={"returncode": 1, "cmd": "ffmpeg", "stderr": {"__bytes__": "2020"}, "stdout": "s"})
case("_process_error_detail", exc={"returncode": 2, "cmd": ["ffmpeg", "-i", "x"]})
case("_process_error_detail", exc={"returncode": 1, "cmd": "ffmpeg", "stderr": 0, "stdout": 5.5})
case("_process_error_detail", exc={"returncode": 1, "cmd": "ffmpeg", "stderr": {"__bytes__": "f09080"}})
case("_process_error_detail", exc=None)  # None.stderr raises

# ── _log_prompt_unsupported ─────────────────────────────────────────────────
case("_log_prompt_unsupported", label="groq", io=io(**{"logger.debug": None}))
case("_log_prompt_unsupported", label="", io=io(**{"logger.debug": None}))
case("_log_prompt_unsupported", label="xai", io=io(**{"logger.debug": None}))
case("_log_prompt_unsupported", label="openai", io=io(**{"logger.debug": None}))
case("_log_prompt_unsupported", label="mistral", io=io(**{"logger.debug": raise_("RuntimeError", "log down")}))

# ── _config_number ──────────────────────────────────────────────────────────
case("_config_number", cfg={"timeout": "30"}, key="timeout", default=60, io=io(cast=30.0))
case("_config_number", cfg={}, key="timeout", default=60.0, io=io(cast=60.0))
case("_config_number", cfg={"timeout": None}, key="timeout", default=60, io=io(cast=None))
case("_config_number", cfg={"timeout": "x"}, key="timeout", default=60, io=io(cast=raise_("ValueError", "bad float")))
case("_config_number", cfg={"timeout": []}, key="timeout", default=60, io=io(cast=raise_("TypeError", "bad type")))
case("_config_number", cfg={"timeout": "x"}, key="timeout", default=60,
     io=io(cast=raise_("STTResponseError", "structured error")))  # ValueError family -> caught
case("_config_number", cfg={"timeout": "x"}, key="timeout", default=60,
     io=io(cast=raise_("KeyboardInterrupt")))  # BaseException passthrough
case("_config_number", cfg={"timeout": "x"}, key="timeout", default=60,
     io=io(cast=raise_("KeyError", "k")))  # NOT caught
case("_config_number", cfg=None, key="timeout", default=60)
case("_config_number", cfg={"t": 1}, key="t", default=0, cast_seam="mycast", io=io(mycast=5))
case("_config_number", cfg={"timeout": "abc"}, key="timeout", default=60.5,
     io=io(cast=raise_("ValueError", "could not convert string to float: 'abc'")))

# ── upload_limit ────────────────────────────────────────────────────────────
case("upload_limit", provider="openai", model="whisper-1")
case("upload_limit", provider="openai", model="gpt-4o-transcribe")
case("upload_limit", provider="openai", model=None)
case("upload_limit", provider="openai", model="gpt-transcribe")
case("upload_limit", provider="groq", model=None)
case("upload_limit", provider="mistral", model="voxtral-mini-latest")
case("upload_limit", provider="xai", model="")
case("upload_limit", provider="elevenlabs", model=None)
case("upload_limit", provider="deepinfra", model=None)
case("upload_limit", provider="local_command", model=None)

# ── _fits ───────────────────────────────────────────────────────────────────
LIM = [25 * MB, 450.0]
case("_fits", path="/a/x.wav", limit=LIM, duration=100, io=io(**{"os.path.getsize": 1000}))
case("_fits", path="/a/x.wav", limit=LIM, duration=100, io=io(**{"os.path.getsize": 25 * MB + 1}))
case("_fits", path="/a/x.wav", limit=LIM, duration=450, io=io(**{"os.path.getsize": 1000}))
case("_fits", path="/a/x.wav", limit=LIM, duration=450.5, io=io(**{"os.path.getsize": 1000}))
case("_fits", path="/a/x.wav", limit=LIM, duration=None, io=io(**{"os.path.getsize": 1000}))
case("_fits", path="/a/x.wav", limit=[25 * MB, None], duration=99999, io=io(**{"os.path.getsize": 1000}))
case("_fits", path="/a/x.wav", limit=None, duration=1)
case("_fits", path="/a/x.wav", limit=[1], duration=1)
case("_fits", path="/a/x.wav", limit=[1, 2, 3], duration=1)
case("_fits", path="/a/x.wav", limit=LIM, duration=1, io=io(**{"os.path.getsize": None}))
case("_fits", path="/a/x.wav", limit=LIM, duration=1,
     io=io(**{"os.path.getsize": raise_("FileNotFoundError", "gone")}))

# ── exceeds_upload_limit ────────────────────────────────────────────────────
case("exceeds_upload_limit", path="/a/x.wav", limit=LIM, io=io(**{"os.path.getsize": 25 * MB + 1}))
case("exceeds_upload_limit", path="/a/x.wav", limit=[25 * MB, None], io=io(**{"os.path.getsize": 1000}))
case("exceeds_upload_limit", path="/a/x.wav", limit=LIM,
     io=io(**{"os.path.getsize": 1000, "_probe_audio_duration": 100}))
case("exceeds_upload_limit", path="/a/x.wav", limit=LIM,
     io=io(**{"os.path.getsize": 1000, "_probe_audio_duration": 500}))
case("exceeds_upload_limit", path="/a/x.wav", limit=LIM,
     io=io(**{"os.path.getsize": 1000, "_probe_audio_duration": None}))
case("exceeds_upload_limit", path="/a/x.wav", limit=None)
case("exceeds_upload_limit", path="/a/x.wav", limit=LIM,
     io=io(**{"os.path.getsize": 1000, "_probe_audio_duration": raise_("RuntimeError", "probe down")}))
case("exceeds_upload_limit", path="/a/x.wav", limit=LIM,
     io=io(**{"os.path.getsize": 1000, "_probe_audio_duration": 450}))

# ── _too_large ──────────────────────────────────────────────────────────────
case("_too_large", path="/a/big.wav", max_bytes=25 * MB, reason="ffmpeg was not found",
     io=io(**{"os.path.getsize": 26 * MB}))
case("_too_large", path="/a/big.wav", max_bytes=500 * MB, reason="its duration could not be read",
     io=io(**{"os.path.getsize": 501 * MB}))
case("_too_large", path="/a/big.wav", max_bytes=5 * 1024 * MB - 1, reason="x",
     io=io(**{"os.path.getsize": 5 * 1024 * MB}))
case("_too_large", path="/a/big.wav", max_bytes=25 * MB, reason="x",
     io=io(**{"os.path.getsize": raise_("OSError", "disk gone")}))
case("_too_large", path="/a/big.wav", max_bytes=25 * MB, reason="",
     io=io(**{"os.path.getsize": MB}))
case("_too_large", path="/a/big.wav", max_bytes=25 * MB, reason="r",
     io=io(**{"os.path.getsize": 25 * MB + MB // 2}))  # 25.5 exact at .1f

# ── _silence_midpoints ──────────────────────────────────────────────────────
def runq(stderr):
    return {"stderr": stderr}


case("_silence_midpoints", ffmpeg="ffmpeg", path="/a/x.wav",
     io=io(_run_quiet=runq("silence_start: 1.0\nsilence_end: 2.0\n")))
case("_silence_midpoints", ffmpeg="ffmpeg", path="/a/x.wav",
     io=io(_run_quiet=runq("silence_start: -0.5\nsilence_end: 1.2 | silence_duration: 1.7\n")))
case("_silence_midpoints", ffmpeg="ffmpeg", path="/a/x.wav",
     io=io(_run_quiet=runq("silence_start: 10\nsilence_end: 12.5\nsilence_start: 20\nsilence_end: 22\nsilence_start: 99.9\n")))
case("_silence_midpoints", ffmpeg="ffmpeg", path="/a/x.wav",
     io=io(_run_quiet=raise_("FileNotFoundError", "no ffmpeg"), **{"logger.debug": None}))
case("_silence_midpoints", ffmpeg="ffmpeg", path="/a/x.wav",
     io=io(_run_quiet=raise_("TimeoutExpired", "ffmpeg"), **{"logger.debug": None}))
case("_silence_midpoints", ffmpeg="ffmpeg", path="/a/x.wav",
     io=io(_run_quiet=raise_("RuntimeError", "not caught")))
case("_silence_midpoints", ffmpeg="ffmpeg", path="/a/x.wav",
     io=io(_run_quiet=runq("silence_start: 1.2.3\nsilence_end: 2.0\n")))
case("_silence_midpoints", ffmpeg="ffmpeg", path="/a/x.wav", io=io(_run_quiet=runq("")))
case("_silence_midpoints", ffmpeg="ffmpeg", path="/a/x.wav")  # absent -> None.stderr raises
case("_silence_midpoints", ffmpeg="ffmpeg", path="/a/x.wav",
     io=io(_run_quiet=raise_("PermissionError", "denied"),
           **{"logger.debug": raise_("RuntimeError", "log down")}))
case("_silence_midpoints", ffmpeg="ffmpeg", path="/a/x.wav",
     io=io(_run_quiet=runq("silence_end: 5.0\n")))
case("_silence_midpoints", ffmpeg="ffmpeg", path="/a/x.wav",
     io=io(_run_quiet=runq("silence_start: .5\nsilence_end: 1.5\n")))

# ── _cut_points ─────────────────────────────────────────────────────────────
case("_cut_points", duration=1000, target=450, silences=[])
case("_cut_points", duration=1000, target=450, silences=[440, 890])
case("_cut_points", duration=1000, target=450, silences=[300])
case("_cut_points", duration=400, target=450, silences=[100])
case("_cut_points", duration=350, target=100, silences=[90, 195, 300])
case("_cut_points", duration=900, target=450, silences=[450])  # exactly at ideal
case("_cut_points", duration=900, target=450, silences=[0])  # == position excluded
case("_cut_points", duration=0, target=450, silences=[])

# ── _split ──────────────────────────────────────────────────────────────────
case("_split", ffmpeg="ffmpeg", path="/a/x.m4a", work_dir="/tmp/w", cuts=[440.0, 890.5],
     io=io(_run_quiet=None, **{"os.listdir": ["part000.m4a", "part001.m4a", "other.txt"]}))
case("_split", ffmpeg="ffmpeg", path="/a/x.m4a", work_dir="/tmp/w", cuts=[100],
     io=io(_run_quiet=None, **{"os.listdir": ["part002.m4a", "part000.m4a", "part001.m4a"]}))
case("_split", ffmpeg="ffmpeg", path="/a/x.m4a", work_dir="/tmp/w", cuts=[100],
     io=io(_run_quiet=None, **{"os.listdir": []}))
case("_split", ffmpeg="ffmpeg", path="/a/x.m4a", work_dir="/tmp/w", cuts=[100],
     io=io(_run_quiet=raise_("CalledProcessError", "ffmpeg"), **{"os.listdir": []}))
case("_split", ffmpeg="ffmpeg", path="/a/x.m4a", work_dir="/tmp/w", cuts=[100],
     io=io(_run_quiet=None, **{"os.listdir": None}))
case("_split", ffmpeg="ffmpeg", path="/a/x.m4a", work_dir="/tmp/w", cuts=[],
     io=io(_run_quiet=None, **{"os.listdir": ["part000.m4a", "zzz"]}))

# ── transcribe_oversized ────────────────────────────────────────────────────
BIG = "/a/big.wav"
COMPACT = "/tmp/w/c.m4a"


def oversized_io(**over):
    base = {
        "_find_ffmpeg_binary": "ffmpeg",
        "TemporaryDirectory": "/tmp/w",
        "_transcode_audio_for_stt": [COMPACT, None],
        "_probe_audio_duration": 100.0,
        "os.path.getsize": 1000,
        "dispatch": {"success": True, "transcript": "hello", "provider": "openai"},
        "logger.info": None,
    }
    base.update(over)
    return base


case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**oversized_io(_find_ffmpeg_binary=None, **{"os.path.getsize": 26 * MB})))
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**oversized_io(_find_ffmpeg_binary="", **{"os.path.getsize": 26 * MB})))
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**oversized_io(_transcode_audio_for_stt=[None, "encode failed"],
                          **{"os.path.getsize": 26 * MB})))
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**oversized_io(_transcode_audio_for_stt=[None, None],
                          **{"os.path.getsize": 26 * MB})))
case("transcribe_oversized", path=BIG, limit=LIM, io=io(**oversized_io()))  # fits -> dispatch
# split path: 1000s, 25MB compact -> target min(900, 405) = 405; silence mid 395
SPLIT_IO = oversized_io(
    _probe_audio_duration=1000.0,
    **{"os.path.getsize": seq(25 * MB, 25 * MB)},
    _run_quiet=runq("silence_start: 390.0\nsilence_end: 400.0\n"),
    **{"os.listdir": ["part000.m4a", "part001.m4a", "part002.m4a"]},
    dispatch=seq({"success": True, "transcript": "one", "provider": "openai"},
                 {"success": True, "transcript": "two"},
                 {"no_speech": True}),
    is_whisper_hallucination=False,
)
case("transcribe_oversized", path=BIG, limit=LIM, io=io(**SPLIT_IO))
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**dict(SPLIT_IO, dispatch=seq(
         {"success": True, "transcript": "one", "provider": "groq"},
         {"success": False, "error": "boom"}))))
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**dict(SPLIT_IO, dispatch=seq(
         {"success": True, "transcript": "one"},
         {"success": False}))))  # error absent -> 'unknown error'
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**dict(SPLIT_IO, dispatch=seq({"no_speech": True}, {"no_speech": True}, {"no_speech": True}))))
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**dict(SPLIT_IO, dispatch=seq(
         {"success": True, "transcript": "thank you", "provider": "openai"},
         {"success": True, "transcript": "real text"},
         {"no_speech": True}),
         is_whisper_hallucination=seq(True, False))))
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**dict(SPLIT_IO, dispatch=seq(
         {"success": True, "transcript": "one"},
         raise_("RuntimeError", "api down")))))
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**dict(SPLIT_IO, dispatch=seq(raise_("SystemExit", "1")))))
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**dict(SPLIT_IO, dispatch=seq(None))))  # None.get raises
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**oversized_io(_probe_audio_duration=None, **{"os.path.getsize": 26 * MB})))
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**oversized_io(_probe_audio_duration=0, **{"os.path.getsize": 26 * MB})))
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**oversized_io(_transcode_audio_for_stt=raise_("ValueError", "bad audio"))))
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**oversized_io(TemporaryDirectory=raise_("OSError", "no tmp"))))
# 1 part, error present-null -> 'None' in the segment message
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**oversized_io(
         _probe_audio_duration=500.0,
         **{"os.path.getsize": seq(25 * MB, 25 * MB)},
         _run_quiet=runq(""),
         **{"os.listdir": ["part000.m4a"]},
         dispatch=seq({"success": False, "error": None}))))
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**dict(SPLIT_IO, dispatch=seq(
         {"success": True, "transcript": "   "},
         {"success": True, "transcript": ""},
         {"no_speech": True}))))  # all stripped empty -> empty transcript error
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**dict(SPLIT_IO, dispatch=seq(
         {"success": True, "transcript": "one", "provider": "groq"},
         {"success": True, "transcript": "two", "provider": None},
         {"success": True, "transcript": "three"}))))  # provider persists
case("transcribe_oversized", path=BIG, limit=LIM,
     io=io(**dict(SPLIT_IO, dispatch=seq({"success": True, "transcript": "one"}))))  # $seq exhaustion
case("transcribe_oversized", path=BIG, limit=[25 * MB, None],
     io=io(**oversized_io(_probe_audio_duration=99999.0)))  # no duration cap -> fits

# ── _model_dir ──────────────────────────────────────────────────────────────
case("_model_dir", io=io(get_hermes_home="/h"))
case("_model_dir", io=io(get_hermes_home="/h/"))
case("_model_dir", io=io(get_hermes_home="/"))
case("_model_dir", io=io(get_hermes_home="//h"))
case("_model_dir", io=io(get_hermes_home=""))
case("_model_dir", io=io(get_hermes_home=None))
case("_model_dir", io=io(get_hermes_home=raise_("RuntimeError", "no home")))

# ── _file_signature ─────────────────────────────────────────────────────────
case("_file_signature", path=DEST1, digest=SHA1, io=io(stat=STAT1))
case("_file_signature", path=DEST1, digest=SHA1, io=io(stat=raise_("FileNotFoundError", "missing")))
case("_file_signature", path=DEST1, digest=SHA1, io=io(stat=raise_("PermissionError", "denied")))
case("_file_signature", path=DEST1, digest=SHA1, io=io(stat=None))
case("_file_signature", path="/h/f/", digest="d", io=io(stat=STAT2))  # normalized seam arg
case("_file_signature", path=DEST2, digest=SHA2, io=io(stat=STAT2))

# ── whisper_cpp_command ─────────────────────────────────────────────────────
def wcmd_io(binary, home=HOME):
    return io(**{"pm.installed_package": {"binary": binary}, "get_hermes_home": home})


case("whisper_cpp_command", io=io(**{"pm.installed_package": None}))
case("whisper_cpp_command", io=wcmd_io("/usr/bin/whisper-cli"))
case("whisper_cpp_command", io=wcmd_io("a b"))
case("whisper_cpp_command", io=wcmd_io(""))
case("whisper_cpp_command", pm_ok=False)
case("whisper_cpp_command", io=io(**{"pm.installed_package": raise_("RuntimeError", "pm down")}))
case("whisper_cpp_command", io=wcmd_io("it's"))
case("whisper_cpp_command")  # absent installed script -> None -> None

# ── ensure_whisper_cpp_models ───────────────────────────────────────────────
case("ensure_whisper_cpp_models", model_name="tiny")
case("ensure_whisper_cpp_models", model_name="")
# fully verified cache: no pending, no download
case("ensure_whisper_cpp_models", model_name="base",
     state=vf(**{DEST1: SIG1, DEST2: SIG2}),
     io=io(get_hermes_home=HOME, Source=seq(src1(), src2()), stat=seq(STAT1, STAT2)))
# nothing cached: both pending, download, then both verified
case("ensure_whisper_cpp_models", model_name="base",
     io=io(get_hermes_home=HOME, Source=seq(src1(), src2()),
           stat=seq(raise_("FileNotFoundError", "m1"), raise_("FileNotFoundError", "m2"),
                    STAT1, STAT2),
           Download={}, **{"Download.run": None, "logger.info": None}))
# one verified, one missing -> only the missing one is pending
case("ensure_whisper_cpp_models", model_name="base",
     state=vf(**{DEST1: SIG1}),
     io=io(get_hermes_home=HOME, Source=seq(src1(), src2()),
           stat=seq(STAT1, raise_("FileNotFoundError", "m2"), STAT2),
           Download={}, **{"Download.run": None, "logger.info": None}))
# stale signature (size changed) -> re-download
case("ensure_whisper_cpp_models", model_name="base",
     state=vf(**{DEST1: [SHA1, 11, 999, 1700000000000000, 1700000000000001], DEST2: SIG2}),
     io=io(get_hermes_home=HOME, Source=seq(src1(), src2()),
           stat=seq(STAT1, STAT2, STAT1),
           Download={}, **{"Download.run": None, "logger.info": None}))
case("ensure_whisper_cpp_models", model_name="base", pm_downloader_ok=False)
case("ensure_whisper_cpp_models", model_name="base",
     io=io(get_hermes_home=HOME, Source=seq(src1(), src2()),
           stat=seq(raise_("FileNotFoundError", "m1"), raise_("FileNotFoundError", "m2")),
           Download={}, **{"Download.run": raise_("RuntimeError", "net down"), "logger.info": None}))
case("ensure_whisper_cpp_models", model_name="base",
     io=io(get_hermes_home=HOME, Source=seq(raise_("ValueError", "bad source"))))
case("ensure_whisper_cpp_models", model_name="base",
     io=io(get_hermes_home=HOME, Source=seq(None, src2())))  # None.dest raises
case("ensure_whisper_cpp_models", model_name="base",
     io=io(get_hermes_home=HOME, Source=seq(src1(), src2()),
           stat=seq(raise_("FileNotFoundError", "m1"), raise_("FileNotFoundError", "m2")),
           Download=None, **{"logger.info": None}))  # None.run raises
case("ensure_whisper_cpp_models", model_name="base",
     io=io(get_hermes_home=HOME, Source=seq(src1(), src2()),
           stat=seq(raise_("PermissionError", "denied"))))
# post-run stat missing -> signature not stored
case("ensure_whisper_cpp_models", model_name="base",
     io=io(get_hermes_home=HOME, Source=seq(src1(), src2()),
           stat=seq(raise_("FileNotFoundError", "m1"), raise_("FileNotFoundError", "m2"),
                    raise_("FileNotFoundError", "still"), STAT2),
           Download={}, **{"Download.run": None, "logger.info": None}))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    print(f"wrote {len(CASES)} cases -> {OUT}")
    print(f"funcs: {len(counts)}; per-func min: {min(counts.values())}")
    if thin:
        print(f"WARNING: funcs with <5 cases: {thin}")
        raise SystemExit(1)


if __name__ == "__main__":
    main()
