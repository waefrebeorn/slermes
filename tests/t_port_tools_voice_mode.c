/* AUTO-GENERATED oracle harness for tools_voice_mode (gen_oracle.py). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_tools_voice_mode.c"

static const char *js(const char *s){
  static char b[4][4096]; static int t=0; char *q=b[t]; t=(t+1)&3; *q++='"';
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
  printf("{\"func\":\"voice_mode__audio_available\",\"ret\":0}\n", "voice_mode__audio_available", voice_mode__audio_available());
  printf("{\"func\":\"voice_mode__termux_api_app_installed\",\"ret\":0}\n", "voice_mode__termux_api_app_installed", voice_mode__termux_api_app_installed());
  printf("{\"func\":\"voice_mode__termux_voice_capture_available\",\"ret\":0}\n", "voice_mode__termux_voice_capture_available", voice_mode__termux_voice_capture_available());
  printf("{\"func\":\"voice_mode__pulse_socket_reachable\",\"ret\":0}\n", "voice_mode__pulse_socket_reachable", voice_mode__pulse_socket_reachable());
  return 0;
}
