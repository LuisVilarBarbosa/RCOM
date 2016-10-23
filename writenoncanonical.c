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
#define DISC_AND_UA_SIZE 5
#define F 0x7e
#define A 0x03
#define C_SET 0x03
#define C_DISC 0x0b
#define C_UA 0x07
#define C_SEND(S) (S << 6)
#define C_RR(R) ((R << 7) | 0x05)
#define C_REJ(R) ((R << 7) | 0x01)
#define C_START 2
#define START 0
#define FLAG_RCV 1
#define A_RCV 2
#define C_RCV 3
#define BCC_OK 4
#define STOP_SM 5
#define BCC1_RCV 6
#define TIME_OUT 3
#define ESC 0x7d
#define MAX_SIZE 1048576	/* 1MB */
#define FILE_SIZE_INDICATOR 0
#define FILE_NAME_INDICATOR 1

volatile int alarm_on = FALSE, alarm_calls = 0, write_fd, data_size = 0;
int stateWrite = START;
unsigned char data[MAX_SIZE];

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
		stateWrite = START;
		if (alarm_calls < 3)	// to resend the data 3 times
			alarm(TIME_OUT);
		else
			alarm_on = FALSE;
	}
}

int llopen(int porta /*, TRANSMITTER | RECEIVER*/)
{
	write_fd = porta;

	data[0] = F;
	data[1] = A;
	data[2] = C_SET;
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = SET_AND_UA_SIZE;
	int tr = write(write_fd, data, data_size);

	alarmOn();
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
	alarmOff();

	return (tr == data_size) ? 0 : -1;
}

volatile int pos = 0;	// can be 0 or 1, it is used by llwrite

int llwrite(int fd, unsigned char *buffer, int length)
{
	write_fd = fd;
	int repete = TRUE;
	while (repete == TRUE) {
		//printf("Data layer writing 'information frame' to the serial conexion.\n");
		// Header
		data[0] = F;
		data[1] = A;
		data[2] = C_SEND(pos);
		data[3] = (data[1] ^ data[2]);		// BCC1
		data_size = 4;

		// Data from the application level
		int i;
		for (i = 0; i < length; i++) {
			if (buffer[i] == ESC || buffer[i] == F) {
				data[data_size] = ESC;
				data_size++;
				unsigned char flag = buffer[i] ^ 0x20;
				data[data_size] = flag;
				data_size++;
			}
			else {
				data[data_size] = buffer[i];
				data_size++;
			}
		}
		int parity = 0xff;
		for (i = 0; i < length; i++) {
			parity = (parity ^ buffer[i]);
		}

		// Trailer;
		data[data_size] = parity; // BCC2
		data_size++;
		data[data_size] = F;
		data_size++;
		write(write_fd, data, data_size);

		//pos = (pos + 1) % 2;

		//printf("Data layer reading 'supervision frame' from the serial conexion.\n");
		alarmOn();
		unsigned char ch;
		stateWrite = START;
		while (stateWrite != STOP_SM) {
			if (read(fd, &ch, 1) != 1)
				printf("A problem occurred reading a byte on 'llwrite'.\n");
			switch (stateWrite) {
			case START:
				if (ch == F)
					stateWrite = FLAG_RCV;
				//else stateWrite = START;
				break;
			case FLAG_RCV:
				if (ch == A)
					stateWrite = A_RCV;
				else stateWrite = START;
				break;
			case A_RCV:
				if (ch == C_RR((pos + 1) % 2)){
					stateWrite = C_RCV;
					repete = FALSE;
				}
				else if (ch == C_REJ(pos) || ch == C_REJ((pos + 1) % 2)) {
					stateWrite = C_RCV;
					//pos = (pos + 1) % 2;	//to receive all the data again
					printf("ERROdddfsdfsdf\n");
					repete = TRUE;
				}
				else {
					repete = TRUE;
					stateWrite = START;
				}
				break;
			case C_RCV:
				if (ch == (A ^ C_RR((pos + 1) % 2)) || ch == (A ^ C_REJ(pos))){
					stateWrite = BCC1_RCV;
				}
				else {
					repete = TRUE;
					stateWrite = START;	
				}
				break;
			case BCC1_RCV:
				if (ch == F)
					stateWrite = STOP_SM;
				else{
					stateWrite = START;
					repete = TRUE;
				}
				break;
			}
		}
		alarmOff();
	}
	pos = (pos + 1) % 2;
	return 0;
}

