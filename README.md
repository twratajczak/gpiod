# gpiod
Trivial GPIO monitor for LEDE/OpenWRT, tested with Onion Omega2+.
Allows posting values to URL and calling shell commands.

# Usage
Compile and link with *UCI* and *cURL* (`-luci -lcurl`), configure in `/etc/config/gpiod`

```
config gpiod
	option url 'http://example.com?key=auth_key'
	option timeout '900'
	option nodes '0 1 2 3'

config rule
	option node 1
	option cmd_rise "/etc/init.d/mjpg-streamer start"
	option cmd_fall "/etc/init.d/mjpg-streamer stop"
```

Run at startup i.e. with rc.local `/sbin/start-stop-daemon -b -S -x /root/gpiod`
Multiple rules are supported. Rule commands are called with `system()`

# Configuration
## gpiod
- url (optional) - where to POST GPIO values
- timeout - poll() timeout in seconds, -1 to disable
- nodes - space separated list of GPIO numbers
## rule (optional)
- node - single GPIO number that rule applies to
- cmd_rise (optional) - called when value changes to 1
- cmd_fall (optional) - called when value changes to 0

