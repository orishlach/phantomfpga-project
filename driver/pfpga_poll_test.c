#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "phantomfpga_uapi.h"

static uint32_t read_le32(const unsigned char *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int stop_device(int fd)
{
    if (ioctl(fd, PHANTOMFPGA_IOCTL_STOP) < 0) {
        printf("[WARN] STOP failed: errno=%d (%s)\n",
               errno, strerror(errno));
        return -1;
    }

    printf("[OK] STOP succeeded\n");
    return 0;
}

static int wait_for_frame_with_poll(int fd, int frame_index)
{
    struct pollfd pfd;

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = fd;
    pfd.events = POLLIN;

    printf("[TEST] poll before frame #%d, timeout=3000ms\n", frame_index);

    int rc = poll(&pfd, 1, 3000);

    if (rc < 0) {
        printf("[FAIL] poll failed: errno=%d (%s)\n",
               errno, strerror(errno));
        return -1;
    }

    if (rc == 0) {
        printf("[FAIL] poll timeout: no frame became ready\n");
        return -1;
    }

    printf("[OK] poll returned rc=%d revents=0x%x\n", rc, pfd.revents);

    if (pfd.revents & (POLLERR | POLLNVAL)) {
        printf("[FAIL] poll returned error revents=0x%x\n", pfd.revents);
        return -1;
    }

    if (pfd.revents & POLLHUP) {
        printf("[FAIL] poll returned POLLHUP, streaming may have stopped\n");
        return -1;
    }

    if (!(pfd.revents & POLLIN)) {
        printf("[FAIL] expected POLLIN, got revents=0x%x\n", pfd.revents);
        return -1;
    }

    return 0;
}

int main(void)
{
    const char *path = "/dev/phantomfpga0";

    printf("[TEST] opening %s\n", path);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        printf("[FAIL] open failed: errno=%d (%s)\n",
               errno, strerror(errno));
        return 1;
    }

    printf("[OK] open succeeded, fd=%d\n", fd);

    /*
     * Safety: stop any previous streaming state.
     */
    ioctl(fd, PHANTOMFPGA_IOCTL_STOP);

    struct phantomfpga_config cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.desc_count = 256;
    cfg.frame_rate = 5;
    cfg.irq_coalesce_count = 1;
    cfg.irq_coalesce_timeout = 40000;

    printf("[TEST] SET_CFG\n");

    if (ioctl(fd, PHANTOMFPGA_IOCTL_SET_CFG, &cfg) < 0) {
        printf("[FAIL] SET_CFG failed: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return 1;
    }

    printf("[OK] SET_CFG succeeded\n");

    printf("[TEST] START\n");

    if (ioctl(fd, PHANTOMFPGA_IOCTL_START) < 0) {
        printf("[FAIL] START failed: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return 1;
    }

    printf("[OK] START succeeded\n");

    unsigned char frame[PHANTOMFPGA_FRAME_SIZE];

    for (int i = 0; i < 5; i++) {
        if (wait_for_frame_with_poll(fd, i) < 0) {
            stop_device(fd);
            close(fd);
            return 1;
        }

        memset(frame, 0, sizeof(frame));

        printf("[TEST] read frame #%d after poll\n", i);

        ssize_t n = read(fd, frame, sizeof(frame));
        if (n < 0) {
            printf("[FAIL] read failed after poll: errno=%d (%s)\n",
                   errno, strerror(errno));
            stop_device(fd);
            close(fd);
            return 1;
        }

        printf("[OK] read returned %zd bytes\n", n);

        if (n != PHANTOMFPGA_FRAME_SIZE) {
            printf("[FAIL] expected %u bytes, got %zd\n",
                   PHANTOMFPGA_FRAME_SIZE, n);
            stop_device(fd);
            close(fd);
            return 1;
        }

        uint32_t magic = read_le32(frame + 0);
        uint32_t seq = read_le32(frame + 4);
        uint32_t crc = read_le32(frame + PHANTOMFPGA_FRAME_SIZE - 4);

        printf("  magic = 0x%08x\n", magic);
        printf("  seq   = %u\n", seq);
        printf("  crc   = 0x%08x\n", crc);

        if (magic != PHANTOMFPGA_FRAME_MAGIC) {
            printf("[FAIL] bad magic, expected 0x%08x\n",
                   PHANTOMFPGA_FRAME_MAGIC);
            stop_device(fd);
            close(fd);
            return 1;
        }

        if (seq >= PHANTOMFPGA_FRAME_COUNT) {
            printf("[FAIL] bad sequence, expected 0..%u\n",
                   PHANTOMFPGA_FRAME_COUNT - 1);
            stop_device(fd);
            close(fd);
            return 1;
        }
    }

    stop_device(fd);

    printf("[PASS] poll test passed\n");

    close(fd);
    return 0;
}
