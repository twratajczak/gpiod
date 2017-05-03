#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <curl/curl.h>
#include <string.h>

#define NUM 4

int i, len;
char buf[1023];
FILE *files[NUM];
struct pollfd fds[NUM] = {};
char values[NUM];

CURL *ch;
CURLcode res;

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

void submit() {
	strcpy(buf, "http://example.com?gpio=");
	len = strlen(buf);
	for (i = 0; i < NUM; ++i) {
		buf[len++] = values[i];
		buf[len++] = ',';
	}
	buf[len] = '\0';
	ch = curl_easy_init();
	curl_easy_setopt(ch, CURLOPT_URL, buf);
	res = curl_easy_perform(ch);
	if (res != CURLE_OK)
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
	curl_easy_cleanup(ch);
}

int main(int argc, char **argv) {
	for (i = 0; i < NUM; ++i)
		setup(i, 0 + i);

	while (1) {
		for (i = 0; i < NUM; ++i) {
			fseek(files[i], 0, SEEK_SET);
			fread(&values[i], sizeof(values[i]), 1, files[i]);
		}
		submit();
		poll(fds, NUM, 1000 * 60 * 15);
	}

	for (i = 0; i < NUM; ++i)
		fclose(files[i]);

	return 0;
}

