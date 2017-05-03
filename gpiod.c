#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <curl/curl.h>
#include <string.h>
#include <uci.h>

#define MAX_NUM 16
int gpio_num = 0;
int gpio_map[MAX_NUM];

int i, len;
char buf[4096], *bufp;
FILE *files[MAX_NUM];
struct pollfd fds[MAX_NUM] = {};
char values[MAX_NUM];

CURL *curl_ctx;
CURLcode curl_res;
int curl_errors;

char url[1024] = "";
int poll_timeout_ms = 1000 * 60 * 15;

struct uci_ptr uci_ptr;
struct uci_context *uci_ctx;
char uci_path[256];

int process_config() {
	uci_ctx = uci_alloc_context();
	if (!uci_ctx) {
		fprintf(stderr, "Failed to alloc UCI ctx\n");
		return 1;
	}

	strcpy(uci_path, "gpiod.@gpiod[0].url");
	if ((uci_lookup_ptr(uci_ctx, &uci_ptr, uci_path, true) != UCI_OK) || !uci_ptr.o || !uci_ptr.o->v.string) {
		fprintf(stderr, "Failed to lookup '%s'\n", uci_path);
		uci_free_context(uci_ctx);
		return 2;
	}
	if (uci_ptr.flags & UCI_LOOKUP_COMPLETE)
		strncpy(url, uci_ptr.o->v.string, 1023);

	strcpy(uci_path, "gpiod.@gpiod[0].timeout");
	uci_lookup_ptr(uci_ctx, &uci_ptr, uci_path, true);
	if (uci_ptr.flags & UCI_LOOKUP_COMPLETE)
		poll_timeout_ms = 1000 * atoi(uci_ptr.o->v.string);
	if (!poll_timeout_ms)
		poll_timeout_ms = -1;

	strcpy(uci_path, "gpiod.@gpiod[0].nodes");
	uci_lookup_ptr(uci_ctx, &uci_ptr, uci_path, true);
	if (uci_ptr.flags & UCI_LOOKUP_COMPLETE) {
		strncpy(buf, uci_ptr.o->v.string, 1023);
		buf[1024] = '\0';
		for (bufp = strtok(buf, " "); bufp; bufp = strtok(NULL, " "))
			gpio_map[gpio_num++] = atoi(bufp);
	}

	uci_free_context(uci_ctx);
	return 0;
}

void setup_gpio(int i) {
	FILE *export = fopen("/sys/class/gpio/export", "w");
	fprintf(export, "%d", gpio_map[i]);
	fclose(export);

	sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio_map[i]);
	FILE *direction = fopen(buf, "w");
	fprintf(direction, "in");
	fclose(direction);
	sprintf(buf, "/sys/class/gpio/gpio%d/edge", gpio_map[i]);
	FILE *edge = fopen(buf, "w");
	fprintf(edge, "both");
	fclose(edge);

	sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio_map[i]);
	files[i] = fopen(buf, "r");
	fds[i].fd = fileno(files[i]);
	fds[i].events = POLLPRI | POLLERR;
}

void cleanup_gpio(int i) {
	FILE *unexport = fopen("/sys/class/gpio/unexport", "w");
	fprintf(unexport, "%d", gpio_map[i]);
	fclose(unexport);
	fclose(files[i]);
}

int setup_curl() {
	curl_ctx = curl_easy_init();
	if (!curl_ctx)
		return 1;
	curl_easy_setopt(curl_ctx, CURLOPT_URL, url);
	curl_errors = 0;
	return 0;
}

void cleanup_curl() {
	curl_easy_cleanup(curl_ctx);
}

void submit() {
	bufp = buf;
	sprintf(bufp, "e=%d", curl_errors);
	bufp += strlen(bufp);

	for (i = 0; i < gpio_num; ++i) {
		sprintf(bufp, "&gpio%d=%c", gpio_map[i], values[i]);
		bufp += strlen(bufp);
	}
	curl_easy_setopt(curl_ctx, CURLOPT_POSTFIELDS, buf);
	curl_res = curl_easy_perform(curl_ctx);
	if (curl_res != CURLE_OK)
		++curl_errors;
	else
		curl_errors = 0;
}

int main(int argc, char **argv) {
	if (process_config())
		return 1;

	if (setup_curl())
		return 2;

	for (i = 0; i < gpio_num; ++i)
		setup_gpio(i);

	while (gpio_num) {
		for (i = 0; i < gpio_num; ++i) {
			fseek(files[i], 0, SEEK_SET);
			fread(&values[i], sizeof(values[i]), 1, files[i]);
		}
		submit();
		poll(fds, gpio_num, poll_timeout_ms);
	}

	cleanup_curl();

	for (i = 0; i < gpio_num; ++i)
		cleanup_gpio(i);

	return 0;
}

