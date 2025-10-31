# U-Boot Bootcount userspace program

U-Boot supports a [boot count
scheme](http://www.denx.de/wiki/view/DULG/UBootBootCountLimit) that can be
used to detect multiple failed attempts to boot Linux.  

Some platforms have persistent registers that survive soft-resets. This tool 
provides a means to read and write the bootcount value to the register on several 
supported platforms (see below).

On platforms without register support, this will look for a Device-Tree EEPROM.  
The I2C bus, address and EEPROM offset are defined at build-time in src/i2c_eeprom.h.

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
~ # bootcount -d     # platform & method detection
Detected TI AM335x
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

## Cross-platform using Docker

There is a `Dockerfile` that can be used to build for armhf and aarch64 on non-linux hosts that support Docker Desktop.  A `docker-bake.hcl` file plus the
`build-all.sh` helper script will perform parallel multi-architecture builds using Docker Buildx.

```
./build-all.sh

```
Resulting tarballs are copied to `dist/`.

To build only one architecture:
```
docker buildx bake armhf
docker buildx bake arm64
```

Override args example:
```
docker buildx bake armhf --set armhf.args.TARGET_ARCH=arm-linux-gnueabihf
```

# Supported Platforms

For all supported chipsets below, the required u-boot config settings are set 
by default when choosing the respective build target.  For I2C EEPROM, config 
settings are listed below.

### TI AM335x

On TI AM335x, the [RTC_SCRATCH2_REG](https://www.ti.com/lit/ug/spruh73p/spruh73p.pdf) 
register is used.

Compatible platforms:
 * `ti,am33xx`

### STM32 MP15x

STM32 chipsets in the MP15x series use the [TAMP_BKP21R register](https://wiki.st.com/stm32mpu/wiki/STM32MP15_backup_registers#Boot_counter_feature).

Compatible platforms:
 * `st,stm32mp153`
 * `st,stm32mp157`

### NXP iMX8

NXP iMX8 uses the `SNVS_LP GPR0` register.

Compatible platforms:
 * `fsl,imx8mm`
 * `fsl,imx8mn`
 * `fsl,imx8mp`
 * `fsl,imx8mq`

### NXP iMX93

NXP iMX93 uses the `BBNSM GPR0` register.

Compatible platforms:
 * `fsl,imx93`

### I2C EEPROM

Chipsets without a dedicated register can use I2C EEPROM.

For I2C EEPROM, use the following u-boot config settings:
```
CONFIG_BOOTCOUNT_LIMIT=y
CONFIG_DM_BOOTCOUNT=y
CONFIG_DM_BOOTCOUNT_I2C_EEPROM=y
CONFIG_SYS_I2C_MVTWSI=y
CONFIG_MISC=y
CONFIG_I2C_EEPROM=y
CONFIG_I2C_SET_DEFAULT_BUS_NUM=y
CONFIG_I2C_DEFAULT_BUS_NUMBER=0x02
CONFIG_SYS_I2C_EEPROM_ADDR=0x50
CONFIG_SPL_I2C=y
CONFIG_CMD_EEPROM=y
CONFIG_SYS_I2C_EEPROM_BUS=2
CONFIG_SYS_EEPROM_SIZE=1024
```
This bootcount program will look for i2c EEPROM at 
`/sys/bus/i2c/devices/2-0050/eeprom`, if none of the above platforms are 
detected.  Bus and address are defined in `i2c_eeprom.h`.


# Further Reading

* http://www.denx.de/wiki/view/DULG/UBootBootCountLimit
* http://git.ti.com/ti-u-boot/ti-u-boot/blobs/master/drivers/bootcount/bootcount_davinci.c
* https://github.com/u-boot/u-boot/blob/v2022.01/drivers/bootcount/Kconfig

