#include "types.h"
#include "user.h"
#include "stat.h"
#include "mmap.h"

int main() {
    uint addr = 0x60020000;
    int len = 12000;
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_ANON | MAP_FIXED | MAP_SHARED;
    int fd = -1;

    /* mmap anon memory */
    void *mem = mmap((void *)addr, len, prot, flags, fd, 0);
    printf(1, "DEBUG: VM Memory: %p\n", mem);
    if (mem == (void *)-1) {
	    goto failed;
    }
    // if (mem != (void *)addr) {
	//     goto failed;
    // } 

    /* Modify something */
    char *memchar = (char*) mem;
    memchar[0] = 'a'; 
    printf(1, "DEBUG: memchar[0] %c\n", memchar[0]);
    memchar[1] = 'b';
    printf(1, "DEBUG: memchar[1] %c\n", memchar[1]);

    flags = MAP_ANON | MAP_SHARED;

    /* mmap anon memory */
    void *mem1 = mmap((void *)addr, len, prot, flags, fd, 0);
    printf(1, "DEBUG: VM Memory: %p %d\n", mem1, len);
    if (mem1 == (void *)-1) {
	    goto failed;
    }
    // if (mem != (void *)addr) {
	//     goto failed;
    // } 

    /* Modify something */
    char *memchar1 = (char*) mem1;
    memchar1[0] = 'c'; 
    printf(1, "DEBUG: memchar[0] %c\n", memchar1[0]);
    memchar1[1] = 'd';
    printf(1, "DEBUG: memchar[1] %c\n", memchar1[1]);
    /* Clean and return */
    int ret = munmap(mem1, len);
    if (ret < 0) {
	    goto failed;
    }
    
    ret = munmap(mem, len);
    if (ret < 0) {
	    goto failed;
    }

// success:
    printf(1, "MMAP\t SUCCESS\n");
    exit();

failed:
    printf(1, "MMAP\t FAILED\n");
    exit();
}
