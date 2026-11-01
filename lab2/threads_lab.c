#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <time.h>

#define MAX_STACK_SIZE 1000

typedef struct {
	int start;
	int len;
} run;

typedef struct {
    int *arr;
    int start_idx;
    int end_idx;
} ThreadArgs;

int getMinrun(int n) {
	int r = 0;
	while (n >= 64) {
		r |= (n & 1);
		n >>= 1;
	}
	return n + r;
}

void reverseRun(int *arr, int start, int end) {
	while (start < end) {
		int temp = arr[start];
		arr[start] = arr[end];
		arr[end] = temp;
		start++;
		end--;
	}
}

void insertionSort(int* arr, int left, int right, int start) {
	for (int i = start; i <= right; ++i) {
		int temp = arr[i];
		int j = i - 1;
		while (j >= left && arr[j] > temp) {
			arr[j + 1] = arr[j];
			j--;
		}
		arr[j + 1] = temp;
	}
}

int createRun(int *arr, int size, int cur, int minrun) {

	if (cur >= size - 1) return size;

	int run_start = cur;
	int run_end = cur + 1;

	if (arr[run_start] <= arr[run_end]) {
		while (run_end + 1 < size && arr[run_end] <= arr[run_end + 1]) {
			run_end++;
		}
	}
	else {
		while (run_end + 1 < size && arr[run_end] > arr[run_end + 1]) {
			run_end++;
		}
		reverseRun(arr, run_start, run_end);
	}

	int run_len = run_end - run_start + 1;

	if (run_len < minrun) {
		int target_end = run_start + minrun - 1;
		if (target_end >= size) {
			target_end = size - 1;
		}
		insertionSort(arr, run_start, target_end, run_end + 1);
		run_end = target_end;
	}

	return run_end + 1;
}

void mergeRuns(int* arr, run* stack, int* stack_size, int idx) {
	run run1 = stack[idx];
	run run2 = stack[idx + 1];

	int* temp = (int*)malloc(run1.len * sizeof(int));
	if (temp == NULL) {
		printf("Memory error!\n");
		return;
	}

	for (int i = 0; i < run1.len; ++i) {
		temp[i] = arr[run1.start + i];
	}

	int cur_temp = 0;
	int cur_run2 = run2.start;
	int end_temp = run1.len;
	int end_run2 = run2.start + run2.len;
	int cur_final = run1.start;

	while (cur_temp < end_temp && cur_run2 < end_run2) {
		if (temp[cur_temp] <= arr[cur_run2]) {
			arr[cur_final++] = temp[cur_temp++];
		}
		else {
			arr[cur_final++] = arr[cur_run2++];
		}
	}

	while (cur_temp < end_temp) {
		arr[cur_final++] = temp[cur_temp++];
	}

	free(temp);
	stack[idx].len = run1.len + run2.len;
	for (int j = idx + 1; j < (*stack_size) - 1; ++j) {
		stack[j] = stack[j + 1];
	}
	(*stack_size)--;
}


void checkAndMerge(int* arr, run *stack, int* stack_size) {
	while (*stack_size > 1) {
		int n = *stack_size;

		if (n == 2) {
			int X = stack[1].len;
			int Y = stack[0].len;

			if (Y <= X) {
				mergeRuns(arr, stack, stack_size, 0);
			}
			else {
				break;
			}
		}
		else {
			int X = stack[n - 1].len;
			int Y = stack[n - 2].len;
			int Z = stack[n - 3].len;

			if (Z <= Y + X || Y <= X) {
				if (Z < X) {
					mergeRuns(arr, stack, stack_size, n - 3);
				}
				else {
					mergeRuns(arr, stack, stack_size, n - 2);
				}
			}
			else {
				break;
			}
		}
	}

}


void timSort(int* arr, int size) {
	if (size <= 1) return;

	run stack[MAX_STACK_SIZE];
	int stack_size = 0;

	int minrun = getMinrun(size);
	int current = 0;

	while (current < size) {
		int run_start = current;
		int next_current = createRun(arr, size, current, minrun);
		int run_len = next_current - run_start;

		stack[stack_size].start = run_start;
		stack[stack_size].len = run_len;
		stack_size++;

		checkAndMerge(arr, stack, &stack_size);

		current = next_current;
	}

	while (stack_size > 1) {
		mergeRuns(arr, stack, &stack_size, stack_size - 2);
	}
}

static void *work(void *_args)
{
	ThreadArgs *args = (ThreadArgs *)_args;

    int *arr = args->arr;
    int start_idx = args->start_idx;
    int end_idx = args->end_idx;

    int local_size = end_idx - start_idx + 1;
    if (local_size <= 0) return NULL;

    timSort(&(arr[start_idx]), local_size);

	return NULL;
}

