#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    const char *path = "/dev/phantomfpga0";

    printf("[TEST] Trying to open %s\n", path);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        printf("[FAIL] open failed: errno=%d (%s)\n",
               errno, strerror(errno));
        return 1;
    }

    printf("[OK] open succeeded, fd=%d\n", fd);

    if (close(fd) < 0) {
        printf("[FAIL] close failed: errno=%d (%s)\n",
               errno, strerror(errno));
        return 1;
    }

    printf("[OK] close succeeded\n");
    printf("[PASS] basic open/close test passed\n");

    return 0;
}
