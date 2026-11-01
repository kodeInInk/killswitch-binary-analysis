#include <signal.h>
#include <stdio.h>
#include <unistd.h>

void nope(int sig) {
    puts("nope, still working");
    fflush(stdout);
}

int main() {
    signal(SIGTERM, nope);
    for (;;) pause();
}
