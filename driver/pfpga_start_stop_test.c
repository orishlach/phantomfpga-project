#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "phantomfpga_uapi.h"

static void print_stats(const char *title, const struct phantomfpga_stats *st)
{
    printf("%s\n", title);
    printf("  frames_produced = %llu\n", (unsigned long long)st->frames_produced);
    printf("  frames_dropped  = %llu\n", (unsigned long long)st->frames_dropped);
    printf("  frames_consumed = %llu\n", (unsigned long long)st->frames_consumed);
    printf("  bytes_produced  = %llu\n", (unsigned long long)st->bytes_produced);
    printf("  bytes_consumed  = %llu\n", (unsigned long long)st->bytes_consumed);
    printf("  desc_completed  = %u\n", st->desc_completed);
    printf("  errors          = %u\n", st->errors);
    printf("  crc_errors      = %u\n", st->crc_errors);
    printf("  irq_count       = %u\n", st->irq_count);
    printf("  desc_head       = %u\n", st->desc_head);
    printf("  desc_tail       = %u\n", st->desc_tail);
    printf("  current_frame   = %u\n", st->current_frame);
    printf("  status          = 0x%08x\n", st->status);
}

static int get_stats(int fd, struct phantomfpga_stats *st)
{
    memset(st, 0, sizeof(*st));

    if (ioctl(fd, PHANTOMFPGA_IOCTL_GET_STATS, st) < 0) {
        printf("[FAIL] GET_STATS failed: errno=%d (%s)\n",
               errno, strerror(errno));
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
     * ליתר ביטחון: אם משהו נשאר רץ מטסט קודם,
     * ננסה לעצור. לא נכשל אם STOP לא באמת היה נחוץ.
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

    struct phantomfpga_stats before;
    struct phantomfpga_stats after_start;
    struct phantomfpga_stats after_stop;

    if (get_stats(fd, &before) < 0) {
        close(fd);
        return 1;
    }

    print_stats("[STATS] before START:", &before);

    printf("[TEST] START\n");

    if (ioctl(fd, PHANTOMFPGA_IOCTL_START) < 0) {
        printf("[FAIL] START failed: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return 1;
    }

    printf("[OK] START succeeded\n");
    printf("[TEST] sleeping 2 seconds while device streams frames...\n");

    sleep(2);

    if (get_stats(fd, &after_start) < 0) {
        ioctl(fd, PHANTOMFPGA_IOCTL_STOP);
        close(fd);
        return 1;
    }

    print_stats("[STATS] after 2 seconds:", &after_start);

    printf("[TEST] STOP\n");

    if (ioctl(fd, PHANTOMFPGA_IOCTL_STOP) < 0) {
        printf("[FAIL] STOP failed: errno=%d (%s)\n",
               errno, strerror(errno));
        close(fd);
        return 1;
    }

    printf("[OK] STOP succeeded\n");

    if (get_stats(fd, &after_stop) < 0) {
        close(fd);
        return 1;
    }

    print_stats("[STATS] after STOP:", &after_stop);

    /*
     * בדיקות בסיסיות:
     * ב-5fps במשך 2 שניות אנחנו מצפים שהמכשיר יפיק בערך כמה frames.
     * לא חייב להיות מספר מדויק, אבל הוא צריך לעלות.
     */
    if (after_start.frames_produced <= before.frames_produced &&
        after_start.desc_completed <= before.desc_completed) {
        printf("[FAIL] no progress detected after START\n");
        close(fd);
        return 1;
    }

    if (after_start.errors != 0) {
        printf("[FAIL] device reported errors=%u\n", after_start.errors);
        close(fd);
        return 1;
    }

    printf("[PASS] START / STOP / STATS test passed\n");

    close(fd);
    return 0;
}
