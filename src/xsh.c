// xsh 0.1
// Written by VeryEpicKebap

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <sys/prctl.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64

void handle_sigint(int sig) {
    write(STDOUT_FILENO, "\n", 1);
}

char *expand_var(const char *arg) {
    static char result[MAX_INPUT];
    int i = 0, j = 0;

    while (arg[i] && j < MAX_INPUT - 1) {
        if (arg[i] == '$') {
            i++;
            char varname[64];
            int k = 0;

            while ((isalnum(arg[i]) || arg[i] == '_') && k < 63) {
                varname[k++] = arg[i++];
            }
            varname[k] = '\0';

            const char *val = getenv(varname);
            if (val) {
                while (*val && j < MAX_INPUT - 1)
                    result[j++] = *val++;
            }
        } else {
            result[j++] = arg[i++];
        }
    }
    result[j] = '\0';
    return result;
}

int main(int argc, char *argv[]) {
    if (!isatty(STDIN_FILENO)) {
        execlp("sh", "sh", "-c", "echo xsh", NULL);
        return 0;
    }
    prctl(PR_SET_NAME, "xsh", 0, 0, 0);
    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    signal(SIGINT, handle_sigint);

    while (1) {
        printf(" ]  ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;

        input[strcspn(input, "\n")] = '\0';
        if (strlen(input) == 0) continue;

        int argcount = 0;
        char *token = strtok(input, " \t");
        while (token && argcount < MAX_ARGS - 1) {
            args[argcount++] = strdup(expand_var(token));
            token = strtok(NULL, " \t");
        }
        args[argcount] = NULL;

        if (args[0] == NULL) continue;

        if (strcmp(args[0], "exit") == 0) {
            break;
        }
        if (strcmp(args[0], "xshver") == 0) {
            printf("xsh (version 0.1)\n");
            continue;
        }
        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                const char *home = getenv("HOME");
                if (home) chdir(home);
                else fprintf(stderr, "cd: HOME not set\n");
            } else {
                if (chdir(args[1]) != 0)
                    perror("cd");
            }
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            execvp(args[0], args);
            perror("execvp");
            exit(1);
        } else if (pid > 0) {
            waitpid(pid, NULL, 0);
        } else {
            perror("fork");
        }

        for (int i = 0; i < argcount; i++) {
            free(args[i]);
        }
    }

    return 0;
}