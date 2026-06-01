#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "phantomfpga_uapi.h"

static int expect_set_cfg_fail(int fd, struct phantomfpga_config *cfg, const char *name)
{
    errno = 0;

    printf("[TEST] invalid config: %s\n", name);

    int rc = ioctl(fd, PHANTOMFPGA_IOCTL_SET_CFG, cfg);
    if (rc == 0) {
        printf("[FAIL] SET_CFG accepted invalid config: %s\n", name);
        return -1;
    }

    printf("[OK] rejected %s: errno=%d (%s)\n",
           name, errno, strerror(errno));

    if (errno != EINVAL) {
        printf("[WARN] expected EINVAL, got errno=%d\n", errno);
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

    struct phantomfpga_config cfg;
    memset(&cfg, 0, sizeof(cfg));

    /*
     * בסיס חוקי.
     */
    cfg.desc_count = 256;
    cfg.frame_rate = 5;
    cfg.irq_coalesce_count = 1;
    cfg.irq_coalesce_timeout = 40000;

    /*
     * frame_rate לא חוקי: 0.
     */
    struct phantomfpga_config bad_rate_zero = cfg;
    bad_rate_zero.frame_rate = 0;

    if (expect_set_cfg_fail(fd, &bad_rate_zero, "frame_rate = 0") < 0) {
        close(fd);
        return 1;
    }

    /*
     * frame_rate לא חוקי: מעל 60.
     */
    struct phantomfpga_config bad_rate_high = cfg;
    bad_rate_high.frame_rate = 61;

    if (expect_set_cfg_fail(fd, &bad_rate_high, "frame_rate = 61") < 0) {
        close(fd);
        return 1;
    }

    /*
     * desc_count לא power of 2.
     */
    struct phantomfpga_config bad_desc_not_power2 = cfg;
    bad_desc_not_power2.desc_count = 300;

    if (expect_set_cfg_fail(fd, &bad_desc_not_power2, "desc_count = 300") < 0) {
        close(fd);
        return 1;
    }

    /*
     * desc_count קטן מדי.
     */
    struct phantomfpga_config bad_desc_small = cfg;
    bad_desc_small.desc_count = 2;

    if (expect_set_cfg_fail(fd, &bad_desc_small, "desc_count = 2") < 0) {
        close(fd);
        return 1;
    }

    /*
     * עכשיו לוודא שקונפיגורציה חוקית עדיין עובדת אחרי ניסיונות כושלים.
     */
    printf("[TEST] valid config after invalid attempts\n");

    if (ioctl(fd, PHANTOMFPGA_IOCTL_SET_CFG, &cfg) < 0) {
        printf("[FAIL] valid SET_CFG failed after invalid attempts: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return 1;
    }

    printf("[OK] valid SET_CFG succeeded\n");

    printf("[PASS] invalid config test passed\n");

    close(fd);
    return 0;
}
