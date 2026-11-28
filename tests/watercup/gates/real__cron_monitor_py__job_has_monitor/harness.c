/* gate: job_has_monitor -> real port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern int cron_monitor_u_job_has_monitor(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("job_has_monitor");
  int r=cron_monitor_u_job_has_monitor(argv[i]); WCUP_VALSTR(r ? "1" : "0");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("job_has_monitor"); } return 0;}
