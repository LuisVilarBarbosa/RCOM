/*Non-Canonical Input Processing*/

#include "common.h"

volatile int alarm_on = FALSE, alarm_calls = 0, write_fd, data_size = 0;
int stateWrite = START, max_alarm_calls = 3, time_out = TIME_OUT;
unsigned char data[MAX_SIZE];

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
	if (alarm_on == TRUE) {
		alarm_calls++;
		write(write_fd, data, data_size);
		printf("Resend %d of the data.\n", alarm_calls);
		stateWrite = START;
		if (alarm_calls < max_alarm_calls)	// to resend the data 3 times
			alarm(time_out);
		else {
			alarm_on = FALSE;
			printf("All attempts to resend the data failed.\n");
			exit(1);
		}
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
				else {
					stateWrite = STOP_SM;
					repete = TRUE;
					tcflush(fd, TCIOFLUSH);
				}
				break;
			case A_RCV:
				if (ch == C_RR((pos + 1) % 2)) {
					stateWrite = C_RCV;
					repete = FALSE;
				}
				else if (ch == C_REJ(pos) || ch == C_REJ((pos + 1) % 2)) {
					stateWrite = STOP_SM;
					//pos = (pos + 1) % 2;	//to receive all the data again
					repete = TRUE;
					tcflush(fd, TCIOFLUSH);
				}
				//else if (ch == F)
				//	stateWrite = FLAG_RCV;
				else stateWrite = START;
				break;
			case C_RCV:
				if (ch == (A ^ C_RR((pos + 1) % 2)) || ch == (A ^ C_REJ(pos))) {
					stateWrite = BCC1_RCV;
				}
				else {
					stateWrite = START;
				}
				break;
			case BCC1_RCV:
				if (ch == F)
					stateWrite = STOP_SM;
				else stateWrite = START;
				break;
			}
		}
		alarmOff();
	}
	pos = (pos + 1) % 2;
	return 0;
}

int llclose(int porta) {
	printf("A terminar ligação.\n");
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

	printf("Ligacao terminada\n");

	return (tr_DISC == DISC_AND_UA_SIZE && tr_UA == DISC_AND_UA_SIZE) ? 0 : -1;
}

int writeToSerial(int fd, char fileName[], int frame_length) {

	int dataFd = open(fileName, O_RDONLY);

	if (dataFd == -1) {
		printf("O ficheiro nao pode ser aberto\n");
		return -1;
	}

	if (llopen(fd) == -1)
		printf("Error occurred executing 'llopen'.\n");

	//	find the size of the file
	struct stat st;
	fstat(dataFd, &st);
	unsigned long fileLength = st.st_size;
	unsigned char appPacket[MAX_SIZE];
	int appPacketSize = 0;
	unsigned char * sizeInPackets = (unsigned char*)&fileLength;

	// start packet 
	appPacket[0] = C_START;
	appPacket[1] = FILE_SIZE_INDICATOR;
	appPacket[2] = sizeof(unsigned long); // 'length' is stored in an ulong
	appPacketSize = 3;

	int i;
	for (i = 0; i < sizeof(unsigned long); i++) {
		appPacket[appPacketSize] = sizeInPackets[i];
		appPacketSize++;
	}

	appPacket[appPacketSize] = FILE_NAME_INDICATOR;
	appPacketSize++;
	appPacket[appPacketSize] = strlen(fileName);
	appPacketSize++;
	for (i = 0; i < strlen(fileName); i++) {
		appPacket[appPacketSize] = (unsigned char)fileName[i];
		appPacketSize++;
	}

	llwrite(fd, appPacket, appPacketSize);

	//sending packets
	unsigned char fileData[MAX_SIZE];
	unsigned char fileToSend[MAX_SIZE];
	i = 0;
	unsigned long j;
	unsigned long size_read = 0;
	unsigned char sequenceNum = 0;
	while ((size_read = read(dataFd, fileData, frame_length))) {
		fileToSend[0] = 1;
		fileToSend[1] = sequenceNum;
		fileToSend[2] = (unsigned char)(size_read / 256);
		fileToSend[3] = (unsigned char)(size_read % 256);
		for (j = 0; j < size_read; j++)
			fileToSend[j + 4] = fileData[j];

		llwrite(fd, fileToSend, size_read + 4);
		i += size_read;
		sequenceNum++;
		printf("bytes enviados: %d\n", i);
	}
	close(dataFd);
	llclose(fd);
	return 0;
}

int main(int argc, char** argv)
{
	(void)signal(SIGALRM, answer_alarm);

	int fd;
	struct termios oldtio, newtio;

	if ((argc != 7) ||
		((strcmp("/dev/ttyS0", argv[1]) != 0) &&
		(strcmp("/dev/ttyS1", argv[1]) != 0))) {
		printf("Usage:\tnserial SerialPort Filename BaudRate FrameLength AlarmCalls TimeOut\n\tex: nserial /dev/ttyS1 pinguim.gif 38400 512 3 3\n");
		showBaudrates();
		exit(1);
	}

	char *serial_port = argv[1];
	char *filename = argv[2];
	tcflag_t baudrate;
	chooseBaudrate(argv[3], &baudrate);
	int frame_length = atoi(argv[4]);
	max_alarm_calls = atoi(argv[5]);
	time_out = atoi(argv[6]);

	if (frame_length < 1 || frame_length > MAX_SIZE - 20 || max_alarm_calls < 0 || time_out <= 0)
		printf("At least one value is invalid.\n");

	/*
	  Open serial port device for reading and writing and not as controlling tty
	  because we don't want to get killed if linenoise sends CTRL-C.
	*/

	fd = open(serial_port, O_RDWR | O_NOCTTY);
	if (fd < 0) { perror(filename); exit(-1); }

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

	if (writeToSerial(fd, filename, frame_length) == -1)
		return -1;

	if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	return 0;
}
