#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <signal.h>
#endif

#define N_SERVERS 3 /* Number of servers in the example cluster */

static int ensureDir(const char *dir)
{
    int rv;
    struct stat sb;
    rv = stat(dir, &sb);
    if (rv == -1) {
        if (errno == ENOENT) {
            rv = mkdir(dir, 0700);
            if (rv != 0) {
                printf("error: create directory '%s': %s", dir,
                       strerror(errno));
                return 1;
            }
        } else {
            printf("error: stat directory '%s': %s", dir, strerror(errno));
            return 1;
        }
    } else {
        if ((sb.st_mode & S_IFMT) != S_IFDIR) {
            printf("error: path '%s' is not a directory", dir);
            return 1;
        }
    }
    return 0;
}
static void forkServer(const char *topLevelDir, unsigned i, pid_t *pid)
{
    *pid = fork();
    if (*pid == 0) {
        char *dir = malloc(strlen(topLevelDir) + strlen("/D") + 1);
        char *id = malloc(N_SERVERS / 10 + 2);
        char *argv[] = {"./example/server", dir, id, NULL};
        char *envp[] = {NULL};
        int rv;
        sprintf(dir, "%s/%u", topLevelDir, i + 1);
        rv = ensureDir(dir);
        if (rv != 0) {
            abort();
        }
        sprintf(id, "%u", i + 1);
        execve("./example/server", argv, envp);
    }
}

int main(int argc, char *argv[])
{
    const char *topLevelDir = "/tmp/raft";
    struct timespec now;
    pid_t pids[N_SERVERS];
    unsigned i;
    int rv;

    if (argc > 2) {
        printf("usage: example-cluster [<dir>]\n");
        return 1;
    }

    if (argc == 2) {
        topLevelDir = argv[1];
    }

    /* Make sure the top level directory exists. */
    rv = ensureDir(topLevelDir);
    if (rv != 0) {
        return rv;
    }

    /* Spawn the cluster nodes */
    for (i = 0; i < N_SERVERS; i++) {
        forkServer(topLevelDir, i, &pids[i]);
    }

    /* Seed the random generator */
    timespec_get(&now, TIME_UTC);
    srandom((unsigned)(now.tv_nsec ^ now.tv_sec));

    while (1) {
        struct timespec interval;
        int status;

        /* Sleep a little bit. */
        interval.tv_sec = 1 + random() % 15;
        interval.tv_nsec = 0;

        rv = nanosleep(&interval, NULL);
        if (rv != 0) {
            printf("error: sleep: %s", strerror(errno));
        }

        /* Kill a random server. */
        i = (unsigned)(random() % N_SERVERS);

        rv = kill(pids[i], SIGINT);
        if (rv != 0) {
            printf("error: kill server %d: %s", i, strerror(errno));
        }

        waitpid(pids[i], &status, 0);

        rv = nanosleep(&interval, NULL);
        if (rv != 0) {
            printf("error: sleep: %s", strerror(errno));
        }

        forkServer(topLevelDir, i, &pids[i]);
    }

    return 0;
}
