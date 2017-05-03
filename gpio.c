#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <curl/curl.h>
#include <string.h>
#include <uci.h>

#define MAX_NUM 16
int num = 0;
int num_map[MAX_NUM];

int i, len;
char buf[4096];
FILE *files[MAX_NUM];
struct pollfd fds[MAX_NUM] = {};
char values[MAX_NUM];
int errors = 0;

CURL *ch;
CURLcode res;

char *url_template = "%s&e=%d&gpio=";
char url[1024] = "";
int poll_timeout_ms = 0;

struct uci_ptr uci_ptr;
struct uci_context *uci_ctx;
char uci_path[256];
char *uci_buf;

int setup_config() {
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
		poll_timeout_ms = 1000 * 60 * 15;

	strcpy(uci_path, "gpiod.@gpiod[0].nodes");
	uci_lookup_ptr(uci_ctx, &uci_ptr, uci_path, true);
	if (uci_ptr.flags & UCI_LOOKUP_COMPLETE) {
		strncpy(buf, uci_ptr.o->v.string, 1023);
		buf[1024] = '\0';
		for (uci_buf = strtok(buf, " "); uci_buf; uci_buf = strtok(NULL, " "))
			num_map[num++] = atoi(uci_buf);
	}

	uci_free_context(uci_ctx);
	return 0;
}

void setup_gpio(int i) {
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
	sprintf(buf, url_template, url, errors);
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
	if (setup_config())
		return 1;

	for (i = 0; i < num; ++i)
		setup_gpio(i);

	while (num) {
		for (i = 0; i < num; ++i) {
			fseek(files[i], 0, SEEK_SET);
			fread(&values[i], sizeof(values[i]), 1, files[i]);
		}
		submit();
		poll(fds, num, poll_timeout_ms);
	}

	for (i = 0; i < num; ++i)
		fclose(files[i]);

	return 0;
}

