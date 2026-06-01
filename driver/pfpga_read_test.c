#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "phantomfpga_uapi.h"

static volatile sig_atomic_t g_timed_out = 0;

static void alarm_handler(int sig)
{
    (void)sig;
    g_timed_out = 1;
}

static uint32_t read_le32(const unsigned char *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void print_frame_preview(const unsigned char *frame)
{
    printf("  preview: \"");

    /*
     * לפי ה-UAPI:
     * offset 0x0000: header, 16 bytes
     * offset 0x0010: ASCII data
     */
    for (int i = 16; i < 16 + 40; i++)
    {
        unsigned char c = frame[i];

        if (c >= 32 && c <= 126)
        {
            putchar(c);
        }
        else
        {
            putchar('.');
        }
    }

    printf("\"\n");
}

static int stop_device(int fd)
{
    if (ioctl(fd, PHANTOMFPGA_IOCTL_STOP) < 0)
    {
        printf("[WARN] STOP failed: errno=%d (%s)\n",
               errno, strerror(errno));
        return -1;
    }

    printf("[OK] STOP succeeded\n");
    return 0;
}

int main(void)
{
    const char *path = "/dev/phantomfpga0";

    printf("[TEST] opening %s\n", path);

    int fd = open(path, O_RDWR);
    if (fd < 0)
    {
        printf("[FAIL] open failed: errno=%d (%s)\n",
               errno, strerror(errno));
        return 1;
    }

    printf("[OK] open succeeded, fd=%d\n", fd);

    ioctl(fd, PHANTOMFPGA_IOCTL_STOP);

    struct phantomfpga_config cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.desc_count = 256;
    cfg.frame_rate = 5;
    cfg.irq_coalesce_count = 1;
    cfg.irq_coalesce_timeout = 40000;

    printf("[TEST] SET_CFG\n");

    if (ioctl(fd, PHANTOMFPGA_IOCTL_SET_CFG, &cfg) < 0)
    {
        printf("[FAIL] SET_CFG failed: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return 1;
    }

    printf("[OK] SET_CFG succeeded\n");

    printf("[TEST] START\n");

    if (ioctl(fd, PHANTOMFPGA_IOCTL_START) < 0)
    {
        printf("[FAIL] START failed: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return 1;
    }

    printf("[OK] START succeeded\n");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;

    if (sigaction(SIGALRM, &sa, NULL) < 0)
    {
        printf("[FAIL] sigaction failed: errno=%d (%s)\n",
               errno, strerror(errno));
        stop_device(fd);
        close(fd);
        return 1;
    }

    unsigned char frame[PHANTOMFPGA_FRAME_SIZE];

    for (int i = 0; i < 5; i++)
    {
        memset(frame, 0, sizeof(frame));
        g_timed_out = 0;

        printf("[TEST] read frame #%d\n", i);

        alarm(5);
        ssize_t n = read(fd, frame, sizeof(frame));
        alarm(0);

        if (n < 0)
        {
            if (g_timed_out)
            {
                printf("[FAIL] read timed out after 5 seconds\n");
            }
            else
            {
                printf("[FAIL] read failed: errno=%d (%s)\n",
                       errno, strerror(errno));
            }

            stop_device(fd);
            close(fd);
            return 1;
        }

        printf("[OK] read returned %zd bytes\n", n);

        if (n != PHANTOMFPGA_FRAME_SIZE)
        {
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

        if (magic != PHANTOMFPGA_FRAME_MAGIC)
        {
            printf("[FAIL] bad magic, expected 0x%08x\n",
                   PHANTOMFPGA_FRAME_MAGIC);
            stop_device(fd);
            close(fd);
            return 1;
        }

        if (seq >= PHANTOMFPGA_FRAME_COUNT)
        {
            printf("[FAIL] bad sequence, expected 0..%u\n",
                   PHANTOMFPGA_FRAME_COUNT - 1);
            stop_device(fd);
            close(fd);
            return 1;
        }

        print_frame_preview(frame);
    }

    stop_device(fd);

    printf("[PASS] read frame test passed\n");

    close(fd);
    return 0;
}
