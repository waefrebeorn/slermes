
#include <stdio.h>
#include <string.h>
#include <time.h>
int main(void){
    static char big[65536];
    const char *s = "ok";
    struct timespec a,b;
    int i, N=1000000;
    clock_gettime(CLOCK_MONOTONIC,&a);
    for(i=0;i<N;i++){ strncpy(big, s, sizeof big); }
    clock_gettime(CLOCK_MONOTONIC,&b);
    double t1=(b.tv_sec-a.tv_sec)+(b.tv_nsec-a.tv_nsec)/1e9;
    clock_gettime(CLOCK_MONOTONIC,&a);
    for(i=0;i<N;i++){ size_t n=strnlen(s,sizeof big-1); memcpy(big,s,n); big[n]=0; }
    clock_gettime(CLOCK_MONOTONIC,&b);
    double t2=(b.tv_sec-a.tv_sec)+(b.tv_nsec-a.tv_nsec)/1e9;
    printf("strncpy(64KB): %.3fs   memcpy-style: %.4fs   speedup %.1fx\n", t1, t2, t1/t2);
    return 0;
}
