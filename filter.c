#include <stdio.h>
#include <unistd.h>
#include <errno.h>

/*
 * Takes three arguments: the filter value n, a file descriptor readfd
 * from which integers are received, and a file descriptor writefd
 * to which integers are written.
 * Its purpose is to remove (filter) any integers from the data stream
 * that are multiples of m. The function returns 0 if it completes
 * without encountering an error and 1 otherwise.
 */

int filter(int n, int readfd, int writefd) {
    int number;
    errno = 0;

    // read from read_fd, if not multiple of m, write to write_fd
    while (read(readfd, &number, sizeof(int)) == sizeof(int)) {
        if (number % n != 0) {
            write(writefd, &number, sizeof(int));
        }
    }

    // errno set to 1 indicate encountering error
    if (errno != 0) {
        return 1;
    }

    return 0;
}

