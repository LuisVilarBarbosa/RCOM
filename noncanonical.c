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
#define DISC_AND_UA_SIZE 5
#define F 0x7E
#define A 0x03
#define C_SET 0x03
#define C_DISC 0x0b
#define C_UA 0x07
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
#define TIME_OUT 1
#define ESC 0x7d
#define MAX_SIZE 1048576	/* 1MB */

volatile int alarm_on = FALSE, alarm_calls = 0, write_fd, data_size = 0;
unsigned char data[MAX_SIZE];
int stateRead = START, readNumB = 0, parityRead = 0xff;

void alarmOn() {
	alarm_on = TRUE;
	alarm_calls = 0;
	alarm(TIME_OUT);
}

void alarmOff() {
	alarm_on = FALSE;
}

void answer_alarm()
{
	if (alarm_on == TRUE) {
		alarm_calls++;
		write(write_fd, data, data_size);
		printf("Resend %d of the data.\n", alarm_calls);
		stateRead = START;
		readNumB = 0;
		parityRead = 0xff;
		if (alarm_calls < 10)	// to resend the data 3 times
			alarm(TIME_OUT);
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

	data[0] = F;
	data[1] = A;
	data[2] = C_UA;
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = SET_AND_UA_SIZE;
	int tr = write(porta, data, data_size);

	return (tr == data_size) ? 0 : -1;
}

volatile int pos = 0;

int llread(int fd, unsigned char * buffer)
{
	write_fd = fd;

	//printf("Data layer reading 'information frame' from the serial conexion.\n");
	stateRead = START;
	readNumB = 0;
	parityRead = 0xff;
	unsigned char ch, antCh, auxAntChar;

	// Supervision frame to be used by the alarm handler
	/*data[0] = F;
	data[1] = A;
	data[2] = C_REJ(pos);
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = 5;*/

	alarmOn();
	while (stateRead != STOP_SM) {
		if (read(fd, &ch, 1) != 1)
			printf("A problem occurred reading on 'llread'.\n");
		switch (stateRead) {
		case START:
			if (ch == F)
				stateRead = FLAG_RCV;
			// else stateRead = START;
			break;
		case FLAG_RCV:
			if (ch == A)
				stateRead = A_RCV;
			else if (ch == F)
				stateRead = FLAG_RCV;
			else stateRead = START;
			break;
		case A_RCV:
			if (ch == C_SEND(pos))
				stateRead = C_RCV;
			//else if (ch == F)
			//	stateRead = FLAG_RCV;
			else{ stateRead = START; 
				printf("Erro: não recebeu a flag que devia");
			}
			break;
		case C_RCV:
			if (ch == (A ^ C_SEND(pos)))	// BCC1
				stateRead = RCV_DATA;
			else if (ch == F)
				stateRead = FLAG_RCV;
			else stateRead = START;
			break;
		case RCV_DATA:
			if (ch == parityRead) {	// BCC2
				stateRead = BCC2_RCV;
				antCh = ch;
			}
			else {
				if (ch == ESC) {
					if (read(fd, &ch, 1) != 1)
						printf("A problem occurred reading on 'llread'.\n");
					ch = ch ^ 0x20;
				}
				buffer[readNumB] = ch;
				parityRead = (parityRead ^ buffer[readNumB]);
				readNumB++;
			}
			break;
		case BCC2_RCV:
			if (ch == F) {
				stateRead = STOP_SM;
			}
			else {
				if (antCh == ESC) { //tratar do byte anterior se for um esc
					ch = ch ^ 0x20;
					buffer[readNumB] = ch;
					parityRead = (parityRead ^ buffer[readNumB]);
					readNumB++;
					stateRead = RCV_DATA;
					break;
				}
				else { // se o anterior se for data normal
					auxAntChar = antCh;
					buffer[readNumB] = auxAntChar;
					parityRead = (parityRead ^ buffer[readNumB]);
					readNumB++;
				}
				

				if (ch == parityRead) {// trata deste byte (pode ser paridade ou data)
					stateRead = BCC2_RCV;
					antCh = ch;
				}
				else {
					if (ch == ESC) {
						if (read(fd, &ch, 1) != 1)
							printf("A problem occurred reading on 'llread'.\n");
						ch = ch ^ 0x20;
					}
					buffer[readNumB] = ch;
					parityRead = (parityRead ^ buffer[readNumB]);
					readNumB++;
					stateRead = RCV_DATA;
				}
			}
			break;
		}
	}
	alarmOff();

	pos = (pos + 1) % 2;

	// Supervision frame in case of success
	data[0] = F;
	data[1] = A;
	data[2] = C_RR(pos);
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = 5;
	write(write_fd, data, data_size);
	return readNumB;
}

int llclose(int porta) {
	printf("chegou ao llclose\n");
	int write_fd = porta;

	unsigned char DISC_char;
	int state = START;
	while (state != STOP_SM) {
		if (read(porta, &DISC_char, 1) != 1)    /* returns after 1 chars have been input */
			printf("A problem occurred reading a 'DISC_char' on 'llclose'.\n");
		switch (state) {
		case START:
			if (DISC_char == F)
				state = FLAG_RCV;
			//else state = START;
			break;
		case FLAG_RCV:
			if (DISC_char == A)
				state = A_RCV;
			else if (DISC_char == F)
				state = FLAG_RCV;
			else state = START;
			break;
		case A_RCV:
			if (DISC_char == C_DISC)
				state = C_RCV;
			else if (DISC_char == F)
				state = FLAG_RCV;
			else state = START;
			break;
		case C_RCV:
			if (DISC_char == (A ^ C_DISC))
				state = BCC_OK;
			else if (DISC_char == F)
				state = FLAG_RCV;
			else state = START;
			break;
		case BCC_OK:
			if (DISC_char == F)
				state = STOP_SM;
			else state = START;
			break;
		}
	}

	data[0] = F;
	data[1] = A;
	data[2] = C_DISC;
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = DISC_AND_UA_SIZE;
	int tr_DISC = write(porta, data, data_size);

	alarmOn();
	unsigned char UA_char;
	state = START;
	while (state != STOP_SM) {
		if (read(write_fd, &UA_char, 1) != 1)    /* returns after 1 chars have been input */
			printf("A problem occurred reading a 'UA_char' on 'llclose'.\n");
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
	alarmOff();

	return (tr_DISC == DISC_AND_UA_SIZE) ? 0 : -1;
}

volatile int STOP = FALSE;

int main(int argc, char** argv)
{
	(void)signal(SIGALRM, answer_alarm);

	int fd;
	struct termios oldtio, newtio;

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
	leitura do(s) proximo(s) caracter(es)
  */

	tcflush(fd, TCIOFLUSH);

	if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	printf("New termios structure set\n");

	if (llopen(fd) == -1)
		printf("Error occurred executing 'llopen'.\n");

	int dataFd = open("pinguim.gif", O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, 0666);
	unsigned char fileData[MAX_SIZE];
	unsigned char readData[MAX_SIZE];

	int i = 0;
	int readBytes = 0;
	int j = 0;

	while (i < 10968){
		printf("bytes passados: %d\n", i);
		readBytes = llread(fd, fileData);
		j = readBytes;

		while(j > 0){
			readData[i+j-1] = fileData[j-1];
			j--;
		}
		i += readBytes;
	}


/*//TESTE
	unsigned char readData2[MAX_SIZE];


	i = 0;
	readBytes = 0;
	j = 0;

	while (i < 10968){
		printf("bytes passados: %d\n", i);
		readBytes = llread(fd, fileData);
		j = readBytes;
		printf("o j e: %d\n", j);

		while(j > 0){
			readData2[i+j-1] = fileData[j-1];
			printf("escreveu em readData[%d]: %02x\n", i+j-1, readData[i+j-1]);
			j--;
		}
		i += readBytes;
	}
	
	int asda = 0;
	for(asda = 0; asda < i; asda++){
		if(readData2[asda] != readData[asda])
			printf("erro em %d: %02x != %02x\n", asda,readData2[asda], readData[asda]);
	}

//fim de teste*/

	write(dataFd, readData, 10968);
	close(dataFd);

	//printf("%s\n", buf);

	//write(fd, buf, strlen(buf) + 1);
	tcsetattr(fd, TCSANOW, &oldtio);
	llclose(fd);
	return 0;
}
