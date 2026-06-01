#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "phantomfpga_uapi.h"

static void print_cfg(const char *title, const struct phantomfpga_config *cfg)
{
    printf("%s\n", title);
    printf("  desc_count           = %u\n", (unsigned)cfg->desc_count);
    printf("  frame_rate           = %u\n", (unsigned)cfg->frame_rate);
    printf("  irq_coalesce_count   = %u\n", (unsigned)cfg->irq_coalesce_count);
    printf("  irq_coalesce_timeout = %u\n", (unsigned)cfg->irq_coalesce_timeout);
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

    struct phantomfpga_config before;
    memset(&before, 0, sizeof(before));

    printf("[TEST] ioctl GET_CFG before SET_CFG\n");

    if (ioctl(fd, PHANTOMFPGA_IOCTL_GET_CFG, &before) < 0)
    {
        printf("[FAIL] GET_CFG before SET_CFG failed: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return 1;
    }

    print_cfg("[OK] current config:", &before);

    struct phantomfpga_config cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.desc_count = 256;
    cfg.frame_rate = 5;
    cfg.irq_coalesce_count = 1;
    cfg.irq_coalesce_timeout = 40000;

    print_cfg("[TEST] setting config:", &cfg);

    if (ioctl(fd, PHANTOMFPGA_IOCTL_SET_CFG, &cfg) < 0)
    {
        printf("[FAIL] SET_CFG failed: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return 1;
    }

    printf("[OK] SET_CFG succeeded\n");

    struct phantomfpga_config after;
    memset(&after, 0, sizeof(after));

    printf("[TEST] ioctl GET_CFG after SET_CFG\n");

    if (ioctl(fd, PHANTOMFPGA_IOCTL_GET_CFG, &after) < 0)
    {
        printf("[FAIL] GET_CFG after SET_CFG failed: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return 1;
    }

    print_cfg("[OK] config after SET_CFG:", &after);

    if (after.desc_count != cfg.desc_count ||
        after.frame_rate != cfg.frame_rate ||
        after.irq_coalesce_count != cfg.irq_coalesce_count ||
        after.irq_coalesce_timeout != cfg.irq_coalesce_timeout)
    {
        printf("[FAIL] config mismatch after SET_CFG\n");
        close(fd);
        return 1;
    }

    printf("[PASS] ioctl SET_CFG + GET_CFG test passed\n");

    if (close(fd) < 0)
    {
        printf("[FAIL] close failed: errno=%d (%s)\n",
               errno, strerror(errno));
        return 1;
    }

    return 0;
}
