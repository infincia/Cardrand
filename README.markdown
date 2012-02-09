###cardrand.c

A simple wrapper around libopensc for retrieving random data from a smart card HWRNG and feeding it into the kernel random pool using an ioctl. This makes it available to any application using /dev/random


###What does this do with the smart card?

Essentially, it does the following, but in code using library functions:

	[steve@Plugbox ~]$ sudo opensc-explorer
	OpenSC Explorer version 0.11.13
	Using reader with a card: Aladdin eToken PRO
	OpenSC [3F00]> random
	Usage: random count
	OpenSC [3F00]> random 128
	00000000: 60 CA DF 5A DD 1D 16 17 1C 98 98 88 76 1B 6E 5D `..Z........v.n]
	00000010: 4B 53 E3 47 9B EE 3E 2D FF 20 0D 7D FF F7 55 57 KS.G..>-. .}..UW
	00000020: F9 C4 21 F1 88 F1 C5 3C EB 37 6D 3C B4 E7 F8 AD ..!....<.7m<....
	00000030: 6D 5C 96 BE A1 FC 7E AF 7C BB 21 B8 9E 3E 50 45 m\....~.|.!..>PE
	00000040: DC 6E 88 1C 90 B5 46 95 7D 9E 73 4F DA FE 52 58 .n....F.}.sO..RX
	00000050: 9F F7 08 1B 71 50 AC F3 BC BB 10 21 40 69 FA 05 ....qP.....!@i..
	00000060: C9 73 47 51 98 EA A8 96 5A 39 88 F8 B6 09 75 9A .sGQ....Z9....u.
	00000070: 5A FC E7 49 FC 01 A3 D1 E7 C8 C5 4F 57 FA 8B 0C Z..I.......OW...


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
