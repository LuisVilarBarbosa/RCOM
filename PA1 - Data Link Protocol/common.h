#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "stats.h"

#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1
#define TRANSMITTER 0
#define RECEIVER 1
#define F 0x7e
#define A_SENDER_TO_RECEIVER_CMD 0x03
#define A_RECEIVER_TO_SENDER_ANSWER 0x03
#define A_RECEIVER_TO_SENDER_CMD 0x01
#define A_SENDER_TO_RECEIVER_ANSWER 0x01
#define C_SET 0x03
#define C_DISC 0x0b
#define C_UA 0x07
#define C_SEND(S) (S << 6)
#define C_RR(R) ((R << 7) | 0x05)
#define C_REJ(R) ((R << 7) | 0x01)
#define C_DATA 1
#define C_START 2
#define C_END 3
#define START 0
#define FLAG_RCV 1
#define A_RCV 2
#define C_RCV 3
#define BCC_OK 4
#define STOP_SM 5
#define BCC1_RCV 6
#define RCV_DATA 7
#define BCC2_RCV 8
#define INITIAL_PARITY 0xff
#define TIME_OUT 3
#define MAX_ALARM_CALLS 3
#define ESC 0x7d
#define MAX_APP_DATA_SIZE 65535	/* 2^(16 bits) - 1 --> application data field 'length' has 2 bytes */
#define MAX_SIZE 66560 /* 65KB -> MAX_APP_DATA_SIZE + some space for layer fields */
#define FILE_SIZE_INDICATOR 0
#define FILE_NAME_INDICATOR 1

void showBaudrates()
{
	printf("'BaudRate' possible values: 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 9600, 19200, 38400\n");
}

void chooseBaudrate(char *str, tcflag_t *baudrate)
{
	int val = atoi(str);
	switch (val) {
	case 50: *baudrate = B50; break;
	case 75: *baudrate = B75; break;
	case 110: *baudrate = B110; break;
	case 134: *baudrate = B134; break;
	case 150: *baudrate = B150; break;
	case 200: *baudrate = B200; break;
	case 300: *baudrate = B300; break;
	case 600: *baudrate = B600; break;
	case 1200: *baudrate = B1200; break;
	case 1800: *baudrate = B1800; break;
	case 2400: *baudrate = B2400; break;
	case 4800: *baudrate = B4800; break;
	case 9600: *baudrate = B9600; break;
	case 19200: *baudrate = B19200; break;
	case 38400: *baudrate = B38400; break;
	default:
		printf("Invalid 'BaudRate'.\n");
		showBaudrates();
		exit(-2);
	}
}

#endif
