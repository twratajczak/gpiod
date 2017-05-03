CC=mipsel-openwrt-linux-gcc

gpiod: export STAGING_DIR=$(HOME)/Documents/onion/lede/staging_dir/target-mipsel_24kc_musl/
gpiod: gpiod.c
	$(CC) -lcurl -luci $^ -o $@
	scp $@ onion:

