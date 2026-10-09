#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char **argv) {
    char buffer[4096];
    ssize_t bytes;

    int file = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
    if (file == -1) {
        const char msg[] = "Error: failed to open file\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(1);
    }

    while (bytes = read(STDIN_FILENO, buffer, sizeof(buffer) - 1)) {
        if (bytes < 0) {
			const char msg[] = "Error: failed to read from stdin\n";
			write(STDERR_FILENO, msg, sizeof(msg));
			exit(1);
		}

        buffer[bytes] = '\0';

        float numbers[256];
        int count = 0;
        char *token = strtok(buffer, " \t\n");
        while (token != NULL && count < 256) {
            numbers[count++] = strtof(token, NULL);
            token = strtok(NULL, " \t\n");
        }

        float result = numbers[0];
        for (int i = 1; i < count; ++i) {
            if (numbers[i] == 0) {
                const char msg[] = "Division by zero\n";
                write(STDOUT_FILENO, msg, sizeof(msg));
                close(file);
                exit(1);
            }
            result /= numbers[i];
        }

        char res[64];
        int result_string = snprintf(res, sizeof(res), "Result = %f\n", result);
        int written = write(file, res, result_string);
        write(STDOUT_FILENO, res, result_string);

        if (written != result_string) {
            const char msg[] = "Error: failed to write to file\n";
            write(STDERR_FILENO, msg, sizeof(msg));
            close(file);
            exit(1);
        }
    }
    close(file);
    return 0;
}
