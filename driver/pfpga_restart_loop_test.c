#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "phantomfpga_uapi.h"

#define ITERATIONS 20

static uint32_t read_le32(const unsigned char *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int set_config(int fd)
{
    struct phantomfpga_config cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.desc_count = 256;
    cfg.frame_rate = 5;
    cfg.irq_coalesce_count = 1;
    cfg.irq_coalesce_timeout = 40000;

    if (ioctl(fd, PHANTOMFPGA_IOCTL_SET_CFG, &cfg) < 0) {
        printf("[FAIL] SET_CFG failed: errno=%d (%s)\n",
               errno, strerror(errno));
        return -1;
    }

    return 0;
}

static int wait_for_frame(int fd)
{
    struct pollfd pfd;
    memset(&pfd, 0, sizeof(pfd));

    pfd.fd = fd;
    pfd.events = POLLIN;

    int rc = poll(&pfd, 1, 3000);

    if (rc < 0) {
        printf("[FAIL] poll failed: errno=%d (%s)\n",
               errno, strerror(errno));
        return -1;
    }

    if (rc == 0) {
        printf("[FAIL] poll timeout\n");
        return -1;
    }

    if (pfd.revents & (POLLERR | POLLNVAL | POLLHUP)) {
        printf("[FAIL] poll returned bad revents=0x%x\n", pfd.revents);
        return -1;
    }

    if (!(pfd.revents & POLLIN)) {
        printf("[FAIL] poll did not return POLLIN, revents=0x%x\n",
               pfd.revents);
        return -1;
    }

    return 0;
}

static int read_one_frame(int fd, int iter)
{
    unsigned char frame[PHANTOMFPGA_FRAME_SIZE];

    memset(frame, 0, sizeof(frame));

    ssize_t n = read(fd, frame, sizeof(frame));
    if (n < 0) {
        printf("[FAIL] iteration %d: read failed: errno=%d (%s)\n",
               iter, errno, strerror(errno));
        return -1;
    }

    if (n != PHANTOMFPGA_FRAME_SIZE) {
        printf("[FAIL] iteration %d: expected %u bytes, got %zd\n",
               iter, PHANTOMFPGA_FRAME_SIZE, n);
        return -1;
    }

    uint32_t magic = read_le32(frame + 0);
    uint32_t seq = read_le32(frame + 4);
    uint32_t crc = read_le32(frame + PHANTOMFPGA_FRAME_SIZE - 4);

    printf("[OK] iteration %d: read %zd bytes, magic=0x%08x seq=%u crc=0x%08x\n",
           iter, n, magic, seq, crc);

    if (magic != PHANTOMFPGA_FRAME_MAGIC) {
        printf("[FAIL] iteration %d: bad magic, expected 0x%08x\n",
               iter, PHANTOMFPGA_FRAME_MAGIC);
        return -1;
    }

    if (seq >= PHANTOMFPGA_FRAME_COUNT) {
        printf("[FAIL] iteration %d: bad sequence %u\n", iter, seq);
        return -1;
    }

    return 0;
}

static int stop_device(int fd)
{
    if (ioctl(fd, PHANTOMFPGA_IOCTL_STOP) < 0) {
        printf("[FAIL] STOP failed: errno=%d (%s)\n",
               errno, strerror(errno));
        return -1;
    }

    return 0;
}

int main(void)
{
    int fd = open("/dev/phantomfpga0", O_RDWR);
    if (fd < 0) {
        printf("[FAIL] open failed: errno=%d (%s)\n",
               errno, strerror(errno));
        return 1;
    }

    printf("[OK] open succeeded\n");

    ioctl(fd, PHANTOMFPGA_IOCTL_STOP);

    for (int i = 0; i < ITERATIONS; i++) {
        printf("\n[TEST] iteration %d/%d\n", i + 1, ITERATIONS);

        if (set_config(fd) < 0) {
            close(fd);
            return 1;
        }

        printf("[TEST] START\n");

        if (ioctl(fd, PHANTOMFPGA_IOCTL_START) < 0) {
            printf("[FAIL] iteration %d: START failed: errno=%d (%s)\n",
                   i, errno, strerror(errno));
            close(fd);
            return 1;
        }

        printf("[OK] START succeeded\n");

        if (wait_for_frame(fd) < 0) {
            stop_device(fd);
            close(fd);
            return 1;
        }

        if (read_one_frame(fd, i) < 0) {
            stop_device(fd);
            close(fd);
            return 1;
        }

        printf("[TEST] STOP\n");

        if (stop_device(fd) < 0) {
            close(fd);
            return 1;
        }

        printf("[OK] STOP succeeded\n");
    }

    close(fd);

    printf("\n[PASS] restart loop test passed: %d iterations\n", ITERATIONS);
    return 0;
}
