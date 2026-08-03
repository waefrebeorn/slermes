/* AUTO-GENERATED oracle harness for tools_tts_tool (gen_oracle.py). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_tts_tool.c"

static const char *js(const char *s){
  static char b[4][4096]; static int t=0; t=(t+1)&3; char *q=b[t]; *q++='"';
  if(s) for(const char *p=s;*p&&(q-b[t])<4000;p++){unsigned char c=*p;
    if(c=='"'||c=='\\'){*q++='\\';*q++=c;}
    else if(c=='\n'){*q++='\\';*q++='n';}
    else if(c=='\t'){*q++='\\';*q++='t';}
    else if(c<' '){*q++='\\';*q++='u';*q++='0';*q++='0';*q++=(c>15?'1':'0');*q++=(c%16<10?c%16+'0':c%16-10+'a');}
    else *q++=c;}
  *q++='"'; *q=0; return b[t];
}
int main(void){
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("{\"func\":\"tts_tool_import_edge_tts\",\"ret\":0}\n", "tts_tool_import_edge_tts", (int)tts_tool_import_edge_tts());
  printf("{\"func\":\"tts_tool_import_elevenlabs\",\"ret\":0}\n", "tts_tool_import_elevenlabs", (int)tts_tool_import_elevenlabs());
  printf("{\"func\":\"tts_tool_import_openai_client\",\"ret\":0}\n", "tts_tool_import_openai_client", (int)tts_tool_import_openai_client());
  printf("{\"func\":\"tts_tool_import_mistral_client\",\"ret\":0}\n", "tts_tool_import_mistral_client", (int)tts_tool_import_mistral_client());
  printf("{\"func\":\"tts_tool_import_sounddevice\",\"ret\":0}\n", "tts_tool_import_sounddevice", (int)tts_tool_import_sounddevice());
  printf("{\"func\":\"tts_tool_import_kittentts\",\"ret\":0}\n", "tts_tool_import_kittentts", (int)tts_tool_import_kittentts());
  printf("{\"func\":\"tts_tool_import_piper\",\"ret\":0}\n", "tts_tool_import_piper", (int)tts_tool_import_piper());
  printf("{\"func\":\"tts_tool_has_openai_audio_backend\",\"ret\":0}\n", "tts_tool_has_openai_audio_backend", (int)tts_tool_has_openai_audio_backend());
  return 0;
}
