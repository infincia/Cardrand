###cardrand.c

A simple wrapper around libopensc for retrieving random data from a smart card HWRNG and feeding it into the kernel random pool using an ioctl. This makes it available to any application using /dev/random


###Compiling

To compile you will need the libopensc development files for your distro along with build tools and a WORKING smart card that is 100% functional in linux. If you don't already have one of those, this code won't do much for you :)

Then just compile the c file directly, while linking against libopensc:

	gcc -lopensc -o cardrand cardrand.c

###How to use

Just run the binary directly:

	./cardrand

This is really barebones code, but it does immediately daemonize itself and start feeding random data into the linux kernel using an IOCTL. This is the *only* way to do this, you can't just retrieve a sample from the card and use cat to get it into /dev/random (the kernel will discard it as unreliable data)

With cardrand running, you should be able to now check the entropy pool:

	 watch -n 1 cat /proc/sys/kernel/random/entropy_avail

The value should fluctuate, but it should always refill if it drops because the card is being used to get more.

###Why would you need this?

If you have a need for truly random data in a machine you probably already know why, but the cryptographic functions in the Linux kernel all rely on there being a "pool" of random data available for use, sometimes that pool drops to zero (which happens frequently on headless systems in my experience). The effect of an empty pool can be applications that hang or refuse to continue until more random entropy is available, and the easiest way to resolve that is to feed more random data into the pool.

Smart cards typically have a real HWRNG inside them, and the OpenSC project libraries provide an easy way to request some of it from the card. From there it was just a simple matter of hooking into the right IOCTL for the kernel :)
