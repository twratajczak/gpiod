CC=mipsel-openwrt-linux-gcc

gpio: export STAGING_DIR=$(HOME)/Documents/onion/lede-toolchain-ramips-mt7688_gcc-5.4.0_musl.Linux-x86_64/toolchain-mipsel_24kc_gcc-5.4.0_musl/
gpio: gpio.c
	$(CC) $^ -o $@
	scp $@ onion:

