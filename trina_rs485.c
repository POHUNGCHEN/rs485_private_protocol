#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <linux/serial.h>
#include <errno.h>
#include "trina_rs485.h"
#include "equipment.h"
#include "mxfb_rule.h"

// command line args
int _cl_rx_dump = 0;
int _cl_rx_dump_ascii = 0;
int _cl_tx_detailed = 0;
int _cl_stop_on_error = 0;
int _cl_rts_cts = 0;
int _cl_2_stop_bit = 0;
int _cl_parity = 0;
int _cl_odd_parity = 0;
int _cl_stick_parity = 0;
int _cl_dump_err = 0;
int _cl_no_rx = 0;
int _cl_no_tx = 0;
int _cl_rx_delay = 0;
int _cl_tx_delay = 0;
int _cl_tx_bytes = 0;
int _cl_rs485_delay = -1;
int _cl_ascii_range = 0;
char *_cl_port = NULL;

// Module variables
unsigned char _write_count_value = 0;
unsigned char _read_count_value = 0;
int _fd = -1;
unsigned char *_write_data;
ssize_t _write_size;

// keep our own counts for cases where the driver stats don't work
long long int _write_count = 0;
long long int _read_count = 0;
long long int _error_count = 0;

static void dump_data(unsigned char *b, int count)
{
	printf("%i bytes: ", count);
	int i;
	for (i = 0; i < count; i++)
	{
		printf("%02x ", b[i]);
	}

	printf("\n");
}

static void dump_data_ascii(unsigned char *b, int count)
{
	int i;
	for (i = 0; i < count; i++)
	{
		printf("%c", b[i]);
	}
}

static void dump_serial_port_stats(void)
{
	struct serial_icounter_struct icount = {0};

	printf("%s: count for this session: rx=%lld, tx=%lld, rx err=%lld\n", _cl_port, _read_count, _write_count, _error_count);

	int ret = ioctl(_fd, TIOCGICOUNT, &icount);
	if (ret != -1)
	{
		printf("%s: TIOCGICOUNT: ret=%i, rx=%i, tx=%i, frame = %i, overrun = %i, parity = %i, brk = %i, buf_overrun = %i\n",
			   _cl_port, ret, icount.rx, icount.tx, icount.frame, icount.overrun, icount.parity, icount.brk,
			   icount.buf_overrun);
	}
}

static unsigned char next_count_value(unsigned char c)
{
	c++;
	if (_cl_ascii_range && c == 127)
		c = 32;
	return c;
}

static void process_read_data(void)
{
	unsigned char rb[1024];
	int c = read(_fd, &rb, sizeof(rb));
	if (c > 0)
	{
		if (_cl_rx_dump)
		{
			if (_cl_rx_dump_ascii)
				dump_data_ascii(rb, c);
			else
				dump_data(rb, c);
		}

		// verify read count is incrementing
		int i;
		for (i = 0; i < c; i++)
		{
			if (rb[i] != _read_count_value)
			{
				if (_cl_dump_err)
				{
					printf("Error, count: %lld, expected %02x, got %02x\n",
						   _read_count + i, _read_count_value, rb[i]);
				}
				_error_count++;
				if (_cl_stop_on_error)
				{
					dump_serial_port_stats();
					exit(1);
				}
				_read_count_value = rb[i];
			}
			_read_count_value = next_count_value(_read_count_value);
		}
		_read_count += c;
	}
}

int write_protocol_data(TR_Msg *tr)
{
	if (tr->_cl_soi_byte >= 0) {
		unsigned char data[16];
		int bytes = 1;
		int written;
		int i=0;
		data[i] = (unsigned char)tr->_cl_soi_byte;
		i++;
		if (tr->_cl_pversion_byte >= 0) {
			data[i] = (unsigned char)tr->_cl_pversion_byte;
			i++;
			bytes++;
			data[i] = (unsigned char)tr->_cl_adr_byte;
			i++;
			bytes++;
			data[i] = (unsigned char)tr->_cl_cid1_byte;
			i++;
			bytes++;
			data[i] = (unsigned char)tr->_cl_cid2_byte;
			i++;
			bytes++;
			data[i] = (unsigned char)tr->_cl_len1_byte;
			i++;
			bytes++;
			data[i] = (unsigned char)tr->_cl_len2_byte;
			i++;
			bytes++;
			if (tr->_cl_DataInfo1_byte >= 0) {
				data[i] = (unsigned char)tr->_cl_DataInfo1_byte;
				i++;
				bytes++;
				if (tr->_cl_DataInfo2_byte >= 0) {
					data[i] = (unsigned char)tr->_cl_DataInfo2_byte;
					i++;
					bytes++;
					if (tr->_cl_DataInfo3_byte >= 0) {
						data[i] = (unsigned char)tr->_cl_DataInfo3_byte;
						i++;
						bytes++;
						if (tr->_cl_DataInfo4_byte >= 0) {
							data[i] = (unsigned char)tr->_cl_DataInfo4_byte;
							i++;
							bytes++;
						}
					}
				}
			}
			data[i] = (unsigned char)tr->_cl_ckcsum1_byte;
			i++;
			bytes++;
			data[i] = (unsigned char)tr->_cl_ckcsum2_byte;
			i++;
			bytes++;
			data[i] = (unsigned char)tr->_cl_eoi_byte;
			i++;
			bytes++;
		}

		written = write(_fd, &data, bytes);
		if (written < 0) {
			perror("write()");
			return 1;
		} else if (written != bytes) {
			fprintf(stderr, "ERROR: write() returned %d, not %d\n", written, bytes);
			return 1;
		}
		return 0;
	}

}

