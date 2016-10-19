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
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1
#define TRANSMITTER 0
#define RECEIVER 1
#define SET_AND_UA_SIZE 5
#define F 0X7E
#define A 0x03
#define C_SET 0x03
#define C_DISC  0xOb
#define C_UA 0X07
#define C_SEND(S) (S << 6)
#define C_RR(R) ((R << 7) | 0x05)
#define C_REJ(R) ((R << 7) | 0x01)
#define START 0
#define FLAG_RCV 1
#define A_RCV 2
#define C_RCV 3
#define BCC_OK 4
#define STOP_SM 5
#define BCC1_RCV 6
#define RCV_DATA 7
#define BCC2_RCV 8
#define TIME_OUT 100

volatile int pos = 0;

volatile int alarm_on = FALSE, alarm_calls = 0, write_fd;


void answer_alarm()
{
	if (alarm_on == TRUE) {
		alarm_calls++;
		sendSupervision();
		printf("Resend %d of 'SET'.\n", alarm_calls);

		if (alarm_calls < 3)	// to resend 'SET' 3 times
			alarm(TIME_OUT*2);
		else
			alarm_on = FALSE;
	}
}

int llopen(int porta /*, TRANSMITTER | RECEIVER*/)
{
	unsigned char SET_char;
	int state = START;
	while (state != STOP_SM) {
		if (read(porta, &SET_char, 1) != 1)    /* returns after 1 chars have been input */
			printf("A problem occurred reading a 'SET_char' on 'llopen'.\n");
		switch (state) {
		case START:
			if (SET_char == F)
				state = FLAG_RCV;
			//else state = START;
			break;
		case FLAG_RCV:
			if (SET_char == A)
				state = A_RCV;
			else if (SET_char == F)
				state = FLAG_RCV;
			else state = START;
			break;
		case A_RCV:
			if (SET_char == C_SET)
				state = C_RCV;
			else if (SET_char == F)
				state = FLAG_RCV;
			else state = START;
			break;
		case C_RCV:
			if (SET_char == (A ^ C_SET))
				state = BCC_OK;
			else if (SET_char == F)
				state = FLAG_RCV;
			else state = START;
			break;
		case BCC_OK:
			if (SET_char == F)
				state = STOP_SM;
			else state = START;
			break;
		}
	}

	unsigned char UA[SET_AND_UA_SIZE];
	UA[0] = F;
	UA[1] = A;
	UA[2] = C_UA;
	UA[3] = UA[1] ^ UA[2];
	UA[4] = F;
	int tr = write(porta, UA, SET_AND_UA_SIZE);

	return (tr == SET_AND_UA_SIZE) ? 0 : -1;
}



int llread(int fd, char * buffer)	// doesn't receive resends and doesn't ask for resends
{
	printf("Data layer reading 'information frame' from the serial conexion.\n");
	int state = START, i = 0, parity = 0xff;
	unsigned char ch;
	write_fd = fd;
	alarm(TIME_OUT);
	alarm_on = TRUE;
	while (state != STOP_SM) {
		if (read(fd, &ch, 1) != 1) {
			printf("A problem occurred reading on 'llread'.\n");
		}
		switch (state) {
		case START:
			if (ch == F)
				state = FLAG_RCV;
			// else state = START;
			break;
		case FLAG_RCV:
			if (ch == A)
				state = A_RCV;
			else if (ch == F)
				state = FLAG_RCV;
			else state = START;
			break;
		case A_RCV:
			if (ch == C_SEND(pos))
				state = C_RCV;
			else if (ch == F)
				state = FLAG_RCV;
			else state = START;
			break;
		case C_RCV:
			if (ch == (A ^ C_SEND(pos)))	// BCC1
				state = RCV_DATA;
			else if (ch == F)
				state = FLAG_RCV;
			else state = START;
			break;
		case RCV_DATA:
			if (ch == parity)	// BCC2
				state = BCC2_RCV;
			if(ch == 0x7d){
				if (read(fd, &ch, 1) != 1) {
					printf("A problem occurred reading on 'llread'.\n");
					}
				ch = ch ^ 0x20;
			}
			buffer[i] = ch;
			parity = (parity ^ buffer[i]);
			i++;
			break;
		case BCC2_RCV:
			if (ch == F) {
				state = STOP_SM;
				i--;	// delete last read char
			}
			else {
				buffer[i] = ch;
				parity = (parity ^ buffer[i]);
				i++;
				state = RCV_DATA;
			}
			break;
		}
	}
	printf("chegou ao fim");
	alarm_on = FALSE;

	pos = (pos + 1) % 2;

	//supervision
	sendSupervision();

	return i;
}


void sendSupervision(){
	unsigned char SP[5];
	SP[0] = F;
	SP[1] = A;
	SP[2] = C_RR(pos);	// or C_REJ(other pos)
	SP[3] = SP[1] ^ SP[2];
	SP[4] = F;
	write(write_fd, SP, 5);
}

volatile int STOP = FALSE;

int main(int argc, char** argv)
{
	(void)signal(SIGALRM, answer_alarm);

	int fd, c, res;
	struct termios oldtio, newtio;
	char buf[6000];

	if ((argc < 2) ||
		((strcmp("/dev/ttyS0", argv[1]) != 0) &&
		(strcmp("/dev/ttyS1", argv[1]) != 0))) {
		printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
		exit(1);
	}

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


	int imageFd;
	imageFd = open("pinguim.gif", O_WRONLY | O_CREAT | O_EXCL, 0644);
	char a[12000];

	llread(fd, a);

	write(imageFd, a, 10968);
	close(imageFd);

	//printf("%s\n", buf);
	printf("ok\n");

	//write(fd, buf, strlen(buf) + 1);
	tcsetattr(fd, TCSANOW, &oldtio);
	close(fd);
	return 0;
}
