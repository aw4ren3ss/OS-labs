#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define SHM_SIZE 4096

int main(int argc, char **argv) {

    char *shared_memory_name = argv[1];
    char *sem_parent_name = argv[2];
    char *sem_child_name = argv[3];

    int shm = shm_open(shared_memory_name, O_RDWR, 0600);
    if (shm == -1) {
        const char msg[] = "Error: failed to open shared memory\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(1);
    }

    char *shm_buffer = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
    if (shm_buffer == MAP_FAILED) {
        const char msg[] = "Error: failed to map shared memory\n";
        write(STDERR_FILENO, msg, sizeof(msg));

        close(shm);
        exit(1);
    }

    // connect to already existing semaphore
    sem_t *sem_parent = sem_open(sem_parent_name, 0);
    if (sem_parent == SEM_FAILED) {
        const char msg[] = "Error: failed to create parent semaphore\n";
        write(STDERR_FILENO, msg, sizeof(msg));

        munmap(shm_buffer, SHM_SIZE);
        close(shm);
        exit(1);
    }

    sem_t *sem_child = sem_open(sem_child_name, 0);
    if (sem_child == SEM_FAILED) {
        const char msg[] = "Error: failed to create child semaphore\n";
        write(STDERR_FILENO, msg, sizeof(msg));

        sem_close(sem_parent);
        munmap(shm_buffer, SHM_SIZE);
        close(shm);
        exit(1);
    }

    int file = open(argv[4], O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (file == -1) {
        const char msg[] = "Error: failed to open file\n";
        write(STDERR_FILENO, msg, sizeof(msg));

        sem_close(sem_parent);
        sem_close(sem_child);
        munmap(shm_buffer, SHM_SIZE);
        close(shm);
        exit(1);
    }

    bool running = true;

    while (running) {

        sem_wait(sem_child);

        uint32_t *length = (uint32_t *)shm_buffer;
        char *text = shm_buffer + sizeof(uint32_t);

        if (*length == UINT32_MAX) {
			running = false;
		}
        else if (*length > 0) {

            text[*length] = '\0';
            float numbers[256];
            int count = 0;
            bool err = false;
            char *token = strtok(text, " \t\n");
            while (token != NULL && count < 256) {
                char *end;
                float number = strtof(token, &end);
                if (end == token || *end != '\0') {
                    err = true;
                    break;
                }
                else {
                    numbers[count++] = number;
                    token = strtok(NULL, " \t\n");
                }
            }
            if (err || count == 0) {
                const char msg[] = "Incorrect input\n";
                memcpy(text, msg, sizeof(msg));
                *length = sizeof(msg);
                sem_post(sem_parent);

                continue;
            }

            float result = numbers[0];
            for (int i = 1; i < count; ++i) {
                if (numbers[i] == 0) {
                    const char msg[] = "Division by zero\n";
                    memcpy(text, msg, sizeof(msg));
                    *length = sizeof(msg);
                    sem_post(sem_parent);

                    sem_close(sem_parent);
                    sem_close(sem_child);
                    munmap(shm_buffer, SHM_SIZE);
                    close(shm);
                    close(file);
                    exit(1);
                }
                result /= numbers[i];
            }

            char res[64];
            int result_string = snprintf(res, sizeof(res), "Result = %f\n", result); // without '\0'
            int written = write(file, res, result_string);
            memcpy(text, res, result_string + 1);
            *length = result_string;

            if (written != result_string) {
                const char msg[] = "Error: failed to write to file\n";
                write(STDERR_FILENO, msg, sizeof(msg));

                sem_close(sem_parent);
                sem_close(sem_child);
                munmap(shm_buffer, SHM_SIZE);
                close(shm);
                close(file);
                exit(1);
            }
        }
        sem_post(sem_parent); 
    }

    sem_close(sem_parent);
    sem_close(sem_child);
    munmap(shm_buffer, SHM_SIZE);
    close(shm);
    close(file);
    return 0;
}