#include <stdio.h>
#include <unistd.h>

int main() {
    if (fork() == 0)
        for (;;) { puts("still here 👋"); fflush(stdout); sleep(1); }
    for (;;) pause();
}
