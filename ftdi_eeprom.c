/*
 * ftdi_eeprom.c
 *
 * Copyright (C) 2013 linruisheng <ruishenglin@126.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <libusb-1.0/libusb.h>
#include <libftdi1/ftdi.h>

static int verbose = 0; //verbose output

struct eeprom_t {
	unsigned char data[1024];
	int len;
};

static struct ftdi_context * ftdi1_open(int vendor, int product,
		const char* serial) {
	struct ftdi_context *ftdi = NULL;
	if ((ftdi = ftdi_new()) == 0) {
		printf("ftdi_bew failed\n");
		return NULL;
	}

	if (ftdi_usb_open_desc(ftdi, vendor, product, NULL, serial) < 0) {
		ftdi_free(ftdi);
		ftdi = NULL;
		return NULL;
	}

	return ftdi;
}

static void ftdi1_close(struct ftdi_context *ftdi, int reset_port) {

	if (ftdi == NULL)
		return;

	if (ftdi->usb_dev)
		if (libusb_release_interface(ftdi->usb_dev, 0) != 0)
			fprintf(stderr, "libusb_release_interface error");

#ifdef __linux__
	if (ftdi->usb_dev) {
		int ret = 0;
		//attach kernel driver, connect to ttyUSB
		ret = libusb_attach_kernel_driver(ftdi->usb_dev, 0);
		if (verbose && ret < 0) {
			if (ret == LIBUSB_ERROR_NOT_FOUND)
				printf("%s: LIBUSB_ERROR_NOT_FOUND\n", __FUNCTION__);
			else if (ret == LIBUSB_ERROR_INVALID_PARAM)
				printf("%s: LIBUSB_ERROR_INVALID_PARAM\n", __FUNCTION__);
			else if (ret == LIBUSB_ERROR_NO_DEVICE)
				printf("%s: LIBUSB_ERROR_NO_DEVICE\n", __FUNCTION__);
			else if (ret == LIBUSB_ERROR_BUSY)
				printf("%s: LIBUSB_ERROR_BUSY\n", __FUNCTION__);
		}
	}
#endif

	if (ftdi->usb_dev) {
		if (reset_port)
			libusb_reset_device(ftdi->usb_dev);
		libusb_close(ftdi->usb_dev);
		ftdi->usb_dev = NULL;
	}

	ftdi_free(ftdi);
	ftdi = NULL;
}

/**
 * checksum - Calculate checksum of p[len-2]
 * @return: return checksum
 */
static unsigned short check_sum(const unsigned char *p, int len) {
	int i;
	unsigned short checksum, value;
	checksum = 0xAAAA;
	for (i = 0; i < len / 2 - 1; i++) {
		value = p[i * 2];
		value += p[(i * 2) + 1] << 8;
		checksum = value ^ checksum;
		checksum = (checksum << 1) | (checksum >> 15);
	}
	return checksum;
}

static int write_eeprom_to_file(const struct eeprom_t *eeprom,
		const char *filename) {
	int ret;
	//S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
	int fd = open(filename, O_WRONLY | O_CREAT, 0666);
	if (fd < 0) {
		fprintf(stderr, "Can't open eeprom file %s: %s\n", filename,
				strerror(errno));
		return -1;
	}
	ret = write(fd, eeprom->data, eeprom->len);
	close(fd);
	return ret == eeprom->len ? 0 : -1;
}

static int read_eeprom_from_file(struct eeprom_t *eeprom, const char *filename) {
	int fd = open(filename, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Can't open eeprom file %s: %s\n", filename,
				strerror(errno));
		return -1;
	}
	eeprom->len = read(fd, eeprom->data, sizeof(eeprom->data));
	close(fd);
	return eeprom->len > 0 ? 0 : -1;
}

/**
 * @return: 0 all fine, -1 write failed
 */
static int write_eeprom_location(struct ftdi_context *ftdi, int eeprom_addr,
		unsigned short eeprom_val) {
	if (libusb_control_transfer(ftdi->usb_dev, FTDI_DEVICE_OUT_REQTYPE,
			SIO_WRITE_EEPROM_REQUEST, eeprom_val, eeprom_addr, NULL, 0,
			ftdi->usb_write_timeout) != 0)
		return -1;

	return 0;
}

