/*Non-Canonical Input Processing*/

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

int llopen(int port)
{
	printf("Opening connection.\n");
	write_fd = port;

	data[0] = F;
	data[1] = A_SENDER_TO_RECEIVER_CMD;
	data[2] = C_SET;
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = 5;
	int tr = write(write_fd, data, data_size);
	stats.sentBytes += data_size;
	stats.sentFrames++;

	unsigned char UA_char;
	int state = START;
	alarmOn();
	while (state != STOP_SM) {
		if (read(port, &UA_char, 1) != 1)    /* returns after 1 chars have been input */
			printf("A problem occurred reading a 'UA_char' on 'llopen'.\n");
		stats.receivedBytes++;
		switch (state) {
		case START:
			if (UA_char == F)
				state = FLAG_RCV;
			//else state = START;
			break;
		case FLAG_RCV:
			if (UA_char == A_RECEIVER_TO_SENDER_ANSWER)
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
			if (UA_char == (A_RECEIVER_TO_SENDER_ANSWER ^ C_UA))
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
	printf("Connection opened.\n");
	return (tr == data_size) ? port : -1;
}

volatile int pos = 0;	// can be 0 or 1, it is used by llwrite

int llwrite(int fd, unsigned char *buffer, int length)
{
	write_fd = fd;

	int repeat = TRUE;
	while (repeat == TRUE) {
		repeat = FALSE;
		// Header
		data[0] = F;
		data[1] = A_SENDER_TO_RECEIVER_CMD;
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
		int parity = INITIAL_PARITY;
		for (i = 0; i < length; i++) {
			parity = (parity ^ buffer[i]);
		}

		// Trailer;
		if (parity == ESC || parity == F) { // BCC2
			data[data_size] = ESC;
			data_size++;
			unsigned char flag = parity ^ 0x20;
			data[data_size] = flag;
			data_size++;
		}
		else {
			data[data_size] = parity;
			data_size++;
		}
		data[data_size] = F;
		data_size++;
		write(write_fd, data, data_size);
		stats.sentBytes += data_size;
		stats.sentFrames++;

		unsigned char ch;
		int state = START;
		alarmOn();
		while (state != STOP_SM) {
			if (read(fd, &ch, 1) != 1)
				printf("A problem occurred reading a byte on 'llwrite'.\n");
			stats.receivedBytes++;
			switch (state) {
			case START:
				if (ch == F)
					state = FLAG_RCV;
				//else state = START;
				break;
			case FLAG_RCV:
				if (ch == A_RECEIVER_TO_SENDER_ANSWER)
					state = A_RCV;
				else if (ch == F)
					state = FLAG_RCV;
				else {
					state = STOP_SM;
					repeat = TRUE;
				}
				break;
			case A_RCV:
				if (ch == C_RR((pos + 1) % 2)) {
					state = C_RCV;
					stats.receivedRR++;
				}
				else if (ch == C_REJ(pos)) {
					state = STOP_SM;
					repeat = TRUE;
					stats.receivedREJ++;
				}
				else if (ch == F)
					state = FLAG_RCV;
				else {
					state = STOP_SM;
					repeat = TRUE;
				}
				break;
			case C_RCV:
				if (ch == (A_RECEIVER_TO_SENDER_ANSWER ^ C_RR((pos + 1) % 2)) ||
					ch == (A_RECEIVER_TO_SENDER_ANSWER ^ C_REJ(pos)))
					state = BCC1_RCV;
				else if (ch == F)
					state = FLAG_RCV;
				else {
					state = STOP_SM;
					repeat = TRUE;
				}
				break;
			case BCC1_RCV:
				if (ch == F)
					state = STOP_SM;
				else {
					state = STOP_SM;
					repeat = TRUE;
				}
				break;
			}

			if (repeat == TRUE)
				tcflush(fd, TCIOFLUSH);
		}
		alarmOff();
	}
	stats.receivedFrames++;
	pos = (pos + 1) % 2;
	return length;
}

int llclose(int port) {
	printf("Closing connection.\n");
	write_fd = port;

	data[0] = F;
	data[1] = A_SENDER_TO_RECEIVER_CMD;
	data[2] = C_DISC;
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = 5;
	int tr_DISC = write(write_fd, data, data_size);
	stats.sentBytes += data_size;
	stats.sentFrames++;

	unsigned char DISC_char;
	int state = START;
	alarmOn();
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
			if (DISC_char == A_RECEIVER_TO_SENDER_CMD)
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
			if (DISC_char == (A_RECEIVER_TO_SENDER_CMD ^ C_DISC))
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
	stats.receivedFrames++;

	data[0] = F;
	data[1] = A_SENDER_TO_RECEIVER_CMD;
	data[2] = C_UA;
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = 5;
	int tr_UA = write(write_fd, data, data_size);
	stats.sentFrames++;
	stats.sentBytes += data_size;
	printf("Connection closed.\n");
	return (tr_DISC == 5 && tr_UA == 5) ? 0 : -1;
}

int writeToSerial(int fd, char filename[], int frame_length) {

	int dataFd = open(filename, O_RDONLY);
	if (dataFd == -1) {
		printf("Couldn't open the data file.\n");
		return -1;
	}
	if (llopen(fd) == -1) {
		printf("Error occurred executing 'llopen'.\n");
		return -1;
	}
	stats = initStatistics();

	struct stat st;
	fstat(dataFd, &st);
	unsigned long fileLength = st.st_size;
	stats.fileSize = fileLength;
	unsigned char appPacket[MAX_SIZE];
	int appPacketSize = 0;
	unsigned char *sizeInPackets = (unsigned char*)&fileLength;

	// start packet 
	appPacket[0] = C_START;
	appPacket[1] = FILE_SIZE_INDICATOR;
	appPacket[2] = sizeof(unsigned long); // 'length' is stored in an ulong
	appPacketSize = 3;

	unsigned long i;
	for (i = 0; i < (int)sizeof(unsigned long); i++) {
		appPacket[appPacketSize] = sizeInPackets[i];
		appPacketSize++;
	}

	appPacket[appPacketSize] = FILE_NAME_INDICATOR;
	appPacketSize++;
	unsigned int filenameLength = strlen(filename);
	appPacket[appPacketSize] = (unsigned char)filenameLength;	// allows names with 256 chars at maximum
	appPacketSize++;
	for (i = 0; i < filenameLength; i++) {
		appPacket[appPacketSize] = filename[i];
		appPacketSize++;
	}

	llwrite(fd, appPacket, appPacketSize);
	stats.sentPackets++;

	// sending data packets
	unsigned char fileData[MAX_APP_DATA_SIZE];
	unsigned char fileToSend[MAX_SIZE];
	unsigned int size_read = 0, sequenceNum = 0;
	unsigned long sentBytes = 0;
	while ((size_read = read(dataFd, fileData, frame_length))) {
		fileToSend[0] = C_DATA;
		fileToSend[1] = sequenceNum;
		fileToSend[2] = (unsigned char)(size_read / 256);
		fileToSend[3] = (unsigned char)(size_read % 256);
		for (i = 0; i < size_read; i++)
			fileToSend[i + 4] = fileData[i];

		llwrite(fd, fileToSend, size_read + 4);
		stats.sentPackets++;
		sentBytes += size_read;
		sequenceNum = (sequenceNum + 1) % 255;
		printf("Sent bytes: %lu.\n", sentBytes);
	}

	// end packet (similar to start packet)
	appPacket[0] = C_END;
	llwrite(fd, appPacket, appPacketSize);
	stats.sentPackets++;

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

	if ((argc != 7) ||
		((strcmp("/dev/ttyS0", argv[1]) != 0) &&
		(strcmp("/dev/ttyS1", argv[1]) != 0))) {
		printf("Usage:\tnserial SerialPort Filename BaudRate FrameLength AlarmCalls TimeOut\n\tex: nserial /dev/ttyS1 pinguim.gif 38400 512 3 3\n");
		showBaudrates();
		exit(-1);
	}

	char *serial_port = argv[1];
	char *filename = argv[2];
	tcflag_t baudrate;
	chooseBaudrate(argv[3], &baudrate);
	int frame_length = atoi(argv[4]);
	max_alarm_calls = atoi(argv[5]);
	time_out = atoi(argv[6]);

	if (frame_length < 1 || frame_length > MAX_APP_DATA_SIZE || max_alarm_calls < 0 || time_out <= 0) {
		printf("At least one value is invalid.\n");
		exit(-1);
	}

	/*
	  Open serial port device for reading and writing and not as controlling tty
	  because we don't want to get killed if linenoise sends CTRL-C.
	*/

	fd = open(serial_port, O_RDWR | O_NOCTTY);
	if (fd < 0) { perror(serial_port); exit(-1); }

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

	if (close(fd) != 0) { perror(argv[1]); }

	return 0;
}
