#include <signal.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <unistd.h>

int main() {
    if (fork() == 0) {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        for (;;) { puts("still here 👋"); fflush(stdout); sleep(1); }
    }
    for (;;) pause();
}
