#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX 1024

void replace_vars(char *line) {
    char temp[MAX], *start = strchr(line, '$'), *value;
    while (start) {
        strncpy(temp, line, start - line);
        temp[start - line] = '\0';
        value = getenv(start + 1);
        if (value) {
            strcat(temp, value);
        } else {
            strcat(temp, "");
        }
        strcat(temp, strchr(start + 1, ' ') ?: "");
        strcpy(line, temp);
        start = strchr(line, '$');
    }
}

void split(char *line, char *words[], const char *delim) {
    char *token = strtok(line, delim);
    int i = 0;
    while (token) {
        words[i++] = token;
        token = strtok(NULL, delim);
    }
    words[i] = NULL;
}

void execute_command(char *cmd) {
    char *words[MAX];
    split(cmd, words, " ");
    execvp(words[0], words); // Replaced with `execvp` for PATH searching.
    perror("exec failed");
    exit(1);
}

void handle_redirection(char *cmd, char *input, char *output, int append) {
    if (input) {
        int fd = open(input, O_RDONLY);
        if (fd == -1) {
            perror("Input redirection failed");
            exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (output) {
        int fd = open(output, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);
        if (fd == -1) {
            perror("Output redirection failed");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    execute_command(cmd);
}

void handle_pipe(char *cmds[], int num_cmds) {
    int pipes[2], in_fd = 0;

    for (int i = 0; i < num_cmds; i++) {
        pipe(pipes);

        if (fork() == 0) {
            dup2(in_fd, STDIN_FILENO);
            if (i < num_cmds - 1) {
                dup2(pipes[1], STDOUT_FILENO);
            }
            close(pipes[0]);
            close(pipes[1]);
            execute_command(cmds[i]);
        }

        close(pipes[1]);
        in_fd = pipes[0];
    }

    while (wait(NULL) > 0);
}

void parse_and_execute(char *line) {
    replace_vars(line);

    char *cmds[MAX];
    split(line, cmds, "///");

    for (int i = 0; cmds[i]; i++) {
        char *input = NULL, *output = NULL;
        int append = 0, bg = 0;

        char *input_redirect = strstr(cmds[i], "<");
        char *output_redirect = strstr(cmds[i], ">");
        bg = cmds[i][strlen(cmds[i]) - 1] == '&';

        if (bg) {
            cmds[i][strlen(cmds[i]) - 1] = '\0';
        }

        if (input_redirect) {
            *input_redirect++ = '\0';
            input = strtok(input_redirect, " ");
        }

        if (output_redirect) {
            append = *(output_redirect + 1) == '>';
            *output_redirect++ = '\0';
            output_redirect += append;
            output = strtok(output_redirect, " ");
        }

        if (strcmp(cmds[i], "exit") == 0 || strcmp(cmds[i], "quit") == 0) {
            exit(0);
        } else if (strncmp(cmds[i], "cd ", 3) == 0) {
            char *path = cmds[i] + 3;
            if (chdir(path) == -1) {
                perror("cd failed");
            }
        } else if (strcmp(cmds[i], "pwd") == 0) {
            char cwd[MAX];
            if (getcwd(cwd, sizeof(cwd))) {
                printf("%s\n", cwd);
            } else {
                perror("pwd failed");
            }
        } else if (strchr(cmds[i], '|')) {
            char *pipe_cmds[MAX];
            split(cmds[i], pipe_cmds, "|");
            handle_pipe(pipe_cmds, num_cmds);
        } else if (fork() == 0) {
            handle_redirection(cmds[i], input, output, append);
        } else if (!bg) {
            wait(NULL);
        }
    }
}

int main(int argc, char *argv[]) {
    char line[MAX];

    if (argc > 1) {
        strcpy(line, argv[1]);
        parse_and_execute(line);
        return 0;
    }

    while (1) {
        printf("xsh# ");
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        line[strcspn(line, "\n")] = '\0';

        if (strlen(line) > 0) {
            parse_and_execute(line);
        }
    }

    return 0;
}
