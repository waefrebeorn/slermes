import subprocess
p='/home/wubu/hermes-agent-dev/slermes/src/tools/port_transcription_tools_ports.c'
r=subprocess.run(['grep','-n','trt_is_protected_path|Idle-unload|trt_load_stt_config|trt_transcribe_audio_local_fallback'],capture_output=True,text=True,cwd='/home/wubu/hermes-agent-dev/slermes')
print('STDOUT:',r.stdout)
print('STDERR:',r.stderr)
