#include <stdlib.h>
#include <stdio.h>
#include <poll.h>

#define NUM 4

int i;
char buf[1023];
FILE *files[NUM];
struct pollfd fds[NUM] = {};
char values[NUM];

void setup(int i, int n) {
	FILE *export = fopen("/sys/class/gpio/export", "w");
	fprintf(export, "%d", n);
	fclose(export);

	sprintf(buf, "/sys/class/gpio/gpio%d/direction", n);
	FILE *direction = fopen(buf, "w");
	fprintf(direction, "in");
	fclose(direction);
	sprintf(buf, "/sys/class/gpio/gpio%d/edge", n);
	FILE *edge = fopen(buf, "w");
	fprintf(edge, "both");
	fclose(edge);

	sprintf(buf, "/sys/class/gpio/gpio%d/value", n);
	files[i] = fopen(buf, "r");
	fds[i].fd = fileno(files[i]);
	fds[i].events = POLLPRI | POLLERR;
}

int main(int argc, char **argv) {
	for (i = 0; i < NUM; ++i)
		setup(i, 0 + i);

	while (1) {
		for (i = 0; i < NUM; ++i) {
			fseek(files[i], 0, SEEK_SET);
			fread(&values[i], sizeof(values[i]), 1, files[i]);
		}
		poll(fds, NUM, 1000 * 30);
	}

	for (i = 0; i < NUM; ++i)
		fclose(files[i]);

	return 0;
}

