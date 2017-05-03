CC=mipsel-openwrt-linux-gcc

gpio: export STAGING_DIR=$(HOME)/Documents/onion/lede/staging_dir/target-mipsel_24kc_musl/
gpio: gpio.c
	$(CC) -lcurl -luci $^ -o $@
	scp $@ onion:

