// Measure the frequency of a square wave on an msm8974 TLMM pad by timing its
// rising edges through /dev/mem. Preemption-tolerant: an over-long interval
// counts as round(interval / median) periods, so the rate is total periods
// over total time, not edges seen.
//   padclock <gpio> <seconds>
#include <fcntl.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define TLMM 0xfd510000UL
#define MAXE 4000000

static int cmp(const void *a, const void *b)
{ int64_t x = *(const int64_t *)a, y = *(const int64_t *)b; return x < y ? -1 : x > y; }

int main(int argc, char **argv)
{
	int gpio = argc > 1 ? atoi(argv[1]) : 80;
	double secs = argc > 2 ? atof(argv[2]) : 4;
	int fd = open("/dev/mem", O_RDONLY | O_SYNC);
	if (fd < 0) { perror("/dev/mem"); return 1; }
	volatile uint8_t *base = mmap(NULL, 0x2000, PROT_READ, MAP_SHARED, fd, TLMM);
	if (base == MAP_FAILED) { perror("mmap"); return 1; }
	volatile uint32_t *in = (volatile uint32_t *)(base + 0x1004 + 0x10 * gpio);
	volatile uint32_t *cfg = (volatile uint32_t *)(base + 0x1000 + 0x10 * gpio);
	printf("gpio%d cfg=0x%08x in_out=0x%08x\n", gpio, *cfg, *in);

	struct sched_param sp = { .sched_priority = 80 };
	sched_setscheduler(0, SCHED_FIFO, &sp);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	static int64_t t[MAXE];
	long ne = 0, reads = 0;
	struct timespec ts, t0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	uint32_t prev = *in & 1;
	for (;;) {
		uint32_t v = *in & 1;
		reads++;
		if (v && !prev) {
			clock_gettime(CLOCK_MONOTONIC, &ts);
			t[ne++] = (int64_t)(ts.tv_sec - t0.tv_sec) * 1000000000LL + (ts.tv_nsec - t0.tv_nsec);
			if (ne >= MAXE) break;
			if (t[ne - 1] > secs * 1e9) break;
		} else if ((reads & 0xffff) == 0) {
			clock_gettime(CLOCK_MONOTONIC, &ts);
			if ((ts.tv_sec - t0.tv_sec) + (ts.tv_nsec - t0.tv_nsec) * 1e-9 > secs) break;
		}
		prev = v;
	}
	clock_gettime(CLOCK_MONOTONIC, &ts);
	double el = (ts.tv_sec - t0.tv_sec) + (ts.tv_nsec - t0.tv_nsec) * 1e-9;
	printf("reads %ld in %.3f s (%.2f Mreads/s), rising edges %ld\n", reads, el, reads / el / 1e6, ne);
	if (ne < 10) { printf("pad is static\n"); return 0; }
	int64_t *iv = malloc(sizeof(int64_t) * (ne - 1));
	for (long i = 1; i < ne; i++) iv[i - 1] = t[i] - t[i - 1];
	int64_t *s = malloc(sizeof(int64_t) * (ne - 1));
	memcpy(s, iv, sizeof(int64_t) * (ne - 1));
	qsort(s, ne - 1, sizeof(int64_t), cmp);
	int64_t med = s[(ne - 1) / 2];
	double periods = 0; long skipped = 0, big = 0;
	for (long i = 0; i < ne - 1; i++) {
		double k = (double)iv[i] / med;
		long r = (long)(k + 0.5);
		if (r < 1) r = 1;
		if (r > 1) { big++; skipped += r - 1; }
		periods += r;
	}
	double span = (t[ne - 1] - t[0]) * 1e-9;
	printf("interval median %lld ns (%.3f Hz), p10 %lld p90 %lld; long intervals %ld (%ld periods missed)\n",
	       (long long)med, 1e9 / med, (long long)s[(ne - 1) / 10], (long long)s[(ne - 1) * 9 / 10], big, skipped);
	printf("rate = %.0f periods / %.6f s = %.3f Hz\n", periods, span, periods / span);
	/* Clean rate: sum only intervals within +/-2% of the median (no stall, no
	 * split edge), so the estimate is the true instantaneous WS frequency. */
	double clean_t = 0; long clean_n = 0;
	for (long i = 0; i < ne - 1; i++)
		if (iv[i] > med * 0.98 && iv[i] < med * 1.02) { clean_t += iv[i]; clean_n++; }
	if (clean_n)
		printf("clean rate = %ld intervals, mean %.1f ns = %.4f Hz (%.1f%% of edges)\n",
		       clean_n, clean_t / clean_n, 1e9 / (clean_t / clean_n), 100.0 * clean_n / (ne - 1));
	return 0;
}
