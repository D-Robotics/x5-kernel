Current version: v1.4.1
changes from version v1.4.0
* bugfix - resolved crash issue in insmod
changes from version v1.3.0
* support for k5.15 and k6.1
changes from version v1.2.0
* Bug fix- resolving compiler warnings
changes from version v1.1.0
* Bug fix- BPDD-4112
changes from version v1.0.0
* Bug fix- deleting unwanted node


BMI088 Generic Linux IIO driver

-----------------------------------------------
sample SPI Interface dtsi configurations
-----------------------------------------------
--------------------------------------------
.dtsi configuration:
--------------------------------------------
	bmi08xa@18 {
		compatible = "bmi08xa";
		reg = <0x18>;
		interrupt-parent = <&gpio0>;
		interrupts = <7 0>;
	};
	bmi08xg@68 {
		compatible = "bmi08xg";
		reg = <0x68>;
		interrupt-parent = <&gpio3>;
		interrupts = <20 0>;
	};
--------------------------------------------


--------------------------------------------
Pin connection
--------------------------------------------
App20							BBB pin
--------------------------------------------
1,2,9 (VDD, VDDIO, PS(GPIO5))			P9_3 (3.3v)
3,4,16 (GND,SDO1(SDO),SDO2(SDO))			P9_1 (GND)
17 (SDA)						P9_19 (SDA)
18 (SCL)						P9_20 (SCL)
21 (INT1 - ACC)					P9_42 (ACC INT)
22 (INT2 - GYRO)				P9_41 (GYR INT)
--------------------------------------------

------------------------------------------------
App30								BBB pin
------------------------------------------------
P1_1,P1_2,P2_6 (VDD,VDDIO,GPIO5)	P9_3 (3.3v)
P1_3,P2_3 (GND, SDO)				P9_1 (GND)
P2_4 (SDA)							P9_19 (SDA)
P2_2 (SCL)							P9_20 (SCL)
P1_6 (INT1 ACC/GPIO2)				P9_42 (ACC INT)
P1_7 (INT2 GRYO/GPIO3)				P9_41 (GYR INT)
------------------------------------------------
	SPI
	App Shuttle 3.0	SPI connection with Accel
------------------------------------------------
App30								BBB pin
------------------------------------------------
P1_1,P1_2 (VDD,VDDIO)				P9_3 (3.3v)
P1_3,P2_3,P2_6 (GND, GPIO5)			P9_1 (GND)
P2_4 MOSI(SDI/SDA)					P9_18 SPI0_D1
P2_2 SCLK(SCK/SCL)					P9_22 SPI0_SCLK
CSB1 (CS)							P9_17 SPI0_CS0
MISO(SDO)							P9_21 SPIO_DO
P1_6 (INT1 ACC/GPIO2)				P9_42 (ACC INT)
------------------------------------------------
------------------------------------------------
	SPI
	App Shuttle 3.0	SPI connection with Gyro
------------------------------------------------
App30								BBB pin
------------------------------------------------
P1_1,P1_2 (VDD,VDDIO)				P9_3 (3.3v)
P1_3,P2_3,P2_6 (GND, GPIO5)			P9_1 (GND)
P2_4 MOSI(SDI/SDA)					P9_30 (SPI1) - MOSI
P2_2 SCLK(SCK/SCL)					P9_31 (SPI1) - SCLK
CSB2 (GPIO4)						P9_28 (SPI1 GYR CS)
MISO(SDO)							P9_29 (SPI1) - MISO
P1_7 (INT2 GRYO/GPIO3)				P9_41 (GYR INT)
------------------------------------------------

--------------------------------------------
Node brief:
--------------------------------------------


* chip_id
----------
read only node
prints acc and gyro device chip id from register map


* driver_version
-----------------
read only node
prints driver software version


* avail_sensor
---------------
read only node
reads sensor supported (bmi088)


* temperature
--------------
read only
prints temperature raw data from sensor data register


* acc_selftest
------------
write only node
trigger sensor selftest on setting "1"


* acc_soft_reset
----------------
write only node
triggers accel sensor soft reset on setting "1"


* gyr_soft_reset
----------------
write only node
triggers gyro sensor soft reset on setting "1"


* acc_op_mode
--------------
read/write node
set/get accel power mode
Input paremeters allowed:
1-ACTIVE
0-SUSPENDED


* gyr_op_mode
--------------
read/write node
set/get gyro power mode
Input paremeters allowed:
0-SUSPENDED
1-NORMAL
2-DEEP_SUSPENDED


* acc_config
-------------
read/write node
set/get accel configuration parameters
Input : acc_odr,acc_bw,acc_range


* gyr_config
-------------
read/write node
set/get gyro configuration parameters
Input : gyr_odr,gyr_bw,gyr_range
echo 7 7 4 > gyr_config


* acc_val
----------
read only
prints accel raw x,y,z value from sensor data register


* gyr_val
----------
read only
prints gyro raw x,y,z value from sensor data register


* acc_reg_sel
--------------
read/write node
set or get accel register address and length to be read/write via acc_reg_val nodes


* acc_reg_val
--------------
read/write node
reads/write length and register address set via acc_reg_sel node


