/* AUTO-GENERATED oracle harness for tools_tool_output_limits (gen_oracle.py). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_tools_tool_output_limits.c"

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
  cli_tools_tool_output_limits__reset_tool_output_limits_cache();
  printf("{\"func\":\"cli_tools_tool_output_limits_get_max_bytes\",\"ret\":0}\n", "cli_tools_tool_output_limits_get_max_bytes", cli_tools_tool_output_limits_get_max_bytes());
  printf("{\"func\":\"cli_tools_tool_output_limits_get_max_lines\",\"ret\":0}\n", "cli_tools_tool_output_limits_get_max_lines", cli_tools_tool_output_limits_get_max_lines());
  printf("{\"func\":\"cli_tools_tool_output_limits_get_max_line_length\",\"ret\":0}\n", "cli_tools_tool_output_limits_get_max_line_length", cli_tools_tool_output_limits_get_max_line_length());
  return 0;
}
