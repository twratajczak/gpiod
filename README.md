# gpiod
Trivial GPIO monitor for LEDE/OpenWRT, tested with Onion Omega2+.

# Usage
Compile and link with *UCI* and *cURL* (`-luci -lcurl`), configure in `/etc/config/gpiod`

```
config gpiod
	option url 'http://example.com?key=auth_key'
	option timeout '900'
	option nodes '0 1 2 3'
```

Run at startup i.e. with rc.local `/sbin/start-stop-daemon -b -S -x /root/gpiod`

# Configuration
- url - where to POST GPIO values
- timeout - poll() timeout in seconds, -1 to disable
- nodes - space separated list of GPIO numbers

