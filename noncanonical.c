/*Non-Canonical Input Processing*/

#include <time.h>
#include "common.h"

volatile int alarm_on = FALSE, alarm_calls = 0, write_fd, data_size = 0;
int max_alarm_calls = MAX_ALARM_CALLS, time_out = TIME_OUT;
unsigned char data[MAX_SIZE];
Statistics stats;

void alarmOn() {
	alarm_on = TRUE;
	alarm_calls = 0;
	alarm(time_out);
}

void alarmOff() {
	alarm_on = FALSE;
}

void answer_alarm()
{
	stats.timeouts++;
	if (alarm_on == TRUE) {
		alarm_calls++;
		write(write_fd, data, data_size);
		stats.sentBytes += data_size;
		stats.sentFrames++;
		printf("Resend %d of the data.\n", alarm_calls);
		if (alarm_calls < max_alarm_calls)	// to resend the data 'max_alarm_calls' times
			alarm(time_out);
		else {
			alarm_on = FALSE;
			printf("All attempts to resend the data failed.\n");
			exit(-1);
		}
	}
}

void sendREJ(int fd, int position) {
	// Supervision frame in case of failure
	stats.sentREJ++;
	tcflush(fd, TCIOFLUSH);
	data[0] = F;
	data[1] = A_RECEIVER_TO_SENDER_ANSWER;
	data[2] = C_REJ(position);
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = 5;
	write(fd, data, data_size);
	stats.sentBytes += data_size;
	stats.sentFrames++;
}

