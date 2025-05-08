#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DEVICE_FILE "/dev/int_stack"
#define INT_STACK_MAGIC 'S'
#define SET_STACK_SIZE _IOW(INT_STACK_MAGIC, 1, unsigned int)

/* Error messages */
#define ERR_STACK_FULL "ERROR: stack is full"
#define ERR_INVALID_SIZE "ERROR: size should be > 0"
#define ERR_DEVICE_ACCESS "ERROR: could not access the device file. Is the module loaded?"
#define ERR_DEVICE_IOCTL "ERROR: ioctl operation failed"

/* Function prototypes */
int push(int value);
int pop(int *value);
int unwind();
int set_size(unsigned int size);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [push VALUE | pop | unwind | set-size SIZE]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "push") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s push VALUE\n", argv[0]);
            return 1;
        }
        int value = atoi(argv[2]);
        int result = push(value);
        if (result < 0) {
            if (result == -ERANGE) {
                fprintf(stderr, "%s\n", ERR_STACK_FULL);
                return -ERANGE;
            } else {
                fprintf(stderr, "ERROR: push operation failed with code %d\n", result);
                return result;
            }
        }
    } else if (strcmp(argv[1], "pop") == 0) {
        int value;
        int result = pop(&value);
        if (result == 0) {
            printf("NULL\n");
        } else if (result > 0) {
            printf("%d\n", value);
        } else {
            fprintf(stderr, "ERROR: pop operation failed with code %d\n", result);
            return result;
        }
    } else if (strcmp(argv[1], "unwind") == 0) {
        return unwind();
    } else if (strcmp(argv[1], "set-size") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s set-size SIZE\n", argv[0]);
            return 1;
        }
        int size = atoi(argv[2]);
        if (size <= 0) {
            fprintf(stderr, "%s\n", ERR_INVALID_SIZE);
            return -EINVAL;
        }
        int result = set_size(size);
        if (result < 0) {
            fprintf(stderr, "%s (error: %d)\n", ERR_DEVICE_IOCTL, result);
            return result;
        }
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        fprintf(stderr, "Usage: %s [push VALUE | pop | unwind | set-size SIZE]\n", argv[0]);
        return 1;
    }

    return 0;
}

/* Push a value onto the stack */
int push(int value) {
    int fd = open(DEVICE_FILE, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "%s\n", ERR_DEVICE_ACCESS);
        return -errno;
    }

    ssize_t result = write(fd, &value, sizeof(int));
    int saved_errno = errno;
    close(fd);
    
    if (result < 0) {
        if (saved_errno == ERANGE) {
            return -ERANGE;
        }
        return -saved_errno;
    }
    
    return result;
}

/* Pop a value from the stack */
int pop(int *value) {
    int fd = open(DEVICE_FILE, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "%s\n", ERR_DEVICE_ACCESS);
        return -errno;
    }

    ssize_t result = read(fd, value, sizeof(int));
    int saved_errno = errno;
    close(fd);
    
    if (result < 0)
        return -saved_errno;
    
    return result;
}

/* Pop all values from the stack and print them */
int unwind() {
    int fd = open(DEVICE_FILE, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "%s\n", ERR_DEVICE_ACCESS);
        return -errno;
    }

    int value;
    ssize_t result;
    int count = 0;

    while ((result = read(fd, &value, sizeof(int))) > 0) {
        printf("%d\n", value);
        count++;
    }

    close(fd);

    if (count == 0) {
        printf("NULL\n");
    }

    return 0;
}

/* Set the maximum size of the stack */
int set_size(unsigned int size) {
    int fd = open(DEVICE_FILE, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "%s\n", ERR_DEVICE_ACCESS);
        return -errno;
    }

    int result = ioctl(fd, SET_STACK_SIZE, &size);
    int saved_errno = errno;
    close(fd);
    
    if (result < 0)
        return -saved_errno;
    
    return result;
} 