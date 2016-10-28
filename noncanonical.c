/*Non-Canonical Input Processing*/

#include <time.h>
#include "common.h"

volatile int alarm_on = FALSE, alarm_calls = 0, write_fd, data_size = 0;
unsigned char data[MAX_SIZE], max_alarm_calls = 3, time_out = TIME_OUT;
int stateRead = START, readNumB = 0, parityRead = 0xff;

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
		printf("Resend %d of the data.\n", alarm_calls);
		stateRead = START;
		readNumB = 0;
		parityRead = 0xff;
		if (alarm_calls < max_alarm_calls)	// to resend the data 3 times
			alarm(time_out);
		else {
			alarm_on = FALSE;
			printf("All attempts to resend the data failed.\n");
			exit(1);
		}
	}
}

void sendREJ(int fd, int position){
	// Supervision frame in case of failure
	stats.sentREJ++;
	tcflush(fd, TCIOFLUSH);
	stateRead = START;
	readNumB = 0;
	parityRead = 0xff;
	unsigned char dataREJ[5];
	//printf("sendREJ chamada\n");
	dataREJ[0] = F;
	dataREJ[1] = A;
	dataREJ[2] = C_REJ(position);
	dataREJ[3] = dataREJ[1] ^ dataREJ[2];
	dataREJ[4] = F;
	int dataREJ_size = 5;
	write(fd, dataREJ, dataREJ_size);
	stats.sentBytes += dataREJ_size;

}

