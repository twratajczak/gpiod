#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <curl/curl.h>
#include <string.h>

#define MAX_NUM 16
int num = 0;
int num_map[MAX_NUM];

int i, len;
char buf[2048];
FILE *files[MAX_NUM];
struct pollfd fds[MAX_NUM] = {};
char values[MAX_NUM];
int errors = 0;

CURL *ch;
CURLcode res;

char *url_template = "http://example.com?k=%s&e=%d&gpio=";
char *url_key = "auth_key";
int poll_ms = 1000 * 60 * 15;

void setup(int i) {
	FILE *export = fopen("/sys/class/gpio/export", "w");
	fprintf(export, "%d", num_map[i]);
	fclose(export);

	sprintf(buf, "/sys/class/gpio/gpio%d/direction", num_map[i]);
	FILE *direction = fopen(buf, "w");
	fprintf(direction, "in");
	fclose(direction);
	sprintf(buf, "/sys/class/gpio/gpio%d/edge", num_map[i]);
	FILE *edge = fopen(buf, "w");
	fprintf(edge, "both");
	fclose(edge);

	sprintf(buf, "/sys/class/gpio/gpio%d/value", num_map[i]);
	files[i] = fopen(buf, "r");
	fds[i].fd = fileno(files[i]);
	fds[i].events = POLLPRI | POLLERR;
}

void submit() {
	sprintf(buf, url_template, url_key, errors);
	len = strlen(buf);
	for (i = 0; i < num; ++i) {
		buf[len++] = values[i];
		buf[len++] = ',';
	}
	buf[len] = '\0';
	ch = curl_easy_init();
	curl_easy_setopt(ch, CURLOPT_URL, buf);
	res = curl_easy_perform(ch);
	if (res != CURLE_OK)
		++errors;
	else
		errors = 0;
	curl_easy_cleanup(ch);
}

int main(int argc, char **argv) {
	num_map[0] = 0;
	num_map[1] = 1;
	num_map[2] = 2;
	num_map[3] = 3;
	num = 4;

	for (i = 0; i < num; ++i)
		setup(i);

	while (1) {
		for (i = 0; i < num; ++i) {
			fseek(files[i], 0, SEEK_SET);
			fread(&values[i], sizeof(values[i]), 1, files[i]);
		}
		submit();
		poll(fds, num, poll_ms);
	}

	for (i = 0; i < num; ++i)
		fclose(files[i]);

	return 0;
}

