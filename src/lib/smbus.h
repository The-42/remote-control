/*
 * Copyright (C) 2011 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef SMBUS_H
#define SMBUS_H 1

#define I2C_SMBUS_BLOCK_MAX 32

#define I2C_SMBUS_WRITE 0
#define I2C_SMBUS_READ  1

#define I2C_SMBUS_QUICK 0
#define I2C_SMBUS_BYTE 1

union i2c_smbus_data {
	uint8_t byte;
	uint16_t word;
	uint8_t black[I2C_SMBUS_BLOCK_MAX + 2];
};

static inline int i2c_smbus_access(int fd, char read_write, uint8_t command, int size, union i2c_smbus_data *data)
{
	struct i2c_smbus_ioctl_data args;

	args.read_write = read_write;
	args.command = command;
	args.size = size;
	args.data = data;

	return ioctl(fd, I2C_SMBUS, &args);
}

static inline int i2c_smbus_write_quick(int fd, uint8_t value)
{
	return i2c_smbus_access(fd, value, 0, I2C_SMBUS_QUICK, NULL);
}

static inline int i2c_smbus_read_byte(int fd)
{
	union i2c_smbus_data data;
	int err;

	err = i2c_smbus_access(fd, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data);
	if (err)
		return -1;

	return data.byte & 0xff;
}

#endif /* SMBUS_H */