static int get_eeprom(struct ftdi_context *ftdi, struct eeprom_t *eeprom) {
	if (ftdi_read_eeprom(ftdi) != 0) {
		fprintf(stderr, "%s\n", ftdi_get_error_string(ftdi));
		return -1;
	}

	if (ftdi_get_eeprom_value(ftdi, CHIP_SIZE, &eeprom->len) != 0)
		return -1;

	if (eeprom->len > sizeof(eeprom->data)) {
		fprintf(stderr, "EEPROM size too large\n");
		return -1;
	}

	if (ftdi_get_eeprom_buf(ftdi, eeprom->data, eeprom->len) != 0) {
		return -1;
	}

	return 0;
}

static void eeprom_build_checksum(struct eeprom_t *eeprom) {
	unsigned short checksum;
	checksum = check_sum(eeprom->data, eeprom->len);
	eeprom->data[eeprom->len - 2] = checksum;
	eeprom->data[eeprom->len - 1] = checksum >> 8;
}

/**
 * write ftdi eeprom
 * @return: return 0 on success, -1 on error
 */
static int write_eeprom(struct ftdi_context *ftdi, struct eeprom_t *eeprom) {
	int i;
	int ret;
	struct eeprom_t cur_eeprom;
	if (get_eeprom(ftdi, &cur_eeprom) != 0)
		return -1;

	eeprom_build_checksum(eeprom);
	for (i = 0, ret = 0; i < cur_eeprom.len; i += 2) {
		if (cur_eeprom.data[i] != eeprom->data[i]
				|| cur_eeprom.data[i + 1] != eeprom->data[i + 1]) {
			unsigned short value;
			value = eeprom->data[i] | (eeprom->data[i + 1] << 8);
			ret |= write_eeprom_location(ftdi, i / 2, value);
		}
	}
	return ret == 0 ? 0 : -1;
}

static int eeprom_get_unused_size(struct eeprom_t *eeprom) {
	/* usage at end of EEPROM:
	 *	manufacturer[n] + product[n] + serial[n] + port_pnp[3] + unused[n] + checksum[2]
	 */
	int unused_len;
	int unused_area;
	unused_area = (eeprom->data[0x12] & (eeprom->len - 1)) + eeprom->data[0x13]
			+ 4;
	unused_len = eeprom->len - unused_area - 2;
	return unused_len;
}

static int eeprom_set_id(struct eeprom_t *eeprom, int ID) {
	/*
	 * NOTE: ID places 6，5，4，3 bytes at the end of EEPROM。
	 */
	int i;
	int unused_len;
	unused_len = eeprom_get_unused_size(eeprom);
	if (unused_len < 4) {
		fprintf(stderr, "unused area too small: %d\n", unused_len);
		return -1;
	}

	i = eeprom->len - 6;
	eeprom->data[i++] = ID >> 24;
	eeprom->data[i++] = ID >> 16;
	eeprom->data[i++] = ID >> 8;
	eeprom->data[i++] = ID;
	return 0;
}

static int eeprom_get_id(struct eeprom_t *eeprom) {
	int id;
	int i;
	int unused_len;
	unused_len = eeprom_get_unused_size(eeprom);
	if (unused_len < 4) {
		fprintf(stderr, "unused area too small: %d\n", unused_len);
		return -1;
	}

	//ID
	i = eeprom->len - 6;
	id = (eeprom->data[i] << 24) | (eeprom->data[i + 1] << 16)
			| (eeprom->data[i + 2] << 8) | (eeprom->data[i + 3]);

	return id;
}

static int eeprom_get_tailer(struct eeprom_t *eeprom, unsigned char buf[4]) {
	int unused_len;
	int i;
	int pos;
	unused_len = eeprom_get_unused_size(eeprom);
	if (unused_len < 0)
		return -1;

	pos = eeprom->len - unused_len - 2 - 4;
	for (i = 0; i < 4; i++) {
		buf[i] = eeprom->data[pos++];
	}

	return 0;
}

static void eeprom_get_str(struct eeprom_t *eeprom, int addr, char *str,
		int len) {
	int pos, size, i;
	if (str == NULL || len <= 0)
		return;
	// Addr: Offset of the string + 0x80, calculated later
	// Addr+1: Length of string
	pos = (eeprom->data[addr] & (eeprom->len - 1));
	size = eeprom->data[addr + 1] / 2 - 1;
	pos += 2;
	for (i = 0; i < size && i < len - 1; i++)
		str[i] = eeprom->data[pos + i * 2];
	str[i] = '\0';
}

