#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PROCESSES 4096
#define STARTUP_TIME 1000000 //microseconds
#define KILL_TIME 1500000

//x86-64 syscall numbers
#define SYS_SIGACTION 13
#define SYS_FORK 57
#define SYS_SETSID 112

enum { POLITE, HARD, GROUP, TREE };
char *strategies[] = {"polite", "kill -9", "group", "tree"};

int is_call(char *line, char *function) {
    return strstr(line, "call") && strstr(line, function);
}

void add(char *found, char *what) {
    if (strstr(found, what))
        return;
    if (found[0] != '\0')
        strcat(found, ", ");
    strcat(found, what);
}

//read the assembly and look for anything that changes how the program dies
void scan(char *program, char *found) {
    char command[256], line[256], *value;
    long eax = 0, edi = 0;
    int raw = 0;

    found[0] = '\0';
    snprintf(command, sizeof command, "objdump -d -M intel %s 2>/dev/null", program);
    FILE *output = popen(command, "r");

    while (fgets(line, sizeof line, output)) {
        if ((value = strstr(line, "mov    eax,")))
            eax = strtol(value + 11, NULL, 16);
        if ((value = strstr(line, "mov    edi,")))
            edi = strtol(value + 11, NULL, 16);

        int syscall = strstr(line, "syscall") != NULL;
        if (syscall)
            raw = 1;

        if (is_call(line, "<signal@plt>") || is_call(line, "<sigaction@plt>") || (syscall && eax == SYS_SIGACTION))
            if (edi == SIGTERM)
                add(found, "catches SIGTERM");
        if (is_call(line, "<fork@plt>") || (syscall && eax == SYS_FORK))
            add(found, "forks");
        if (is_call(line, "<setsid@plt>") || (syscall && eax == SYS_SETSID))
            add(found, "setsid");
        if (is_call(line, "<prctl@plt>") && edi == PR_SET_PDEATHSIG)
            add(found, "leashed");
    }

    if (pclose(output) != 0)
        strcpy(found, "script, nothing to read");
    else if (found[0] == '\0')
        strcpy(found, "nothing risky");
    else if (raw)
        strcat(found, " (raw)");
}

//every process below us, found by following parent ids in /proc
int below_us(int *found) {
    int pids[MAX_PROCESSES], parents[MAX_PROCESSES];
    int total = 0, count = 0;
    DIR *proc = opendir("/proc");
    struct dirent *entry;

    while ((entry = readdir(proc)) && total < MAX_PROCESSES) {
        char path[300], state;
        snprintf(path, sizeof path, "/proc/%s/stat", entry->d_name);
        FILE *stat = fopen(path, "r");
        if (!stat)
            continue;
        //zombies are already dead, they just wait to be collected
        if (fscanf(stat, "%d %*s %c %d", &pids[total], &state, &parents[total]) == 3 && state != 'Z')
            total++;
        fclose(stat);
    }
    closedir(proc);

    for (int j = 0; j < total; j++)
        if (parents[j] == getpid())
            found[count++] = pids[j];

    for (int i = 0; i < count; i++)
        for (int j = 0; j < total; j++)
            if (parents[j] == found[i])
                found[count++] = pids[j];

    return count;
}

int start(char *program) {
    fflush(stdout);
    int pid = fork();
    if (pid == 0) {
        setsid(); //own group, so the group kill only hits the program
        dup2(open("/dev/null", O_WRONLY), STDOUT_FILENO);
        execl(program, program, NULL);
        exit(1);
    }
    return pid;
}

void clean_up() {
    int left[MAX_PROCESSES];
    int count = below_us(left);
    for (int i = 0; i < count; i++)
        kill(left[i], SIGKILL);
    usleep(200000);
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void test(char *program, int strategy) {
    int seen[MAX_PROCESSES], left[MAX_PROCESSES];
    int pid = start(program);

    usleep(STARTUP_TIME);
    int started = below_us(seen);

    switch (strategy) {
    case POLITE:
        kill(pid, SIGTERM);
        break;
    case HARD:
        kill(pid, SIGKILL);
        break;
    case GROUP:
        kill(-pid, SIGKILL); //negative pid = the whole group
        break;
    case TREE:
        for (int i = 0; i < started; i++)
            kill(seen[i], SIGKILL);
        break;
    }

    usleep(KILL_TIME);
    while (waitpid(-1, NULL, WNOHANG) > 0);

    if (below_us(left) == 0)
        printf("%s O  ", strategies[strategy]);
    else
        printf("%s X  ", strategies[strategy]);
    clean_up();
}

int main(int argc, char **argv) {
    //orphans get handed to us instead of init, so we can still count them
    prctl(PR_SET_CHILD_SUBREAPER, 1);

    printf("O = everything stopped, X = something kept running\n");

    for (int i = 1; i < argc; i++) {
        char found[256];
        scan(argv[i], found);
        printf("\n%s: %s\n", basename(argv[i]), found);

        for (int strategy = POLITE; strategy <= TREE; strategy++)
            test(argv[i], strategy);
        printf("\n");
    }
    return 0;
}
