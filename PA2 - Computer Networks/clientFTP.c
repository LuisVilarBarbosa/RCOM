#include <netdb.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define BUFFER_SIZE 256
#define BUFFER_LENGTH BUFFER_SIZE - 1
#define START_SM 0
#define USER_SM 1
#define PASSWORD_SM 2
#define HOST_SM 3
#define URL_PATH_SM 4
#define STOP_SM 5
#define ERROR_SM 6
#define DEFAULT_FTP_PORT 21

size_t find_pos(char *str, char toFind, size_t startPos)
{
	int endPos;
	for (endPos = startPos; str[endPos] != '\0'; endPos++)
		if (str[endPos] == toFind)
			break;
	return endPos;
}

// substitutes 'strncpy', because 'strncpy' does not add '\0' when it is not present
void copy_string(char *dest, char *src, size_t n)
{
	int i;
	for (i = 0; i < n && src[i] != '\0'; i++)
		dest[i] = src[i];
	dest[i] = '\0';
}


void split_ftp_address(char *ftp_address, char *user, char *password, char *hostname, char *url_path)
{
	char state = START_SM;
	size_t startPos, endPos;

	while (state != STOP_SM) {
		switch (state)
		{
		case START_SM:
			if (strncasecmp(ftp_address, "ftp://", 6) == 0) {
				state = USER_SM;
				startPos = 6;
			}
			else {
				printf("Invalid URL syntax.\n");
				state = ERROR_SM;
			}
			break;
		case USER_SM:
			endPos = find_pos(ftp_address, ':', startPos);
			if (ftp_address[endPos] == '\0') {
				sprintf(user, "%s", "anonymous");
				sprintf(password, "%s", "rcom@fe.up.pt");
				state = HOST_SM;	// No user neither password indicated
			}
			else {
				copy_string(user, &ftp_address[startPos], endPos - startPos);
				printf("User: %s\n", user);
				state = PASSWORD_SM;
				startPos = endPos + 1;
			}
			break;
		case PASSWORD_SM: ;
  			char *pch;
  			pch=strrchr(ftp_address,'@');
  			int indexOfLastAt = pch - ftp_address+1;

			endPos = find_pos(ftp_address, '@', startPos);
			if (ftp_address[endPos] == '\0') {
				printf("'@' not found.\n");
				state = ERROR_SM;
			}
			else {
				copy_string(password, &ftp_address[startPos], indexOfLastAt - 1 - startPos);
				printf("Password: %s\n", password);
				state = HOST_SM;
				startPos = indexOfLastAt;
			}
			break;
		case HOST_SM:
			endPos = find_pos(ftp_address, '/', startPos);
			if (ftp_address[endPos] == '\0') {
				printf("Host not found.\n");
				state = ERROR_SM;
			}
			else {
				copy_string(hostname, &ftp_address[startPos], endPos - startPos);
				printf("Host name: %s\n", hostname);
				state = URL_PATH_SM;
				startPos = endPos + 1;
			}
			break;
		case URL_PATH_SM:
			copy_string(url_path, &ftp_address[startPos], BUFFER_LENGTH);
			printf("URL path: %s\n", url_path);
			state = STOP_SM;
			break;
		case ERROR_SM:
			printf("URL syntax: 'ftp://[<user>:<password>@]<host>/<url-path>'\n");
			// state = STOP_SM;
			exit(-1);
			break;
		default:
			printf("Problem executing URL split state machine.\n");
			state = ERROR_SM;
			break;
		}
	}
}

struct sockaddr_in generate_sockaddr_in(char *server_address, uint16_t server_port)
{
	struct sockaddr_in server_addr;
	/*server address handling*/
	bzero((char*)&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(server_address);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(server_port);		/*server TCP port must be network byte ordered */
	return server_addr;
}

int new_connection(char *server_address, uint16_t server_port)
{
	int sockfd;
	struct sockaddr_in server_addr = generate_sockaddr_in(server_address, server_port);
	/*open an TCP socket*/
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket()");
		exit(-1);
	}
	/*connect to the server*/
	if (connect(sockfd,
		(struct sockaddr *)&server_addr,
		sizeof(server_addr)) < 0) {
		perror("connect()");
		exit(-1);
	}
	return sockfd;
}

