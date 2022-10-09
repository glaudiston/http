http
====

In this repo I am pushing some code I have using to expose files, code and resources over http. it can be used as static web server, dynamic embeded webserver of just for learning purposes.

# Warrant: NONE. Use it at your own risk.
I have no intent to cause problems to anyone but you should not trust the code quality. For this project I am a solo developer and it probably has a lot of issues... and maybe I fix it may not. I do reserve my right to not fix or improve anything.

# Why reinvent the wheels ?
Because:
	- I want to;
	- I can;
	- I'm enjoying;
	- it makes me learn a lot and became a better developer;
	- it's funny;
	- it open possibilities;

# Why C
	C is great. I can see how things are translated to low level code; In higher level languages it's hard to understand the magic;

# known issues
	On core linux I've found the need to increase `/proc/sys/kernel/threads-max` value to avoid the error:
	`runtime/cgo: pthread_create failed: Resource temporarily unavailable`
