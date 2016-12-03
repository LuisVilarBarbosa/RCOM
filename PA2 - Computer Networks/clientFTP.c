#include <netdb.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#define STRINGS_SIZE 256
#define START_SM 0
#define USER_SM 1
#define PASSWORD_SM 2
#define HOST_SM 3
#define URL_PATH_SM 4
#define STOP_SM 5
#define ERROR_SM 6
#define SERVER_PORT 6000

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

int main(int argc, char** argv)
{
	if (argc != 2) {
		printf("Usage: %s <ftp address>\n", argv[0]);
		exit(1);
	}

	// split ftp address
	char user[STRINGS_SIZE], password[STRINGS_SIZE], hostname[STRINGS_SIZE];
	char url_path[STRINGS_SIZE], state = START_SM;
	size_t startPos, endPos;

	while (state != STOP_SM) {
		switch (state)
		{
		case START_SM:
			if (strncasecmp(argv[1], "ftp://", 6) == 0) {
				state = USER_SM;
				startPos = 6;
			}
			else {
				printf("Invalid URL syntax.\n");
				state = ERROR_SM;
			}
			break;
		case USER_SM:
			endPos = find_pos(argv[1], ':', startPos);
			if (argv[1][endPos] == '\0')
				state = HOST_SM;	// No user neither password indicated
			else {
				copy_string(user, &argv[1][startPos], endPos - startPos);
				printf("User: %s\n", user);
				state = PASSWORD_SM;
				startPos = endPos + 1;
			}
			break;
		case PASSWORD_SM:
			endPos = find_pos(argv[1], '@', startPos);
			if (argv[1][endPos] == '\0') {
				printf("'@' not found.\n");
				state = ERROR_SM;
			}
			else {
				copy_string(password, &argv[1][startPos], endPos - startPos);
				printf("Password: %s\n", password);
				state = HOST_SM;
				startPos = endPos + 1;
			}
			break;
		case HOST_SM:
			endPos = find_pos(argv[1], '/', startPos);
			if (argv[1][endPos] == '\0') {
				printf("Host not found.\n");
				state = ERROR_SM;
			}
			else {
				copy_string(hostname, &argv[1][startPos], endPos - startPos);
				printf("Host name: %s\n", hostname);
				state = URL_PATH_SM;
				startPos = endPos + 1;
			}
			break;
		case URL_PATH_SM:
			copy_string(url_path, &argv[1][startPos], STRINGS_SIZE);
			printf("URL path: %s\n", url_path);
			state = STOP_SM;
			break;
		case ERROR_SM:
			printf("URL syntax: 'ftp://[<user>:<password>@]<host>/<url-path>'\n");
			// state = STOP_SM;
			exit(1);
			break;
		default:
			printf("Problem executing URL split state machine.\n");
			state = ERROR_SM;
			break;
		}
	}

	// get host by name
	struct hostent *h;
	if ((h = gethostbyname(hostname)) == NULL) {
		herror("gethostbyname");
		exit(1);
	}
	printf("Host name  : %s\n", h->h_name);
	char *server_address = inet_ntoa(*((struct in_addr *)h->h_addr));
	printf("IP Address : %s\n", server_address);

	// connect via socket
	int	sockfd;
	struct	sockaddr_in server_addr;
	char	buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";
	int	bytes;

	/*server address handling*/
	bzero((char*)&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(server_address);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(SERVER_PORT);		/*server TCP port must be network byte ordered */
													/*open an TCP socket*/
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket()");
		exit(0);
	}
	/*connect to the server*/
	if (connect(sockfd,
		(struct sockaddr *)&server_addr,
		sizeof(server_addr)) < 0) {
		perror("connect()");
		exit(0);
	}
	/*send a string to the server*/
	bytes = write(sockfd, buf, strlen(buf));
	printf("Bytes escritos %d\n", bytes);

	close(sockfd);

	// login host
	// enter passive mode
	// get path
	// receive data
	// indicate success(file saved in current working directory)
	// or indicate un-success(indicating failing phase) where it occurs

	return 0;
}