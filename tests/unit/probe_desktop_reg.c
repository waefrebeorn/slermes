/* Isolation harness: does registering the desktop_ui tools crash? */
#include <stdio.h>
void registry_init_desktop_ui(void);
int main(void) {
    printf("before\n"); fflush(stdout);
    registry_init_desktop_ui();
    printf("after\n");
    return 0;
}
