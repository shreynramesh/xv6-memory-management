#include "types.h"
#include "user.h"
#include "stat.h"
#include "mmap.h"

int main() {
    uint addr = 0x60020000;
    int len = 4000;
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_ANON | MAP_FIXED | MAP_SHARED;
    int fd = -1;

    /* mmap anon memory */
    void *mem = mmap((void *)addr, len, prot, flags, fd, 0);
    printf(1, "DEBUG: VM Memory: %p\n", mem);
    if (mem == (void *)-1) {
	    goto failed;
    }
    if (mem != (void *)addr) {
	    goto failed;
    } 

    /* Modify something */
    char *memchar = (char*) mem;
    memchar[0] = 'a'; 
    printf(1, "DEBUG: memchar[0] %c\n", memchar[0]);
    memchar[1] = 'b';
    printf(1, "DEBUG: memchar[1] %c\n", memchar[1]);

    int pid = fork();
    if (pid == 0) {
        printf(1, "Inside child %c\n", memchar[0]);
        printf(1, "Inside child %c\n", memchar[1]);

        // change the first element
        memchar[0] = 'c';

        printf(1, "Modified and inside child %c %c\n", memchar[0], memchar[1]);

    } else {
        wait();
        printf(1, "here\n");

        printf(1, "Inside parent %c %c\n", memchar[0], memchar[1]);
    }

    // free the memory
    int ret = munmap(mem, len);
    if (ret < 0) {
	    goto failed;
    }

    failed:
    printf(1, "MMAP\t FAILED\n");
    exit();
}