* gyr_reg_sel
--------------
read/write node
set or get gyro register address and length to be read/write via gyr_reg_val nodes


* gyr_reg_val
--------------
read/write node
reads/write length and register address set via gyr_reg_sel node


* config_function
------------------
read/write node
Set or Get features enable/disabled
below are the index to enable sensor feature/Interrupt:
0	BMI088_ACC_DATA_READY,
1	BMI088_ACC_FIFO_WM,
2	BMI088_ACC_FIFO_FULL,
3	BMI088_GYRO_DATA_READY,
4	BMI088_GYRO_FIFO_WM,
5	BMI088_GYRO_FIFO_FULL,

to enable sensor use echo <sensor feature index> <en/disable> > config_function
to enable accel data ready use "echo 0 1 > config_function"
to disable a gyro fifo_full feature use "echo 5 0 > config_function"


* acc_fifo_dataframe
---------------------
read only node
reads fifo data from sensor, extract and print accel frames


* acc_fifo_config
------------------
read/write node
get/set accel fifo configration parameters
input order accel_fifo_en, fifo_mode, fifo_wm


* gyr_fifo_dataframe
---------------------
read only node
reads fifo data from sensor and extract, print gyro frames


* gyr_fifo_config
------------------
read/write node
get/set gyro fifo configration parameters
input order tag, wm, mode, data_select
gyr data ready
echo 0 0 0 > gyr_config
echo 1 50 1 > gyr_fifo_config
I2C
echo 3 1 > config_function
SPI
echo 0 1 > config_function

* gyr_fifo_full_interrupt
echo 0 0 1 > gyr_fifo_config
echo 7 7 4 > gyr_config
echo 5 1 > config_function //I2C
echo 2 1 > config_function //SPI

* gyr_fifo_wm_interrupt
echo 1 50 1 > gyr_fifo_config
echo 7 7 4 > gyr_config
echo 4 1 > config_function //I2C
echo 1 1 > config_function //SPI



* acc_fifo_full_interrupt
echo 1 0 300 > acc_fifo_config
echo 12 10 3 > acc_config
echo 2 1 > config_function

* acc_fifo_wm_interrupt
echo 1 0 300 > acc_fifo_config
echo 12 10 3 > acc_config
echo 1 1 > config_function




* gyr_soft_reset
--------------
write only node
triggers sensor soft reset on setting "1"


* gyr_selftest
------------
write only node
trigger sensor selftest on setting "1"


* data_sync
------------
read only node
enable data sync feature and correponding interrupt configurations
Acc and Gyro data will be printed once GYRO_DRDY_INT occures.


* sensor_time
--------------
read only node
reads sensor time data register



Test IIO Ring buffer:

cd /sys/bus/iio/devices/iio\:device0

echo 1 > iio_generic_buffer
I2C
/media/bst/iio_buffer_tool_dir/iio_buffer_tool -a -c 10 --device-name bmi08x -t bmi08x-dev0   -l 512
SPI accelerometer
/media/bst/iio_buffer_tool_dir/iio_buffer_tool -a -c 10 --device-name bmi08a -t bmi08a-dev0   -l 512
SPI Gyro meter
/media/bst/iio_buffer_tool_dir/iio_buffer_tool -a -c 10 --device-name bmi08g -t bmi08g-dev0   -l 512


echo 0 > iio_generic_buffer

NOTE :The test app (iio_buffer_tool) should be placed in  /media/bst/iio_buffer_tool_dir directory


Compilation Note:

If the kernel is k5.15, then enable BMI08_KERNEL_5_15 flag in Makefile(line number 5)
If the kernel is k6.1, then enable BMI08_KERNEL_6_1 flag in Makefile(line number 9)


Modify the kernel configurations

1. Update the kernel configuration to include the BMI088 IIO driver. You can do this by modifying the Kconfig and Makefile in the drivers/iio/imu directory.
	a. Edit drivers/iio/imu/Kconfig: Add an entry for the BMI088 driver:
		menu "Bosch sensor driver"
		config BOSCH_DRIVER
			tristate "Bosch sensor driver"
			depends on IIO
			help
			Say yes here to build support for the Bosch sensor.
			The Bosch Bosch is a high-precision, low-power digital pressure sensor
			designed for a wide range of applications, including weather monitoring,
			altitude tracking, and indoor navigation. This driver provides support
			for interfacing with the Bosch sensor through the Industrial I/O (IIO)
			subsystem.
			If you choose to compile this driver as a module, it will be named
			'Bosch'. This allows the driver to be dynamically loaded and unloaded
			as needed, providing flexibility in managing the sensor.
			To compile this driver as a module, choose M here: the module will be
			called Bosch. If you are unsure, it is safe to say 'N' here.
		if BOSCH_DRIVER
			source "drivers/iio/imu/bmi088/Kconfig"
		endif # BOSCH_DRIVER
		endmenu
	b. Edit drivers/iio/imu/Makefile : Add the BMI088 driver to the Makefile:
		obj-$(CONFIG_BOSCH_DRIVER)    += bmi088/

2. Modify the kernel  configuration using "make menuconfig" and select the inputs in below node:
		Device Drivers > Industrial I/O support > Inertial measurement units > Bosch sensor driver