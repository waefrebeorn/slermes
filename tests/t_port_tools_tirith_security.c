/* AUTO-GENERATED oracle harness for tools_tirith_security (gen_oracle.py). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tirith.c"

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
  printf("{\"func\":\"_load_security_config\",\"ret\":%s}\n", js(_load_security_config()));
  _reset_spawn_warning_state();
  printf("{\"func\":\"_get_hermes_home\",\"ret\":%s}\n", js(_get_hermes_home()));
  printf("{\"func\":\"_read_failure_reason\",\"ret\":%s}\n", js(_read_failure_reason()));
  printf("{\"func\":\"_is_install_failed_on_disk\",\"ret\":%d}\n", (int)_is_install_failed_on_disk());
  _clear_install_failed();
  printf("{\"func\":\"_detect_target\",\"ret\":%s}\n", js(_detect_target()));
  printf("{\"func\":\"is_platform_supported\",\"ret\":%d}\n", (int)is_platform_supported());
  return 0;
}