int llopen(int porta /*, TRANSMITTER | RECEIVER*/)
{
	unsigned char SET_char;
	int state = START;
	while (state != STOP_SM) {
		if (read(porta, &SET_char, 1) != 1)    /* returns after 1 chars have been input */
			printf("A problem occurred reading a 'SET_char' on 'llopen'.\n");
			stats.receivedBytes++;
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
	stats.receivedFrames++;

	data[0] = F;
	data[1] = A;
	data[2] = C_UA;
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = SET_AND_UA_SIZE;
	int tr = write(porta, data, data_size);
	stats.sentBytes += data_size;
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
	
	while (stateRead != STOP_SM) {
		if (read(fd, &ch, 1) != 1)
			printf("A problem occurred reading on 'llread'.\n");
		stats.receivedBytes++;
	       
		if((rand() % 10000) == 1){	// generate random error
		  ch = ch ^ 0xb5;printf("Gerou erro.\n");}
		switch (stateRead) {
		case START:
			if (ch == F)
				stateRead = FLAG_RCV;
			else{
				sendREJ(fd, pos);			
				stateRead = START;
			}
			break;
		case FLAG_RCV:
			if (ch == A)
				stateRead = A_RCV;
			else{ 
				sendREJ(fd, pos);		
				stateRead = START;
			}
			break;
		case A_RCV:
			if (ch == C_SEND(pos))
				stateRead = C_RCV;
			else{
					sendREJ(fd, pos);
					stateRead = START;
			}
			break;
		case C_RCV:
			if (ch == (A ^ C_SEND(pos)))	// BCC1
				stateRead = RCV_DATA;
			else{
				 sendREJ(fd, pos);
				 stateRead = START;
				}
			break;
		case RCV_DATA:
			if (ch == parityRead) {	// BCC2
				stateRead = BCC2_RCV;
				antCh = ch;
			}
			else if(ch == F){
				sendREJ(fd, pos);
				stateRead = START;
			}else {
				if (ch == ESC) {
					if (read(fd, &ch, 1) != 1)
						printf("A problem occurred reading on 'llread'.\n");
						if(ch == F){
								sendREJ(fd, pos);
								stateRead = START;
								break;
							}
					stats.receivedBytes++;
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
							if(ch == F){
								sendREJ(fd, pos);
								stateRead = START;
								break;
							}
						stats.receivedBytes++;
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
	stats.receivedFrames++;


	pos = (pos + 1) % 2;

	// Supervision frame in case of success
	data[0] = F;
	data[1] = A;
	data[2] = C_RR(pos);
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = 5;
	write(write_fd, data, data_size);
	stats.sentRR++;
	stats.sentBytes += data_size;
	return readNumB;
}

int llclose(int porta) {
	printf("A terminar ligacao\n");
	int write_fd = porta;
	unsigned char DISC_char;
	int state = START;
	while (state != STOP_SM) {
		if (read(porta, &DISC_char, 1) != 1)    /* returns after 1 chars have been input */
			printf("A problem occurred reading a 'DISC_char' on 'llclose'.\n");
		stats.receivedBytes++;
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
	stats.receivedFrames++;

	data[0] = F;
	data[1] = A;
	data[2] = C_DISC;
	data[3] = data[1] ^ data[2];
	data[4] = F;
	data_size = DISC_AND_UA_SIZE;
	int tr_DISC = write(porta, data, data_size);
	stats.sentBytes += data_size;
	
	alarmOn();
	unsigned char UA_char;
	state = START;
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
	stats.receivedFrames++;
	alarmOff();
	printf("Ligacao terminada\n");
	return (tr_DISC == DISC_AND_UA_SIZE) ? 0 : -1;
}


int receiveFromSerial(int fd){
  	if (llopen(fd) == -1)
	  printf("Error occurred executing 'llopen'.\n");
	initStatistics();
	unsigned char appControlPacket[MAX_SIZE];
	int sizeAppCtlPkt = llread(fd, appControlPacket);
	int sizeOfFile = 0;
	char nameOfFile[250];
	if(appControlPacket[0] != C_START){
		printf("Erro receiving the Control packet of the aplication.\n");
		return -1;
	}
	
	int i = 1;
	int k;
	int numLength;
	int initialN;
	while(i < sizeAppCtlPkt){
	switch (appControlPacket[i]) {
		case FILE_SIZE_INDICATOR:
			i++;
			sizeOfFile = 0;
			numLength = appControlPacket[i];
			initialN = i;
			for(k = 1; k <= numLength; k++){
				sizeOfFile += ((appControlPacket[initialN + k]) << ((k-1)*8));
				i++;
			}
			i++;
			break;
		case FILE_NAME_INDICATOR:
			i++;
			numLength = appControlPacket[i];
			initialN = i;
			for(k = 1; k <= numLength; k++){
				nameOfFile[k-1] += appControlPacket[initialN + k];
				i++;
			}
			i++;
			break;
		default:
			printf("Erro receiving os the control fields.\n");
			break;
		}
	}

	int dataFd = open(nameOfFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if(dataFd == -1){
    printf("Unable to create data file.\n");
    llclose(fd);
    return -1;
  }
	stats.receivedPackets++;
	stats.fileSize = sizeOfFile;
	unsigned char fileData[MAX_SIZE];
	unsigned char readData[MAX_SIZE];

	i = 0;
	int j;
	unsigned char sequenceNum = 0;
	unsigned long nunOcte;

	while (i < sizeOfFile){
		llread(fd, fileData);
		stats.receivedPackets++;
		
		if(fileData[0] != 1){
			printf("Erro receiving data packets: control field wrong.\n");
			return -1;
		}
			
		if(fileData[1] != sequenceNum){
			printf("Erro receiving data packets: sequence number wrong.\n");
			return -1;
		}
		
		nunOcte = fileData[2]*256 + fileData[3];
		j = 0;
		while(j < nunOcte){
			readData[i+j] = fileData[j+4];
			j++;
		}
		
		i += nunOcte;
		sequenceNum++;
		printf("bytes received: %d\n", i);
	}
	
	write(dataFd, readData, sizeOfFile);
	close(dataFd);
	llclose(fd);
	printStatistics();
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
		exit(1);
	}

	char *serial_port = argv[1];
	tcflag_t baudrate;
	chooseBaudrate(argv[2], &baudrate);
	max_alarm_calls = atoi(argv[3]);
	time_out = atoi(argv[4]);

	if (max_alarm_calls < 0 || time_out <= 0)
		printf("At least one value is invalid.\n");

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

	receiveFromSerial(fd);

	tcsetattr(fd, TCSANOW, &oldtio);

	return 0;
}