static void eeprom_get_strings(struct eeprom_t *eeprom, char * manufacturer,
		int mnf_len, char * description, int desc_len, char * serial,
		int serial_len) {
	eeprom_get_str(eeprom, 0x0E, manufacturer, mnf_len);
	eeprom_get_str(eeprom, 0x10, description, desc_len);
	eeprom_get_str(eeprom, 0x12, serial, serial_len);
}

static int eeprom_set_serial(struct eeprom_t *eeprom, char *serial) {
	unsigned char tailer[4];
	int serial_size = 0;
	int serial_pos;
	int i;
	if (serial == NULL)
		return -1;

	serial_size = strlen(serial);

	if (eeprom_get_tailer(eeprom, tailer) != 0)
		return -1;

	// Addr 12: Offset of the serial string + 0x80, calculated later
	// Addr 13: Length of serial string
	serial_pos = (eeprom->data[0x12] & (eeprom->len - 1));
	eeprom->data[0x13] = serial_size * 2 + 2;
	eeprom->data[serial_pos++] = serial_size * 2 + 2;
	eeprom->data[serial_pos++] = 0x03;

	for (i = 0; i < serial_size; i++) {
		eeprom->data[serial_pos++] = serial[i];
		eeprom->data[serial_pos++] = 0x00;
	}
	for (i = 0; i < 4; i++) {
		eeprom->data[serial_pos++] = tailer[i];
	}
	while (serial_pos < eeprom->len)
		eeprom->data[serial_pos++] = 0;
	return 0;
}

static const char *version_text =
		"ftdi-eeprom version 0.01 ("__DATE__ "-" __TIME__")- A tool to read & write ID & EEPROM of ftdi chips\n"
		"Copyright (C) 2013 ruisheng <ruishengleen@gmail.com>\n"
		"This is free software; see the source for copying conditions.  There is NO\n"
		"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR "
		"PURPOSE.\n\n";

static void print_usage(const char *programname) {
	printf("%s", version_text);
	printf("usage: %s [options]\n", programname);
	printf("   -s\n"
			"      usb device serial string\n"
			"   -g\n"
			"      get id\n"
			"   -i [id]\n"
			"      set id\n"
			"   -r [file]\n"
			"      read device's eeprom to file\n"
			"   -w [file]\n"
			"      write device's eeprom to file\n"
			"   -S [serial string]\n"
			"      set new serial string\n"
			"   -l\n"
			"      list ftdi devices\n"
			"   -v\n"
			"      verbose output\n"
			"   -h\n"
			"      help\n");

	printf("\nexamples:\n");
	printf("read ID:\n");
	printf("\t%s -s [serial] -g\n", programname);
	printf("write ID:\n");
	printf("\t%s -s [serial] -i [ID]\n", programname);
	printf("read device's EEPROM to file:\n");
	printf("\t%s -s [serial] -r [EEPROM file]\n", programname);
	printf(
			"write EEPROM file to device's EEPROM, only change serial string:\n");
	printf("\t%s -s [serial] -S [new serial] -w [EEPROM file]\n", programname);

}

enum op_mode {
	GET_ID, SET_ID, WRITE_EEPROM, READ_EEPROM
};

struct ftdi_eeprom_op {
	char *serial;
	char *new_serial;
	char *eeprom_file;
	int id;
	enum op_mode mode;
};

static int list_usb(void) {
	int ret, i;
	struct ftdi_context *ftdi;
	struct ftdi_device_list *devlist, *curdev;
	char manufacturer[128], description[128], serial[128];
	int retval = EXIT_SUCCESS;

	if ((ftdi = ftdi_new()) == 0) {
		fprintf(stderr, "ftdi_new failed\n");
		return EXIT_FAILURE;
	}

	if ((ret = ftdi_usb_find_all(ftdi, &devlist, 0, 0)) < 0) {
		fprintf(stderr, "ftdi_usb_find_all failed: %d (%s)\n", ret,
				ftdi_get_error_string(ftdi));
		retval = EXIT_FAILURE;
		goto do_deinit;
	}

	for (i = 0, curdev = devlist; curdev != NULL; i++) {

		if ((ret = ftdi_usb_get_strings(ftdi, curdev->dev, manufacturer, 128,
				description, 128, serial, 128)) < 0) {
			fprintf(stderr, "ftdi_usb_get_strings failed: %d (%s)\n", ret,
					ftdi_get_error_string(ftdi));
			retval = EXIT_FAILURE;
			goto done;
		}

		printf("Device #%d: \n"
				"\tManufacturer:\t%s\n"
				"\tDescription:\t%s\n"
				"\tSerial:     \t%s\n", i, manufacturer, description, serial);
		curdev = curdev->next;
	}
	done: ftdi_list_free(&devlist);
	do_deinit: ftdi_free(ftdi);

	return retval;
}

