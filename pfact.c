#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>
#include "filter.h"

#define READ 0
#define WRITE 1


void close_fd(int fd);
void pipe_fd(int *fd);
void read_integer(int fd, int *store);
void write_integer(int fd, int *store);
void wait_process(int *status);
int check_command(int argc, char *argv[]);
void zero_filter(int integer);
void clean_pipe(int fd);
int check_factor(int integer, int factor, int candidate, int fd);

int main(int argc, char *argv[]) {
    int integer, filter_num, candidate, candidate_factor;
    int start, loop_filter;
    int filter_status, status;
    int read_fd[2];
    int write_fd[2];
    int factor = 1;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    integer = check_command(argc, argv);

    // 0 filter case
    if (integer == 2 || integer == 3 || integer == 4) {
        zero_filter(integer);
        return 0;
    }

    // number of filters > 0, fork begin
    pipe_fd(read_fd);

    // keep track of factor
    if (integer % 2 == 0) {
        factor = 2;
    }

    start = fork();
    if (start < 0) {
        perror("fork");
        exit(1);
    }

    // in the child process, iteration creates filters
    if (start == 0) {
        while (1) {
            close_fd(read_fd[WRITE]);

            read_integer(read_fd[READ], &filter_num);
            read_integer(read_fd[READ], &candidate);

            // helper function check factor and print
            candidate_factor = check_factor(integer, factor, candidate, read_fd[READ]);

            // open new write pipe before next fork
            pipe_fd(write_fd);

            write_integer(write_fd[WRITE], &candidate);
            if (filter(filter_num, read_fd[READ], write_fd[WRITE]) != 0) {
                fprintf(stderr, "Error filter function");
                exit(1);
            }
            close_fd(read_fd[READ]);

            // update factor before fork
            if (candidate_factor != 1) {
                factor = candidate_factor;
            }

            loop_filter = fork();

            if (loop_filter > 0) {
                close_fd(write_fd[READ]);
                close_fd(write_fd[WRITE]);

                wait_process(&filter_status);
                if (WIFEXITED(filter_status)) {
                    exit(WEXITSTATUS(filter_status) + 1);
                }
            } else if (loop_filter == 0) {
                // child's read-fd get the copy of parent's write_fd
                read_fd[0] = write_fd[0];
                read_fd[1] = write_fd[1];
            }
        }
    } else if (start > 0) { // outer parent process
        close_fd(read_fd[READ]);
        for (int i = 2; i <= integer; i++) {
            write_integer(read_fd[WRITE], &i);
        }
        close_fd(read_fd[WRITE]);
    }

    // outer parent process collect information
    wait_process(&status);
    if (WIFEXITED(status)) {
        printf("Number of filters = %d\n", WEXITSTATUS(status));
    }
    return 0;
}

/*
 * Close the file descriptor fd and error checking.
 */
void close_fd(int fd) {
    if (close(fd) == -1) {
        perror("close file descriptor");
        exit(1);
    }
}

/*
 * Pipe the file descriptor and error checking.
 */
void pipe_fd(int *fd) {
    if (pipe(fd) == -1) {
        perror("pipe the file descriptor");
        exit(1);
    }
}

/*
 * Read the integer from the file descriptor fd the variable
 * store and error checking.
 */
void read_integer(int fd, int *store) {
    if (read(fd, store, sizeof(int)) != sizeof(int)) {
        perror("read from file descriptor");
        exit(1);
    }
}

/*
 * Write the integer from variable store to the file descriptor
 * fd and error checking.
 */
void write_integer(int fd, int *store) {
    if (write(fd, store, sizeof(int)) != sizeof(int)) {
        perror("write to file descriptor");
        exit(1);
    }
}

/*
 * Check command line argument, if it is a valid integer,
 * return the integer, otherwise exit.
 */
int check_command(int argc, char *argv[]) {
    int integer;
    errno = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage:\n\tpfact n\n");
        exit(1);
    }

    // argc == 2 case
    char *next = NULL;
    integer = (int) strtol(argv[1], &next, 0);
    if (errno != 0 || integer < 2 || strlen(next) != 0) {
        fprintf(stderr, "Usage:\n\tpfact n\n");
        exit(1);
    }

    // reset errno
    errno = 0;
    return integer;
}

/*
 * Deal with the special case with zero filter.
 */
void zero_filter(int integer) {
    if (integer == 2) {
        printf("%d is prime\n", integer);
        printf("Number of filters = 0\n");
    }

    if (integer == 3) {
        printf("%d is prime\n", integer);
        printf("Number of filters = 0\n");
    }

    if (integer == 4) {
        printf("%d 2 2\n", integer);
        printf("Number of filters = 0\n");
    }
}

/*
 * Clean the remaining integer in the last pipe
 */
void clean_pipe(int fd) {
    int integer;
    while (read(fd, &integer, sizeof(int)) == sizeof(int)) {
        ;
    }
}

/*
 * Check factor helper function, decide whether need to create next
 * filter. If not need to fork next process, print correspond message
 * statement and exit.
 */
int check_factor(int integer, int factor, int candidate, int fd) {
    int candidate_factor = 1;
    double sqrt_root = sqrt(integer);

    if (integer % candidate == 0) {
        candidate_factor = candidate;
    }

    if (candidate == sqrt_root) {
        printf("%d %d %d\n", integer, candidate, candidate);
        clean_pipe(fd);
        exit(1);
    }

    if (candidate > sqrt_root) {
        if (factor * candidate_factor == 1) {
            printf("%d is prime\n", integer);
            clean_pipe(fd);
            exit(1);
        }
        if (factor * candidate_factor == integer) {
            printf("%d %d %d\n", integer, factor, candidate_factor);
            clean_pipe(fd);
            exit(1);
        }

        // stop create filter, check remaining list whether there is a prime factor
        int potential_factor;
        while (read(fd, &potential_factor, sizeof(int)) == sizeof(int)) {
            if (integer % potential_factor == 0) {
                candidate_factor = potential_factor;
                break;
            }
        }

        // case like 16 and 8
        if ((1 < factor * candidate_factor && factor * candidate_factor < integer)
            || candidate_factor % factor == 0) {
            printf("%d is not the product of two primes\n", integer);
            clean_pipe(fd);
            exit(1);
        }

        if (factor * candidate_factor == integer) {
            printf("%d %d %d\n", integer, factor, candidate_factor);
            clean_pipe(fd);
            exit(1);
        }
    }

    if (candidate < sqrt_root) {
        // cases like 72, candidate < sqrt_root
        if (factor != 1 && candidate_factor != 1) {
            printf("%d is not the product of two primes\n", integer);
            clean_pipe(fd);
            exit(1);
        }
    }

    return candidate_factor;
}

