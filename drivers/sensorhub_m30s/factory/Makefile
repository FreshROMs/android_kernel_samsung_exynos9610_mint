#
# Makefile for the sensor hub drivers.
#

# Each configuration option enables a list of files.
obj-y                                           += ssp_sensor.o
obj-$(CONFIG_SENSORS_SSP_ACCELOMETER)           += Accelometer/ accelometer_sensor.o
obj-$(CONFIG_SENSORS_SSP_GYROSCOPE)             += Gyroscope/ gyroscope_sensor.o
obj-$(CONFIG_SENSORS_SSP_MAGNETIC)              += Magnetic/ magnetic_sensor.o
obj-$(CONFIG_SENSORS_SSP_PROXIMITY)             += Proximity/ proximity_sensor.o
obj-$(CONFIG_SENSORS_SSP_LIGHT)                 += Light/ light_sensor.o
obj-$(CONFIG_SENSORS_SSP_BAROMETER)             += Barometer/ barometer_sensor.o
obj-$(CONFIG_SENSORS_SSP_MOBEAM)                += barcode_emulator.o

