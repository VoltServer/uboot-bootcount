# U-Boot Bootcount Manager

U-Boot implements a [boot count
scheme](http://www.denx.de/wiki/view/DULG/UBootBootCountLimit) that can be
used to detect multiple failed attempts to boot Linux.  On Davinci (TI AM
335x) the bootcount is stored in the `RTC SCRATCH2` register.  However there's
no way to read or write this register from userspace.  This tool provides a
means to read and write the bootcount value.

On non TI AM33xx platforms, this will look for a Device-Tree EEPROM.  The I2C bus, 
address and EEPROM offset are defined at build-time in stc/i2c_eeprom.h.

Platform detection is perfomed by looking at the value of `/proc/device-tree/soc/compatible`.


# Usage

If invoked without any args, bootcount will read and print the current value
to `stdout`.  When passing the `-r` flag, it will reset the bootcount value
```
~ # bootcount -s 42  # set value
~ # bootcount        # read present bootcount value
42
~ # bootcount -r     # Reset to 0.  Exit status 0 means success
~ # bootcount
0
~ # bootcount -f     # sets bootcount to UINT16_MAX - 1
~ # bootcount
65534

```
Set the `DEBUG` environment variable to get debugging data on stdout.  Example:
```
~ $ DEBUG=1 sudo -E bootcount -f
DEBUG: DEBUG=1
DEBUG: Read from /proc/device-tree/soc/compatible: ti,omap-infra
DEBUG: Using EEPROM? 0
DEBUG: Action=force
DEBUG: Write value 65534

```


# Development

Assuming you're doing a cross-build from x86 host to ARM target:
```
./autogen.sh
./configure --host=arm-linux-gnueabihf --prefix=/usr
make
make install DESTDIR=$LOCATION_OF_CHROOT
```

During development, periodically run `autoscan` to detect if changes should be made to `configure.ac`.

# Further Reading

* http://www.denx.de/wiki/view/DULG/UBootBootCountLimit
* http://git.ti.com/ti-u-boot/ti-u-boot/blobs/master/drivers/bootcount/bootcount_davinci.c
* https://github.com/u-boot/u-boot/blob/v2022.01/drivers/bootcount/Kconfig

