#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "phantomfpga_uapi.h"

static uint32_t read_le32(const unsigned char *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
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

    printf("[TEST] poll before mmap frame #%d, timeout=3000ms\n", frame_index);

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

static void print_preview(const unsigned char *frame)
{
    printf("  preview: \"");

    for (int i = 16; i < 16 + 40; i++) {
        unsigned char c = frame[i];

        if (c >= 32 && c <= 126) {
            putchar(c);
        } else {
            putchar('.');
        }
    }

    printf("\"\n");
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
     * Safety: stop previous state if any.
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

    struct phantomfpga_buffer_info info;
    memset(&info, 0, sizeof(info));

    printf("[TEST] GET_BUFFER_INFO\n");

    if (ioctl(fd, PHANTOMFPGA_IOCTL_GET_BUFFER_INFO, &info) < 0) {
        printf("[FAIL] GET_BUFFER_INFO failed: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return 1;
    }

    printf("[OK] GET_BUFFER_INFO succeeded\n");
    printf("  buffer_size  = %llu\n", (unsigned long long)info.buffer_size);
    printf("  buffer_count = %llu\n", (unsigned long long)info.buffer_count);
    printf("  total_size   = %llu\n", (unsigned long long)info.total_size);
    printf("  frame_size   = %u\n", info.frame_size);

    if (info.frame_size != PHANTOMFPGA_FRAME_SIZE) {
        printf("[FAIL] unexpected frame_size=%u expected=%u\n",
               info.frame_size, PHANTOMFPGA_FRAME_SIZE);
        close(fd);
        return 1;
    }

    if (info.buffer_size < PHANTOMFPGA_FRAME_SIZE) {
        printf("[FAIL] buffer_size too small: %llu\n",
               (unsigned long long)info.buffer_size);
        close(fd);
        return 1;
    }

    if (info.buffer_count == 0) {
        printf("[FAIL] buffer_count is zero\n");
        close(fd);
        return 1;
    }

    long page_size_long = sysconf(_SC_PAGESIZE);
    if (page_size_long <= 0) {
        printf("[FAIL] sysconf(_SC_PAGESIZE) failed\n");
        close(fd);
        return 1;
    }

    size_t page_size = (size_t)page_size_long;
    size_t stride = align_up((size_t)info.buffer_size, page_size);

    /*
     * The driver maps buffers with PAGE_ALIGN(buffer_size) stride.
     * So userspace should use the same stride between buffers.
     */
    size_t mmap_size = stride * (size_t)info.buffer_count;

    printf("[INFO] page_size = %zu\n", page_size);
    printf("[INFO] stride    = %zu\n", stride);
    printf("[INFO] mmap_size = %zu\n", mmap_size);

    printf("[TEST] mmap\n");

    unsigned char *map = mmap(NULL,
                              mmap_size,
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED,
                              fd,
                              0);

    if (map == MAP_FAILED) {
        printf("[FAIL] mmap failed: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return 1;
    }

    printf("[OK] mmap succeeded, address=%p\n", (void *)map);

    printf("[TEST] START\n");

    if (ioctl(fd, PHANTOMFPGA_IOCTL_START) < 0) {
        printf("[FAIL] START failed: errno=%d (%s)\n",
               errno, strerror(errno));
        munmap(map, mmap_size);
        close(fd);
        return 1;
    }

    printf("[OK] START succeeded\n");

    /*
     * We expect frames to complete in descriptor order:
     * descriptor 0, 1, 2, ...
     * because START does soft reset and ring starts cleanly.
     */
    for (int i = 0; i < 5; i++) {
        if (wait_for_frame_with_poll(fd, i) < 0) {
            stop_device(fd);
            munmap(map, mmap_size);
            close(fd);
            return 1;
        }

        size_t buffer_index = (size_t)i % (size_t)info.buffer_count;
        unsigned char *frame = map + buffer_index * stride;

        printf("[TEST] checking mmap frame #%d at buffer_index=%zu\n",
               i, buffer_index);

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
            munmap(map, mmap_size);
            close(fd);
            return 1;
        }

        if (seq >= PHANTOMFPGA_FRAME_COUNT) {
            printf("[FAIL] bad sequence, expected 0..%u\n",
                   PHANTOMFPGA_FRAME_COUNT - 1);
            stop_device(fd);
            munmap(map, mmap_size);
            close(fd);
            return 1;
        }

        print_preview(frame);

        printf("[TEST] CONSUME_FRAME for frame #%d\n", i);

        if (ioctl(fd, PHANTOMFPGA_IOCTL_CONSUME_FRAME) < 0) {
            printf("[FAIL] CONSUME_FRAME failed: errno=%d (%s)\n",
                   errno, strerror(errno));
            stop_device(fd);
            munmap(map, mmap_size);
            close(fd);
            return 1;
        }

        printf("[OK] CONSUME_FRAME succeeded\n");
    }

    stop_device(fd);

    printf("[TEST] munmap\n");

    if (munmap(map, mmap_size) < 0) {
        printf("[FAIL] munmap failed: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return 1;
    }

    printf("[OK] munmap succeeded\n");

    close(fd);

    printf("[PASS] mmap test passed\n");
    return 0;
}