int main(int argc, char **argv) {
    // ./threads_lab -s <arrSize> -t <n_threads> -a <y/n>

    int size = 0;
    int n_threads = 0;
    int arr_marker = 0;

    if (argc != 7) {
        printf("Your input must look like:\n ./threads_lab -s <arrSize> -t <n_threads> -a <1/0>\n");
        return 1;
    }

    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "-s") == 0) {
            if (sscanf(argv[i + 1], "%d", &size) != 1 || size <= 0) {
                printf("Wrong array size parameter: %s.\n", argv[i + 1]);
                return 1;
            } 
        } else if (strcmp(argv[i], "-t") == 0) {
            if (sscanf(argv[i + 1], "%d", &n_threads) != 1 || n_threads <= 0) {
                printf("Wrong threads number parameter: %s.\n", argv[i + 1]);
                return 1;
            }
        } else if (strcmp(argv[i], "-a") == 0) {
            if (sscanf(argv[i + 1], "%d", &arr_marker) != 1 || (arr_marker != 1 && arr_marker != 0)) {
                printf("Wrong array output marker parameter: %s.\n", argv[i + 1]);
                return 1;
            }
        }
        
        else {
            printf("Unknown key: %s\n", argv[i]);
            return 1;
        }
    }

    int *arr = (int*)malloc(size * sizeof(int));
    if (arr == NULL) {
        printf("Memory error\n");
        free(arr);
        return 1;
    }

    srand((unsigned int)time(NULL));
    for (int i = 0; i < size; ++i) {
        arr[i] = (rand() % 65536) - 32768;
    }

    if (arr_marker == 1) {
        printf("Original array:\n");
        for (int i = 0; i < size; ++i) {
            printf("%d ", arr[i]);
        }
        printf("\n");
    }
    
    int start_idx = 0;
    int *test_arr = (int*)malloc(size * sizeof(int));
    for (int i = 0; i < size; ++i) {
        test_arr[i] = arr[i];
    }
    if (test_arr == NULL) {
        printf("Memory error\n");
        free(test_arr);
        return 1;
    }

    struct timespec start, end;

    ThreadArgs serial = (ThreadArgs){
        .arr = test_arr,
        .start_idx = start_idx,
        .end_idx = size - 1,
    };

    clock_gettime(CLOCK_MONOTONIC, &start);
    work(&serial);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double serial_time = (end.tv_sec - start.tv_sec) * 1e3 + (end.tv_nsec - start.tv_nsec) / 1e6;

    printf("Serial sort:\n");
    if (arr_marker == 1) {
        for (int i = 0; i < size; ++i) {
            printf("%d ", test_arr[i]);
        }
        printf("\n");
    }
    printf("time: %f ms\n", serial_time);

    int bucket = size / n_threads;
    int left_elements = size % n_threads;
    int minrun = getMinrun(size);

    if (bucket < minrun) {
        n_threads = size / minrun;
        bucket = size / n_threads;
        left_elements = size % n_threads;
    }

	pthread_t *threads = (pthread_t*)malloc(n_threads * sizeof(pthread_t));
	ThreadArgs *thread_args = (ThreadArgs*)malloc(n_threads * sizeof(ThreadArgs));
    if (threads == NULL || thread_args == NULL) {
        printf("Memory error\n");
        free(threads);
        free(thread_args);
        return 1;
    }

    for (int i = 0; i < size; ++i) {
        test_arr[i] = arr[i];
    }

	for (int i = 0; i < n_threads; ++i) {
        int bucket_size = bucket + (left_elements > 0 ? 1 : 0);
        left_elements--;

		thread_args[i] = (ThreadArgs){
			.arr = test_arr,
			.start_idx = start_idx,
            .end_idx = start_idx + bucket_size - 1,
		};

        start_idx += bucket_size;
	}

    printf("Parallel sort:\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    int threads_created = 0;
    for (int i = 0; i < n_threads - 1; ++i) {
		if (pthread_create(&threads[i], NULL, work, &thread_args[i]) != 0) {
            printf("Error while creating thread.\n");
            for (int j = 0; j < threads_created; ++j) {
                pthread_join(threads[j], NULL);
            }
            free(threads);
            free(thread_args);
            return 1;
        }
        ++threads_created;
	}

    work(&(thread_args[n_threads - 1]));

    run *global_stack = (run*)malloc(n_threads * sizeof(run));
    int global_stack_size = 0;

    for (int i = 0; i < threads_created; ++i) {
        pthread_join(threads[i], NULL);
    }

	for (int i = 0; i < n_threads; ++i) {
        run adding_run;
        adding_run.start = thread_args[i].start_idx;
        adding_run.len = thread_args[i].end_idx - thread_args[i].start_idx + 1;
        global_stack[global_stack_size++] = adding_run;
	}
    
    free(threads);
    free(thread_args);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double parallel_time = (end.tv_sec - start.tv_sec) * 1e3 + (end.tv_nsec - start.tv_nsec) / 1e6;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    while (global_stack_size > 1) {
		mergeRuns(test_arr, global_stack, &global_stack_size, global_stack_size - 2);
	}
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double merge_time = (end.tv_sec - start.tv_sec) * 1e3 + (end.tv_nsec - start.tv_nsec) / 1e6;
    double total_parallel_time = parallel_time + merge_time;

    if (arr_marker == 1) {
        for (int i = 0; i < size; ++i) {
            printf("%d ", test_arr[i]);
        }
        printf("\n");
    }

    double speedup = serial_time / total_parallel_time;
    double efficiency = speedup / n_threads;

    printf("work time: %f ms; merge time: %f ms; time: %f ms\n", parallel_time, merge_time, total_parallel_time);
    printf("speedup: %f; efficiency: %f\n", speedup, efficiency * 100.0);
    free(global_stack);
    free(arr);
    free(test_arr);
    
    return 0;
}