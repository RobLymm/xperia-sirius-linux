// Publish one QMI service ID per QRTR socket (the in-kernel name service
// registers one server per port) and log whoever contacts any of them.
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/qrtr.h>
#include <libqrtr.h>

#define MAXS 700
int main(int argc, char **argv)
{
	int lo = argc > 1 ? atoi(argv[1]) : 1, hi = argc > 2 ? atoi(argv[2]) : 600;
	int secs = argc > 3 ? atoi(argv[3]) : 200;
	int skip[] = {14, 227, 228, 229, 4096, 43, 256, 15, 24, 23, 22, 36, 34, 41, 17, 13, 16, 42, 769};
	static struct pollfd pfd[MAXS]; static int ids[MAXS]; int n = 0;
	for (int id = lo; id <= hi && n < MAXS; id++) {
		int s = 0;
		for (unsigned k = 0; k < sizeof(skip)/sizeof(skip[0]); k++) if (skip[k] == id) s = 1;
		if (s) continue;
		int fd = qrtr_open(0);
		if (fd < 0) { perror("qrtr_open"); break; }
		int r = -1;
		for (int t = 0; t < 50 && r; t++) { r = qrtr_publish(fd, id, 1, 0); if (r) usleep(5000); }
		if (r) { close(fd); continue; }
		pfd[n].fd = fd; pfd[n].events = POLLIN; ids[n] = id; n++;
		usleep(1000);
	}
	fprintf(stderr, "published %d services in [%d,%d] on %d sockets, listening %d s\n", n, lo, hi, n, secs);
	time_t end = time(NULL) + secs; unsigned char buf[4096];
	while (time(NULL) < end) {
		if (poll(pfd, n, 1000) <= 0) continue;
		for (int i = 0; i < n; i++) {
			if (!(pfd[i].revents & POLLIN)) continue;
			struct sockaddr_qrtr sq; socklen_t sl = sizeof(sq);
			int len = recvfrom(pfd[i].fd, buf, sizeof(buf), 0, (struct sockaddr *)&sq, &sl);
			if (len < 0 || sq.sq_port == QRTR_PORT_CTRL) continue;
			printf("%ld service %d contacted from node %u port %u len %d:", (long)time(NULL), ids[i], sq.sq_node, sq.sq_port, len);
			for (int k = 0; k < len && k < 32; k++) printf(" %02x", buf[k]);
			printf("\n"); fflush(stdout);
		}
	}
	return 0;
}