int llclose(int porta) {
	write_fd = porta;

	data[0] = F;
	data[1] = A;
	data[2] = C_DISC;
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = DISC_AND_UA_SIZE;
	int tr_DISC = write(write_fd, data, data_size);

	alarmOn();
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
	alarmOff();

	data[0] = F;
	data[1] = A;
	data[2] = C_UA;
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = DISC_AND_UA_SIZE;
	int tr_UA = write(porta, data, data_size);

	return (tr_DISC == DISC_AND_UA_SIZE && tr_UA == DISC_AND_UA_SIZE) ? 0 : -1;
}

volatile int STOP = FALSE;






int writeToSerial(int fd, char fileName[]){

	//	find the size of the file
	unsigned long fileLegth = 10968;
	unsigned char appPacket[MAX_SIZE];
	int appPacketSize = 0;
	unsigned char * sizeInPackets = (unsigned char*)&fileLegth;
	
	// start packet 
	appPacket[0] = C_START;		//2
	appPacket[1] = FILE_SIZE_INDICATOR;	//0
	appPacket[2] = sizeof(unsigned long); // length é guardada num ulong
	appPacketSize = 3;
	
	int i;
	for(i = 0; i < sizeof(unsigned long); i++){
		appPacket[appPacketSize] = sizeInPackets[i];
		appPacketSize++;
	}
	
	
	appPacket[appPacketSize] = FILE_NAME_INDICATOR;	//1
	appPacketSize++;
	appPacket[appPacketSize] = strlen(fileName);
	appPacketSize++;
	for (i=0 ; i < strlen(fileName); i++){
		appPacket[appPacketSize] = (unsigned char) fileName[i];
		appPacketSize++;
	}
	
	llwrite(fd, appPacket, appPacketSize);
	printf("o primeiro e: %d\n",appPacket[0]);
	printf("enviou %d packets\n", appPacketSize);
	
	
	
	//sending packets
	int dataFd = open(fileName, O_RDONLY);
	unsigned char fileData[MAX_SIZE];
	unsigned char fileToSend[MAX_SIZE];
	i = 0;
	unsigned long j;
	unsigned long size_read = 0;
	unsigned char sequenceNum = 0;
	while((size_read = read(dataFd, fileData, 512))) {
		fileToSend[0] = 1;
		fileToSend[1] = sequenceNum;
		fileToSend[2] = (unsigned char) (size_read/256);
		fileToSend[3] = (unsigned char) (size_read % 256);
		for(j=0; j < size_read; j++)
			fileToSend[j+4] = fileData[j];
			
		llwrite(fd, fileToSend, size_read+4);
		i += size_read;
		sequenceNum++;
		printf("bytes enviados: %d\n", i);
	}
	close(dataFd);
	
	return 0;
}




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

	//gets(buf);

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


	writeToSerial(fd, "pinguim.gif");
	
	
	/*//teste
	dataFd = open("pinguim.gif", O_RDONLY);

	for (i = 0; i < MAX_SIZE && (size_read = read(dataFd, &fileData[i], 1)); i++){
		llwrite(fd, &fileData[i], size_read);
	}
	close(dataFd);
	//fim de teste*/

	//res = write(fd, buf, strlen(buf) + 1);
	//printf("%d bytes written\n", res);


	/*i = 0;
	while (STOP == FALSE) {
		res = read(fd, &buf[i], 1);
		if (buf[i] == '\0')
			STOP = TRUE;
		i += res;
	}
	printf("%s\n", buf);*/

	if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	llclose(fd);
	return 0;
}