int llopen(int port)
{
	printf("Opening connection.\n");

	unsigned char SET_char;
	int state = START;
	while (state != STOP_SM) {
		if (read(port, &SET_char, 1) != 1)    /* returns after 1 chars have been input */
			printf("A problem occurred reading a 'SET_char' on 'llopen'.\n");
		stats.receivedBytes++;
		switch (state) {
		case START:
			if (SET_char == F)
				state = FLAG_RCV;
			//else state = START;
			break;
		case FLAG_RCV:
			if (SET_char == A_SENDER_TO_RECEIVER_CMD)
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
			if (SET_char == (A_SENDER_TO_RECEIVER_CMD ^ C_SET))
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
	stats.receivedFrames++;

	data[0] = F;
	data[1] = A_RECEIVER_TO_SENDER_ANSWER;
	data[2] = C_UA;
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = 5;
	int tr = write(port, data, data_size);
	stats.sentBytes += data_size;
	stats.receivedFrames++;
	printf("Connection opened.\n");
	return (tr == data_size) ? port : -1;
}

volatile int pos = 0;

int llread(int fd, unsigned char *buffer)
{
	write_fd = fd;

	int state = START;
	int readBytes = 0;
	int parity = INITIAL_PARITY;
	unsigned char ch;

	alarmOn();
	while (state != STOP_SM) {
		if (read(fd, &ch, 1) != 1)
			printf("A problem occurred reading on 'llread'.\n");
		stats.receivedBytes++;

		if ((rand() % 10000) == 1) {	// generate random loss
			ch = ch ^ (rand() % 32);
			printf("Pseudo-random information frame byte loss generated.\n");
		}
		if ((rand() % 10000) == 10) {	// generate random error
			ch = ch ^ (rand() % 32);
			printf("Pseudo-random information frame byte error generated.\n");
		}

		switch (state) {
		case START:
			readBytes = 0;
			parity = INITIAL_PARITY;
			if (ch == F)
				state = FLAG_RCV;
			//else state = START;
			break;
		case FLAG_RCV:
			if (ch == A_SENDER_TO_RECEIVER_CMD)
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
			if (ch == (A_SENDER_TO_RECEIVER_CMD ^ C_SEND(pos)))	// BCC1
				state = RCV_DATA;
			else if (ch == F)
				state = FLAG_RCV;
			else state = START;
			break;
		case RCV_DATA:
			if (ch == F) {
				int antParity = (parity ^ buffer[readBytes - 1]);
				if (buffer[readBytes - 1] == antParity) {	// BCC2
					state = STOP_SM;
					readBytes--;	// delete BCC2 from buffer
				}
				else {
					sendREJ(fd, pos);
					state = START;
				}
			}
			else {
				if (ch == ESC) {
					if (read(fd, &ch, 1) != 1)
						printf("A problem occurred reading on 'llread'.\n");
					if (ch == F) {
						sendREJ(fd, pos);
						state = START;
						break;
					}
					stats.receivedBytes++;
					ch = ch ^ 0x20;
				}
				buffer[readBytes] = ch;
				parity = (parity ^ buffer[readBytes]);
				readBytes++;
			}
			break;
		}
	}
	alarmOff();
	stats.receivedFrames++;

	pos = (pos + 1) % 2;

	// Supervision frame in case of success
	data[0] = F;
	data[1] = A_RECEIVER_TO_SENDER_ANSWER;
	data[2] = C_RR(pos);
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = 5;
	if ((rand() % 10000) == 1)	// generate random loss
		printf("Pseudo-random RR frame loss generated.\n");
	else if ((rand() % 10000) == 10) {	// generate random error
		int x = rand() % data_size;
		unsigned char origDataX = data[x];
		data[x] = data[x] ^ (rand() % 32);
		printf("Pseudo-random RR frame byte error generated.\n");
		write(write_fd, data, data_size);
		data[x] = origDataX;	// to 'alarm_answer' resend the correct value
	}
	else
		write(write_fd, data, data_size);
	stats.sentRR++;
	stats.sentBytes += data_size;
	stats.sentFrames++;

	return readBytes;
}

int llclose(int port) {
	printf("Closing connection.\n");
	write_fd = port;

	unsigned char DISC_char;
	int state = START;
	while (state != STOP_SM) {
		if (read(port, &DISC_char, 1) != 1)    /* returns after 1 chars have been input */
			printf("A problem occurred reading a 'DISC_char' on 'llclose'.\n");
		stats.receivedBytes++;
		switch (state) {
		case START:
			if (DISC_char == F)
				state = FLAG_RCV;
			//else state = START;
			break;
		case FLAG_RCV:
			if (DISC_char == A_SENDER_TO_RECEIVER_CMD)
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
			if (DISC_char == (A_SENDER_TO_RECEIVER_CMD ^ C_DISC))
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
	stats.receivedFrames++;

	data[0] = F;
	data[1] = A_RECEIVER_TO_SENDER_CMD;
	data[2] = C_DISC;
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = 5;
	int tr_DISC = write(write_fd, data, data_size);
	stats.sentBytes += data_size;
	stats.sentFrames++;

	unsigned char UA_char;
	state = START;
	alarmOn();
	while (state != STOP_SM) {
		if (read(write_fd, &UA_char, 1) != 1)    /* returns after 1 chars have been input */
			printf("A problem occurred reading a 'UA_char' on 'llclose'.\n");
		stats.receivedBytes++;
		switch (state) {
		case START:
			if (UA_char == F)
				state = FLAG_RCV;
			//else state = START;
			break;
		case FLAG_RCV:
			if (UA_char == A_SENDER_TO_RECEIVER_CMD)
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
			if (UA_char == (A_SENDER_TO_RECEIVER_CMD ^ C_UA))
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
	stats.receivedFrames++;
	printf("Connection closed.\n");
	return (tr_DISC == data_size) ? 0 : -1;
}

int receiveAppControlPacket(int fd, int expected_C, unsigned long *file_size, char *filename)
{
	unsigned char appControlPacket[MAX_SIZE];
	int sizeAppCtlPkt = llread(fd, appControlPacket);
	if (appControlPacket[0] != expected_C) {
		printf("Error receiving the control packet of the aplication.\n");
		return -1;
	}

	int i = 1, k, length, initialN;
	while (i < sizeAppCtlPkt) {
		switch (appControlPacket[i]) {
		case FILE_SIZE_INDICATOR:
			i++;
			*file_size = 0;
			length = appControlPacket[i];
			initialN = i;
			for (k = 1; k <= length; k++) {
				*file_size += ((appControlPacket[initialN + k]) << ((k - 1) * 8));
				i++;
			}
			i++;
			break;
		case FILE_NAME_INDICATOR:
			i++;
			length = appControlPacket[i];
			initialN = i;
			for (k = 1; k <= length; k++) {
				filename[k - 1] = appControlPacket[initialN + k];
				i++;
			}
			i++;
			filename[length] = '\0';
			break;
		default:
			printf("Error receiving the control fields.\n");
			return -1;
		}
	}
	stats.receivedPackets++;
	stats.fileSize = *file_size;

	return 0;
}

int receiveFromSerial(int fd) {
	if (llopen(fd) == -1) {
		printf("Error occurred executing 'llopen'.\n");
		return -1;
	}

	stats = initStatistics();

	unsigned long file_size = 0;
	char filename[256];	// Control frame 'length' field has only 1 byte -> maximum of 255
	if (receiveAppControlPacket(fd, C_START, &file_size, filename) != 0)
		return -1;

	int dataFd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (dataFd == -1) {
		printf("Unable to create the data file.\n");
		if (llclose(fd) != 0)
			printf("Error occurred executing 'llclose'.\n");
		return -1;
	}

	unsigned char llreadData[MAX_SIZE];

	unsigned long i = 0, receivedBytes, sequenceNum = 0, numOcte;

	while (i < file_size) {
		receivedBytes = llread(fd, llreadData);
		stats.receivedPackets++;

		if (llreadData[0] != C_DATA) {
			printf("Erro receiving data packets: control field wrong.\n");
			return -1;
		}

		if (llreadData[1] != sequenceNum) {
			printf("Erro receiving data packets: sequence number wrong.\n");
			return -1;
		}

		numOcte = llreadData[2] * 256 + llreadData[3];
		if (numOcte != receivedBytes - 4) {
			printf("Expected %lu bytes, received %lu.\n", numOcte, receivedBytes - 4);
			return -1;
		}
		write(dataFd, &llreadData[4], numOcte);

		i += numOcte;
		sequenceNum = (sequenceNum + 1) % 255;
		printf("Received bytes: %lu.\n", i);
	}

	unsigned long file_size2 = 0;
	char filename2[256];	// Control frame 'length' field has only 1 byte -> maximum of 255
	if (receiveAppControlPacket(fd, C_END, &file_size2, filename2) != 0)
		return -1;
	if (file_size != file_size2 || strcmp(filename, filename2) != 0)
		printf("File size and/or filename mismatch:\nFirst received: %lu - %s\nLast received: %lu - %s\n", file_size, filename, file_size2, filename2);

	close(dataFd);
	if (llclose(fd) != 0)
		printf("Error occurred executing 'llclose'.\n");
	printStatistics(stats);
	return 0;
}


int main(int argc, char** argv)
{
	(void)signal(SIGALRM, answer_alarm);

	int fd;
	struct termios oldtio, newtio;

	if ((argc != 5) ||
		((strcmp("/dev/ttyS0", argv[1]) != 0) &&
		(strcmp("/dev/ttyS1", argv[1]) != 0))) {
		printf("Usage:\tnserial SerialPort BaudRate AlarmCalls TimeOut\n\tex: nserial /dev/ttyS1 38400 3 3\n");
		showBaudrates();
		exit(-1);
	}

	char *serial_port = argv[1];
	tcflag_t baudrate;
	chooseBaudrate(argv[2], &baudrate);
	max_alarm_calls = atoi(argv[3]);
	time_out = atoi(argv[4]);

	if (max_alarm_calls < 0 || time_out <= 0) {
		printf("At least one value is invalid.\n");
		exit(-1);
	}

	/*
	  Open serial port device for reading and writing and not as controlling tty
	  because we don't want to get killed if linenoise sends CTRL-C.
	*/

	fd = open(serial_port, O_RDWR | O_NOCTTY);
	if (fd < 0) { perror(argv[1]); exit(-1); }

	if (tcgetattr(fd, &oldtio) == -1) { /* save current port settings */
		perror("tcgetattr");
		exit(-1);
	}

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = baudrate | CS8 | CLOCAL | CREAD;
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

	srand((unsigned)time(NULL));

	if (receiveFromSerial(fd) == -1)
		return -1;

	if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	return 0;
}
