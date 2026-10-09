#include <unistd.h>
#include <stdint.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    
    char filename[1024];
    write(STDOUT_FILENO, "Enter filename: ", 16);
    ssize_t name = read(STDIN_FILENO, filename, sizeof(filename));
    if (name <= 0) {
        const char msg[] = "Error: failed to read filename\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(1);
    }
    if (filename[name - 1] == '\n') {
        filename[name - 1] = '\0';
        name--;
    }
    int pipe1[2];
    if (pipe(pipe1) == -1) {
        const char msg[] = "Error: failed to create pipe\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(1);
    }

    int pipe2[2];
    if (pipe(pipe2) == -1) {
        const char msg[] = "Error: failed to create pipe\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(1);
    }

    const pid_t child = fork();

    switch(child) {
    case -1: {
		const char msg[] = "Error: failed to spawn new process\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		exit(1);
	} break;

    case 0: {
        close(pipe1[1]);
        close(pipe2[0]);

        dup2(pipe1[0], STDIN_FILENO); 
        close(pipe1[0]);

        dup2(pipe2[1], STDOUT_FILENO);
        close(pipe2[1]);

        char *path = "./child_program";
        char *const args[] = {
            "./child_program",
            filename,
            NULL
        };

        int status = execv(path, args);
        if (status == -1) {
            const char msg[] = "Error: failed to exec\n";
            write(STDERR_FILENO, msg, sizeof(msg));
            exit(1);
        }
    } break;

    default: {
        close(pipe1[0]);
        close(pipe2[1]);

        char buffer[4096];
        ssize_t bytes;
        while (bytes = read(STDIN_FILENO, buffer, sizeof(buffer))) {
            if (bytes < 0) {
                const char msg[] = "Error: failed to read from stdin\n";
                write(STDERR_FILENO, msg, sizeof(msg));
                exit(1);
            } else if (buffer[0] == '\n') {
                break;
            }

            write(pipe1[1], buffer, bytes);

            bytes = read(pipe2[0], buffer, sizeof(buffer));
            if (bytes > 0) {
                write(STDOUT_FILENO, buffer, bytes);
                if (strstr(buffer, "Division by zero\n")) {
                    break;
                }
            }
        }

        close(pipe1[1]);
        close(pipe2[0]);

        wait(NULL);
    } break;
    }
    return 0;
}
