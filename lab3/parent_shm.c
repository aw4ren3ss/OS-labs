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

int main(int argc, char** argv) {
    
    char sem_parent_name[64];
    char sem_child_name[64];
    char shared_memory_name[64];

    pid_t ppid = getpid();

    snprintf(sem_parent_name, sizeof(sem_parent_name), "/sem_parent_%d", ppid);
    snprintf(sem_child_name, sizeof(sem_child_name), "/sem_child_%d", ppid);
    snprintf(shared_memory_name, sizeof(shared_memory_name), "/shm_%d", ppid);

    // open shm file descriptor
    int shm = shm_open(shared_memory_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (shm == -1) {
        const char msg[] = "Error: failed to open shared memory\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(1);
    }
    // resize shm
    if (ftruncate(shm, SHM_SIZE) == -1) {
        const char msg[] = "Error: failed to resize shared memory\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        
        shm_unlink(shared_memory_name);
        close(shm);
        exit(1);
    }
    // map shm 
    char *shm_buffer = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
    if (shm_buffer == MAP_FAILED) {
        const char msg[] = "Error: failed to map shared memory\n";
        write(STDERR_FILENO, msg, sizeof(msg));

        shm_unlink(shared_memory_name);
        close(shm);
        exit(1);
    }

    
    // file for result
    char filename[1024];
    write(STDOUT_FILENO, "Enter filename: ", 16);
    ssize_t name = read(STDIN_FILENO, filename, sizeof(filename));
    if (name <= 0 || (name == 1 && filename[0] == '\n')) {
        const char msg[] = "Error: failed to read filename\n";
        write(STDERR_FILENO, msg, sizeof(msg));

        munmap(shm_buffer, SHM_SIZE);
        shm_unlink(shared_memory_name);
        close(shm);
        exit(1);
    }
    if (filename[name - 1] == '\n') {
        filename[name - 1] = '\0';
    }
    else {
        filename[name] = '\0';
    }


    /* semaphore for parent process:
    0 - blocked
    1 - able to read/write */
    sem_t *sem_parent = sem_open(sem_parent_name, O_CREAT | O_EXCL, 0600, 0);
    if (sem_parent == SEM_FAILED) {
        const char msg[] = "Error: failed to create parent semaphore\n";
        write(STDERR_FILENO, msg, sizeof(msg));

        munmap(shm_buffer, SHM_SIZE);
        shm_unlink(shared_memory_name);
        close(shm);
        exit(1);
    }
    /* semaphore for child:
    0 - blocked
    1 - able to read/write */
    sem_t *sem_child = sem_open(sem_child_name, O_CREAT | O_EXCL, 0600, 0);
    if (sem_child == SEM_FAILED) {
        const char msg[] = "Error: failed to create child semaphore\n";
        write(STDERR_FILENO, msg, sizeof(msg));

        sem_unlink(sem_parent_name);
        sem_close(sem_parent);
        munmap(shm_buffer, SHM_SIZE);
        shm_unlink(shared_memory_name);
        close(shm);
        exit(1);
    }

    pid_t child = fork();

    switch(child) {
    case -1: {
		const char msg[] = "Error: failed to spawn new process\n";
		write(STDERR_FILENO, msg, sizeof(msg));

        sem_unlink(sem_parent_name);
        sem_close(sem_parent);
        sem_unlink(sem_child_name);
        sem_close(sem_child);
        munmap(shm_buffer, SHM_SIZE);
        shm_unlink(shared_memory_name);
        close(shm);
		exit(1);
	} break;

    case 0: {
        
        char *path = "./child_shm";
        char *const args[] = {
            "./child_shm",
            shared_memory_name,
            sem_parent_name,
            sem_child_name,
            filename,
            NULL
        };

        int status = execv(path, args);
        if (status == -1) {
            const char msg[] = "Error: failed to exec\n";
            write(STDERR_FILENO, msg, sizeof(msg));

            sem_close(sem_parent);
            sem_close(sem_child);
            munmap(shm_buffer, SHM_SIZE);
            close(shm);
		    exit(1);
        }
    } break;

    default: {
        
        bool running = true;

        while (running) {
            write(STDOUT_FILENO, "Enter numbers:\n", 15);
            
            // stdin buffer
            char buffer[SHM_SIZE - sizeof(uint32_t)];
            
            ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer));

            if (bytes < 0) {
                const char msg[] = "Error: failed to read from stdin\n";
                write(STDERR_FILENO, msg, sizeof(msg));

                sem_unlink(sem_parent_name);
                sem_close(sem_parent);
                sem_unlink(sem_child_name);
                sem_close(sem_child);
                munmap(shm_buffer, SHM_SIZE);
                shm_unlink(shared_memory_name);
                close(shm);
                exit(1);
            } 

            // size of mapped memory buffer
            uint32_t *length = (uint32_t *)shm_buffer;
            // pointer ariphmetics: text begins from address of shm_buf + ~4
            char *text = shm_buffer + sizeof(uint32_t);

            // the only correct input
            if (bytes > 0 && buffer[0] != '\n') {
                // copy of what was in stdin buffer to mapped shm text part
                *length = bytes;
                memcpy(text, buffer, bytes);
                sem_post(sem_child);
            }
            else {
                *length = UINT32_MAX;
                running = false;
                sem_post(sem_child);
                break;
            }

            sem_wait(sem_parent);

            if (*length > 0) {
                write(STDOUT_FILENO, text, *length);
                if (strstr(text, "Division by zero\n")) {
                    running = false;
                    break;
                }
            }
        }

        wait(NULL);
        sem_unlink(sem_parent_name);
        sem_close(sem_parent);
        sem_unlink(sem_child_name);
        sem_close(sem_child);
        munmap(shm_buffer, SHM_SIZE);
        shm_unlink(shared_memory_name);
        close(shm);

    } break;
    }
    return 0;
}