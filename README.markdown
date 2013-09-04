#Deprecated!!!!!

*CardRand is old unmaintained code*, it worked well at the time it was written but I've replaced it with something more stable, more maintainable and more widely usable. You can read more about it at the [TokenTools](https://github.com/infincia/TokenTools) repository.

You can read more about why CardRand is being deprecated [here](https://github.com/infincia/TokenTools#deprecating-cardrand).

#CardRand

A simple wrapper around libopensc for retrieving random data from a smart card HWRNG and feeding it into the kernel random pool using an ioctl. This makes it available to any application using /dev/random