static int diff_ms(const struct timespec *t1, const struct timespec *t2)
{
	struct timespec diff;

	diff.tv_sec = t1->tv_sec - t2->tv_sec;
	diff.tv_nsec = t1->tv_nsec - t2->tv_nsec;
	if (diff.tv_nsec < 0)
	{
		diff.tv_sec--;
		diff.tv_nsec += 1000000000;
	}
	return (diff.tv_sec * 1000 + diff.tv_nsec / 1000000);
}

int mx_rs485_new(char _cl_port)
{
	struct termios newtio;
	struct serial_rs485 rs485;
	int baud = B9600;
	int enable = 1;

	_fd = open(_cl_port, O_RDWR | O_NONBLOCK);

	if (_fd < 0)
	{
		printf("Error opening serial port %s \n", _cl_port);
		//free(_cl_port);
		exit(1);
	}

	bzero(&newtio, sizeof(newtio)); /* clear struct for new port settings */

	/* man termios get more info on below settings */
	newtio.c_cflag = baud | CS8 | CLOCAL | CREAD;

	if (_cl_rts_cts)
	{
		newtio.c_cflag |= CRTSCTS;
	}

	if (_cl_2_stop_bit)
	{
		newtio.c_cflag |= CSTOPB;
	}

	if (_cl_parity)
	{
		newtio.c_cflag |= PARENB;
		if (_cl_odd_parity)
		{
			newtio.c_cflag |= PARODD;
		}
		if (_cl_stick_parity)
		{
			newtio.c_cflag |= CMSPAR;
		}
	}

	newtio.c_iflag = 0;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;

	// block for up till 128 characters
	newtio.c_cc[VMIN] = 128;

	// 0.5 seconds read timeout
	newtio.c_cc[VTIME] = 5;

	/* now clean the modem line and activate the settings for the port */
	tcflush(_fd, TCIOFLUSH);
	tcsetattr(_fd, TCSANOW, &newtio);

	/* enable/disable rs485 direction control */
	if (ioctl(_fd, TIOCGRS485, &rs485) < 0)
	{
		printf("Error: TIOCGRS485 ioctl not supported.\n");
	}
	if (enable)
	{
		//Enable rs485 mode
		printf("Port %s ==> type: RS485\n", _cl_port);
		rs485.flags |= SER_RS485_ENABLED;
		rs485.delay_rts_before_send = 0x00000004;
		if (ioctl(_fd, TIOCSRS485, &rs485) < 0)
		{
			perror("Error setting RS-485 mode");
		}
	}
}

int mx_rs485_connect(TR_Msg *tr)
{

	struct pollfd serial_poll;
	serial_poll.fd = _fd;
	serial_poll.events &= ~POLLIN;
	serial_poll.events &= ~POLLOUT;

	struct timespec start_time, last_stat, last_timeout, last_read, last_write;

	clock_gettime(CLOCK_MONOTONIC, &start_time);
	last_stat = start_time;
	last_timeout = start_time;
	last_read = start_time;
	last_write = start_time;

	while (!(_cl_no_rx && _cl_no_tx))
	{
		struct timespec current;
		int rx_timeout, tx_timeout;
		int retval = poll(&serial_poll, 1, 1000);

		clock_gettime(CLOCK_MONOTONIC, &current);

		if (retval == -1){
			perror("poll()");
		}
		else if (retval){
			if (serial_poll.revents & POLLOUT){
				write_protocol_data(tr);
				last_write = current;
			}
			
			if (serial_poll.revents & POLLIN){
				process_read_data();
				last_read = current;
			}
		}

		// Has it been at least 500ms since we reported a timeout?
		if (diff_ms(&current, &last_timeout) > 500)
		{
			// Has it been over 500ms since we transmitted or received data?
			rx_timeout = (!_cl_no_rx && diff_ms(&current, &last_read) > 500);
			tx_timeout = (!_cl_no_tx && diff_ms(&current, &last_write) > 500);

			if (_cl_no_tx && _write_count != 0 && _write_count == _read_count)
			{
				rx_timeout = 0;
			}

			if (rx_timeout || tx_timeout)
			{
				const char *s;
				if (rx_timeout)
				{
					printf("No data received for %.1fs.",
						   (double)diff_ms(&current, &last_read) / 500);
					s = " ";
				}
				else
				{
					s = "";
				}
				/*
				if (tx_timeout)
				{
					printf("%sNo data transmitted for %.1fs.",
						   s, (double)diff_ms(&current, &last_write) / 500);
				}
				*/
				printf("\n");
				last_timeout = current;
			}
		}
	}

	tcdrain(_fd);
	dump_serial_port_stats();
	tcflush(_fd, TCIOFLUSH);
//	free(_cl_port);

	long long int result = llabs(_write_count - _read_count) + _error_count;

	return (result > 125) ? 125 : (int)result;
}

int mx_rs485_reconnect(TR_Msg *tr)
{
	close(_fd);
	tcflush(_fd, TCIOFLUSH);
//	free(_cl_port);
	mx_rs485_new(_cl_port);
	mx_rs485_connect(tr);
	return 0;
}

int mx_rs485_close()
{
	tcflush(_fd, TCIOFLUSH);
//	free(_cl_port);
	exit(0);
}