static void parse_argument(int argc, char *argv[], struct ftdi_eeprom_op *op) {
	int opt;
	while ((opt = getopt(argc, argv, "s:i:r:w:S:vlgh")) != -1) {
		switch (opt) {
		case 's': // target serial
			op->serial = optarg;
			break;

		case 'g': //get id
			op->mode = GET_ID;
			break;

		case 'i': // set id
			op->mode = SET_ID;
			op->id = atoi(optarg);
			break;

		case 'w': // write eeprom
			op->mode = WRITE_EEPROM;
			op->eeprom_file = optarg;
			break;

		case 'S': // new serial string
			op->new_serial = optarg;
			break;

		case 'r': // read eeprom to file
			op->mode = READ_EEPROM;
			op->eeprom_file = optarg;
			break;

		case 'l':
			list_usb();
			exit(0);
			break;
		case 'h':
			print_usage(argv[0]);
			exit(0);
			break;
		case 'v': //verbose output
			verbose = 1;
			break;
		default:
			print_usage(argv[0]);
			exit(-1);
			break;
		}
	}

	//check argument
	if (op->serial != NULL) {
		switch (op->mode) {
		case GET_ID:
			return;
			break;
		case SET_ID:
			if (op->id > 0)
				return;
			break;
		case READ_EEPROM:
			if (op->eeprom_file != NULL)
				return;
			break;
		case WRITE_EEPROM:
			if (op->eeprom_file != NULL)
				return;
			break;
		}
	}
	print_usage(argv[0]);
	exit(-1);
}

static int process(struct ftdi_eeprom_op *op) {
	int retval = 0;
	struct ftdi_context *ftdi = NULL;
	struct eeprom_t eeprom;

	ftdi = ftdi1_open(0x403, 0x6014, op->serial);
	if (ftdi == NULL)
		ftdi = ftdi1_open(0x403, 0x6001, op->serial);
	if (ftdi == NULL) {
		fprintf(stderr, "Can't open device\n");
		return -1;
	}

	if (get_eeprom(ftdi, &eeprom) != 0) {
		fprintf(stderr, "Failed to read EEPROM\n");
		return -1;
	}

	switch (op->mode) {
	case GET_ID:
		printf("ID = %d\n", eeprom_get_id(&eeprom));
		break;
	case SET_ID:
		if (eeprom_set_id(&eeprom, op->id) != 0)
			retval = -1;
		else if (write_eeprom(ftdi, &eeprom) != 0) {
			fprintf(stderr, "Failed to write EEPROM\n");
			retval = -1;
		}
		break;
	case READ_EEPROM:
		if (write_eeprom_to_file(&eeprom, op->eeprom_file) != 0) {
			retval = -1;
		}
		break;
	case WRITE_EEPROM:
		if (read_eeprom_from_file(&eeprom, op->eeprom_file) != 0) {
			fprintf(stderr,
					"Failed to write EEPROM file, read length = %d bytes\n",
					eeprom.len);
			retval = -1;
		} else {
			if (verbose) {
				char manu[16] = { 0 };
				char des[50] = { 0 };
				char serial[50] = { 0 };
				eeprom_get_strings(&eeprom, manu, sizeof(manu), des,
						sizeof(des), serial, sizeof(serial));
				printf("EEPROM FILE: (%s) (%s) (%s)\n", manu, des, serial);
			}
			if (op->new_serial != NULL && eeprom_set_serial(&eeprom, op->new_serial) != 0) {
				fprintf(stderr, "Failed to set serial to EEPROM buffer\n");
				retval = -1;
			} else if (write_eeprom(ftdi, &eeprom) != 0) {
				fprintf(stderr, "Failed to write EEPROM\n");
				retval = -1;
			}
		}
		break;
	default:
		fprintf(stderr, "Unknown operation mode\n");
		retval = -1;
		break;
	}

	if (ftdi)
		ftdi1_close(ftdi, op->mode == WRITE_EEPROM ? 1 : 0);
	return retval;
}

int main(int argc, char *argv[]) {
	int ret;
	struct ftdi_eeprom_op ftdi_op;

	bzero(&ftdi_op, sizeof(ftdi_op));
	parse_argument(argc, argv, &ftdi_op);

	ret = process(&ftdi_op);
	return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
