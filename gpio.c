#include <stdio.h>
#include <poll.h>

int main(int argc, char **argv) {
	FILE *export = fopen("/sys/class/gpio/export", "w");
	fprintf(export, "19");
	fclose(export);

	FILE *direction = fopen("/sys/class/gpio/gpio19/direction", "w");
	fprintf(direction, "in");
	fclose(direction);
	FILE *edge = fopen("/sys/class/gpio/gpio19/edge", "w");
	fprintf(edge, "both");
	fclose(edge);

	FILE *value = fopen("/sys/class/gpio/gpio19/value", "r");
	struct pollfd pfd;
	pfd.fd = fileno(value);
	pfd.events = POLLPRI | POLLERR;

	while (1) {
		char buf;
		fseek(value, 0, SEEK_SET);
		fread(&buf, sizeof(buf), 1, value);
		printf("value: %c\n", buf);
		int p = poll(&pfd, 1, 1000 * 15);
		printf("poll: %d\n", p);
	}

	fclose(value);

	return 0;
}

