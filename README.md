# U-Boot Davinci Bootcounter

U-Boot implements a [boot count
scheme](http://www.denx.de/wiki/view/DULG/UBootBootCountLimit) that can be
used to detect multiple failed attempts to boot Linux.  On Davinci (TI AM
33xx) the bootcount is stored in the `RTC SCRATCH2` register.  However there's
no way to read or write this register from userspace.  This tool provides a
means to read and write the bootcount value.


# Usage

If invoked without any args, bootcount will read and print the current value
to `stdout`.  When passing the `-r` flag, it will reset the bootcount value
```
~ # bootcount
42
~ # bootcount -r     # <-- prints nothing to stdout, exit code 0 means success
~ # bootcount
0
~ #
```

# Building

Assuming you're doing a cross-build from x86 host to ARM target:
```
./autogen.sh
./configure --host=arm-linux-gnueabihf --prefix=/usr
make
make install DESTDIR=$LOCATION_OF_CHROOT
```

# Further Reading

* http://www.denx.de/wiki/view/DULG/UBootBootCountLimit
* http://git.ti.com/ti-u-boot/ti-u-boot/blobs/master/drivers/bootcount/bootcount_davinci.c

