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

CURL *curl_ctx = NULL;
int curl_errors;

char url[1024] = "";
int poll_timeout_ms = 1000 * 60 * 15;

struct rule {
	short num;
	char *cmd_rise;
	char *cmd_fall;
	struct rule *prev;
} *rules = NULL, *rule;

int setup_config() {
	struct uci_ptr uci_ptr;
	struct uci_context *uci_ctx;
	struct uci_element *uci_element;
	struct uci_package *uci_package;
	struct uci_section *uci_section;
	char uci_path[256];

	uci_ctx = uci_alloc_context();
	if (!uci_ctx) {
		fprintf(stderr, "Failed to alloc UCI ctx\n");
		return 1;
	}

	strcpy(uci_path, "gpiod.@gpiod[0].url");
	uci_lookup_ptr(uci_ctx, &uci_ptr, uci_path, true);
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

	uci_package = uci_lookup_package(uci_ctx, "gpiod");
	if (uci_package) {
		uci_foreach_element(&uci_package->sections, uci_element)
			if (UCI_TYPE_SECTION == uci_element->type) {
				uci_section = uci_lookup_section(uci_ctx, uci_package, uci_element->name);
				if (uci_section && !strcmp("rule", uci_section->type)) {
					rule = NULL;
					int node = atoi(uci_lookup_option_string(uci_ctx, uci_section, "node"));
					for (i = 0; i < gpio_num; ++i)
						if (node == gpio_map[i]) {
							rule = malloc(sizeof(struct rule));
							rule->prev = rules;
							rule->num = i;
							rule->cmd_rise = strdup(uci_lookup_option_string(uci_ctx, uci_section, "cmd_rise"));
							rule->cmd_fall = strdup(uci_lookup_option_string(uci_ctx, uci_section, "cmd_fall"));
							rules = rule;
						}
					if (!rule) {
						fprintf(stderr, "Failed to parse rule %d (gpiod.%s). Is node %d listed as monitored?\n", i, uci_element->name, node);
						uci_free_context(uci_ctx);
						return 2;
					}
				}
			}
	}

	uci_free_context(uci_ctx);
	return 0;
}

void cleanup_config() {
	for (; rules; rules = rule) {
		rule = rules->prev;
		free(rules->cmd_rise);
		free(rules->cmd_fall);
		free(rules);
	}
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
	if (!*url)
		return 0;
	curl_ctx = curl_easy_init();
	if (!curl_ctx)
		return 1;
	curl_easy_setopt(curl_ctx, CURLOPT_URL, url);
	curl_errors = 0;
	return 0;
}

void cleanup_curl() {
	if (!curl_ctx)
		return;
	curl_easy_cleanup(curl_ctx);
}

void submit() {
	if (!curl_ctx)
		return;

	static CURLcode curl_res;

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

void process() {
	for (rule = rules; rule; rule = rule->prev)
		if (POLLPRI & fds[rule->num].revents) {
			if (rule->cmd_rise && *rule->cmd_rise && '1' == values[rule->num])
				system(rule->cmd_rise);
			if (rule->cmd_fall && *rule->cmd_fall && '0' == values[rule->num])
				system(rule->cmd_fall);
		}
}

int main(int argc, char **argv) {
	if (setup_config())
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
		process();
		poll(fds, gpio_num, poll_timeout_ms);
	}


	for (i = 0; i < gpio_num; ++i)
		cleanup_gpio(i);

	cleanup_curl();

	cleanup_config();

	return 0;
}

