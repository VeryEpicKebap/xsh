// xsh 0.2
// Written by VeryEpicKebap

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <sys/prctl.h>
#include <fcntl.h>

#define XSH_MAX_INPUT 1024
#define MAX_ARGS 64

void handle_sigint(int sig) {
    write(STDOUT_FILENO, "\n", 1);
}

char *expand_var(const char *arg) {
    static char result[XSH_MAX_INPUT];
    int i = 0, j = 0;
    while (arg[i] && j < XSH_MAX_INPUT - 1) {
        if (arg[i] == '$') {
            i++;
            char varname[64];
            int k = 0;
            while ((isalnum(arg[i]) || arg[i] == '_') && k < 63)
                varname[k++] = arg[i++];
            varname[k] = '\0';
            const char *val = getenv(varname);
            if (val) while (*val && j < XSH_MAX_INPUT - 1) result[j++] = *val++;
        } else result[j++] = arg[i++];
    }
    result[j] = '\0';
    return result;
}

int main() {
    if (!isatty(STDIN_FILENO)) {
        execlp("sh", "sh", "-c", "echo xsh", NULL);
        return 0;
    }
    prctl(PR_SET_NAME, "xsh", 0, 0, 0);
    signal(SIGINT, handle_sigint);
    {
        const char *home = getenv("HOME");
        if (home) {
            char rcpath[512];
            snprintf(rcpath, sizeof(rcpath), "%s/.xshrc", home);
            FILE *f = fopen(rcpath, "r");
            if (f) {
                char line[XSH_MAX_INPUT];
                while (fgets(line, sizeof(line), f)) {
                    line[strcspn(line, "\n")] = '\0';
                    if (!*line || line[0] == '#') continue;
                    char *eq = strchr(line, '=');
                    if (!eq) continue;
                    *eq = '\0';
                    setenv(line, eq + 1, 1);
                }
                fclose(f);
            }
        }
    }

    char input[XSH_MAX_INPUT];

    while (1) {
        const char *prompt = getenv("PROMPT");
        if (!prompt) prompt = "] ";
        printf("%s", prompt);
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = '\0';
        if (!*input) continue;

        char *args[MAX_ARGS];
        int argcount = 0;
        char *token = strtok(input, " \t");
        while (token && argcount < MAX_ARGS - 1) {
            args[argcount++] = strdup(expand_var(token));
            token = strtok(NULL, " \t");
        }
        args[argcount] = NULL;
        if (!args[0]) continue;

        if (strcmp(args[0], "exit") == 0) {
            for (int i = 0; i < argcount; i++) free(args[i]);
            break;
        }
        if (strcmp(args[0], "xshver") == 0) {
            printf("xsh (version 0.2)\n");
        }
        else if (strcmp(args[0], "cd") == 0) {
            if (!args[1]) {
                const char *home = getenv("HOME");
                if (home) chdir(home);
                else fprintf(stderr, "cd: HOME not set\n");
            } else if (chdir(args[1]) != 0) perror("cd");
        }
        else {
            int pipe_pos = -1, redirect_pos = -1;
            for (int i = 0; i < argcount; i++) {
                if (strcmp(args[i], "|") == 0) pipe_pos = i;
                if (strcmp(args[i], ">") == 0) redirect_pos = i;
            }

            if (pipe_pos != -1) {
                args[pipe_pos] = NULL;
                int fd[2]; pipe(fd);
                pid_t p1 = fork();
                if (p1 == 0) {
                    dup2(fd[1], STDOUT_FILENO);
                    close(fd[0]); close(fd[1]);
                    execvp(args[0], args);
                    perror("execvp"); exit(1);
                }
                pid_t p2 = fork();
                if (p2 == 0) {
                    dup2(fd[0], STDIN_FILENO);
                    close(fd[0]); close(fd[1]);
                    execvp(args[pipe_pos + 1], &args[pipe_pos + 1]);
                    perror("execvp"); exit(1);
                }
                close(fd[0]); close(fd[1]);
                waitpid(p1, NULL, 0);
                waitpid(p2, NULL, 0);
            }
            else if (redirect_pos != -1) {
                args[redirect_pos] = NULL;
                int fd = open(args[redirect_pos + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) perror("open");
                else {
                    pid_t pid = fork();
                    if (pid == 0) {
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                        execvp(args[0], args);
                        perror("execvp"); exit(1);
                    }
                    close(fd);
                    waitpid(pid, NULL, 0);
                }
            }
            else {
                pid_t pid = fork();
                if (pid == 0) {
                    execvp(args[0], args);
                    perror("execvp"); exit(1);
                }
                waitpid(pid, NULL, 0);
            }
        }

        for (int i = 0; i < argcount; i++) free(args[i]);
    }
    return 0;
}