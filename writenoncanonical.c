/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1
#define TRANSMITTER 0
#define RECEIVER 1
#define SET_AND_UA_SIZE 5
#define F 0X7e
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07
#define START 0
#define FLAG_RCV 1
#define A_RCV 2
#define C_RCV 3
#define BCC_OK 4
#define STOP_SM 5
#define TIME_OUT 3

volatile int alarm_on = FALSE, alarm_calls = 0, write_fd;
unsigned char SET[SET_AND_UA_SIZE];	// to be used by 'answer_alarm' and 'llopen'

void answer_alarm()
{
	if (alarm_on == TRUE) {
		alarm_calls++;
		write(write_fd, SET, SET_AND_UA_SIZE);
		printf("Resend %d of 'SET'.\n", alarm_calls);

		if (alarm_calls < 3)	// to resend 'SET' 3 times
			alarm(TIME_OUT);
		else
			alarm_on = FALSE;
	}
}

int llopen(int porta /*, TRANSMITTER | RECEIVER*/)
{
	write_fd = porta;
	SET[0] = F;
	SET[1] = A;
	SET[2] = C_SET;
	SET[3] = SET[1] ^ SET[2];
	SET[4] = F;
	int tr = write(write_fd, SET, SET_AND_UA_SIZE);
	(void)signal(SIGALRM, answer_alarm);	// mantain here?
	alarm(TIME_OUT);
	alarm_on = TRUE;

	unsigned char UA_char;
	int state = START;
	while (state != STOP_SM) {
		if (read(porta, &UA_char, 1) != 1)    /* returns after 1 chars have been input */
			printf("A problem occurred reading a 'UA_char' on 'llopen'.\n");
		switch (state) {
		case START:
			if (UA_char == F)
				state = FLAG_RCV;
			//else state = START;
			break;
		case FLAG_RCV:
			if (UA_char == A)
				state = A_RCV;
			else if (UA_char == F)
				state = FLAG_RCV;
			else state = START;
			break;
		case A_RCV:
			if (UA_char == C_UA)
				state = C_RCV;
			else if (UA_char == F)
				state = FLAG_RCV;
			else state = START;
			break;
		case C_RCV:
			if (UA_char == (A ^ C_UA))
				state = BCC_OK;
			else if (UA_char == F)
				state = FLAG_RCV;
			else state = START;
			break;
		case BCC_OK:
			if (UA_char == F)
				state = STOP_SM;
			else state = START;
			break;
		}
	}
	alarm_on = FALSE;

	return (tr == SET_AND_UA_SIZE) ? 0 : -1;
}

volatile int STOP = FALSE;

int main(int argc, char** argv)
{
	int fd, c, res;
	struct termios oldtio, newtio;
	char buf[255];
	int i, sum = 0, speed = 0;

	if ((argc < 2) ||
		((strcmp("/dev/ttyS0", argv[1]) != 0) &&
		(strcmp("/dev/ttyS1", argv[1]) != 0))) {
		printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
		exit(1);
	}

	gets(buf);

	/*
	  Open serial port device for reading and writing and not as controlling tty
	  because we don't want to get killed if linenoise sends CTRL-C.
	*/


	fd = open(argv[1], O_RDWR | O_NOCTTY);
	if (fd < 0) { perror(argv[1]); exit(-1); }

	if (tcgetattr(fd, &oldtio) == -1) { /* save current port settings */
		perror("tcgetattr");
		exit(-1);
	}

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;

	/* set input mode (non-canonical, no echo,...) */
	newtio.c_lflag = 0;

	newtio.c_cc[VTIME] = 0;   /* inter-character timer unused */
	newtio.c_cc[VMIN] = 1;   /* blocking read until 1 char received */

  /*
	VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
	leitura do(s) próximo(s) caracter(es)
  */

	tcflush(fd, TCIOFLUSH);

	if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	printf("New termios structure set\n");

	if (llopen(fd) == -1)
		printf("Error occurred executing 'llopen'.\n");

	res = write(fd, buf, strlen(buf) + 1);
	printf("%d bytes written\n", res);

	i = 0;
	while (STOP == FALSE) {
		res = read(fd, &buf[i], 1);
		if (buf[i] == '\0')
			STOP = TRUE;
		i += res;
	}
	printf("%s\n", buf);

	if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	close(fd);
	return 0;
}