int new_passive_connection(char *str)
{
	int ip[4];
	int port[2];
	sscanf(&str[find_pos(str, '(', 0)], "(%d,%d,%d,%d,%d,%d)", &ip[0], &ip[1], &ip[2], &ip[3], &port[0], &port[1]);
	char server_address[16];
	sprintf(server_address, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	uint16_t server_port = (port[0] << 8) + port[1];
	return new_connection(server_address, server_port);
}

void write_to_socket(int sockfd, char *str)
{
	size_t nBytes = strlen(str);
	if (write(sockfd, str, nBytes) != nBytes) {
		perror("write()");
		printf("Error writing to the socket: %s", str);
		exit(-1);
	}
	printf("> %s", str);
}

void read_from_socket(int sockfd, char *str)
{
	int bytes = read(sockfd, str, BUFFER_LENGTH);
	if (bytes < 0) {
		perror("read()");
		printf("Error reading from the socket.\n");
		exit(-1);
	}
	str[bytes] = '\0';
	printf("< %s", str);
}

void verify_answer(int sockfd, int expectedAnswer, char *str)
{
	read_from_socket(sockfd, str);
	int val = atoi(str);
	if (val != expectedAnswer) {
		printf("Expected answer code '%d', but received '%d'.\n", expectedAnswer, val);
		exit(-1);
	}
}

int main(int argc, char** argv)
{  	
	if (argc != 2) {
		printf("Usage: %s <ftp address>\n", argv[0]);
		exit(-1);
	}

	char user[BUFFER_SIZE], password[BUFFER_SIZE], hostname[BUFFER_SIZE], url_path[BUFFER_SIZE];
	split_ftp_address(argv[1], user, password, hostname, url_path);

	// get host by name
	struct hostent *h;
	if ((h = gethostbyname(hostname)) == NULL) {
		herror("gethostbyname");
		exit(-1);
	}
	printf("Host name  : %s\n", h->h_name);
	char *server_address = inet_ntoa(*((struct in_addr *)h->h_addr));
	printf("IP Address : %s\n", server_address);

	// connect via socket
	int	sockfd = new_connection(server_address, DEFAULT_FTP_PORT);

	char buf[BUFFER_SIZE];
	verify_answer(sockfd, 220, buf);

	// login host
	sprintf(buf, "user %s\r\n", user);
	write_to_socket(sockfd, buf);
	verify_answer(sockfd, 331, buf);

	sprintf(buf, "pass %s\r\n", password);
	write_to_socket(sockfd, buf);
	verify_answer(sockfd, 230, buf);

	// enter passive mode
	sprintf(buf, "pasv\r\n");
	write_to_socket(sockfd, buf);
	verify_answer(sockfd, 227, buf);

	if (close(sockfd) < 0)
		perror("close()");

	sockfd = new_passive_connection(buf);

	// get path
	sprintf(buf, "retr /%s\r\n", url_path);
	write_to_socket(sockfd, buf);
	verify_answer(sockfd, 125, buf);

	// receive data
	char *filename = strrchr(url_path, '/') + 1;
	int bytes = -1, fd = open(filename, O_CREAT | O_WRONLY | O_EXCL | O_TRUNC);
	if (fd < 0) {
		perror("open()");
		printf("Error opening the file '%s'.\n", filename);
		bytes = 0;
	}
	while (bytes) {
		if ((bytes = read(sockfd, buf, BUFFER_SIZE)) < 0) {
			perror("read()");
			bytes = 0;
		}
		if (write(fd, buf, bytes) != bytes) {
			perror("write()");
			bytes = 0;
		}
	}
	if (close(fd) < 0)
		perror("close()");

	// close connection
	sprintf(buf, "quit\r\n");
	write_to_socket(sockfd, buf);
	verify_answer(sockfd, 221, buf);

	if (close(sockfd) < 0)
		perror("close()");

	// indicate success(file saved in current working directory)
	// or indicate un-success(indicating failing phase) where it occurs

	return 0;
}
