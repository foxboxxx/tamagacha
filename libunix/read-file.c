#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "demand.h"
#include "libunix.h"

// allocate buffer, read entire file into it, return it.   
// buffer is zero padded to a multiple of 4.
//
//  - <size> = exact nbytes of file.
//  - for allocation: round up allocated size to 4-byte multiple, pad
//    buffer with 0s. 
//
// fatal error: open/read of <name> fails.
//   - make sure to check all system calls for errors.
//   - make sure to close the file descriptor (this will
//     matter for later labs).
// 
void *read_file(unsigned *size, const char *name) {
    // How: 
    //    - use stat() to get the size of the file.
    //    - round up to a multiple of 4.
    //    - allocate a buffer
    //    - zero pads to a multiple of 4.
    //    - read entire file into buffer (read_exact())
    //    - fclose() the file descriptor
    //    - make sure any padding bytes have zeros.
    //    - return it.   
    
    // create a stat struct here for the buffer
    struct stat stat_out;
    if (stat(name, &stat_out) == -1) {
        panic("error in stat(): %d", errno);
    }

    *size = stat_out.st_size;
    // round up to a multiple of 4
    unsigned alloc_size = stat_out.st_size + 3 & ~3;
    
    // use calloc to automatically zero out memory
    void *buf = calloc(1, alloc_size);
    int fd = open(name, O_RDONLY);
    if (fd < 0) {
        panic("error opening fd: %d", errno);
    }
    if (read(fd, buf, *size) == -1) {
        panic("read error: %d", errno);
    }

    close(fd);
    return buf;
}
