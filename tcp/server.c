#include <sys/socket.h>
#include <netinet/in.h>

#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>

#define STATUS_user_not_logged_in 0
#define STATUS_user_need_password 1
#define STATUS_user_logged_in 2
#define MODE_transfer_undefined 0
#define MODE_transfer_port 1
#define MODE_transfer_pasv 2

int status_user = STATUS_user_not_logged_in;
int mode_transfer = MODE_transfer_undefined;
char *user_name;
char *user_email;

int listenfd, connfd; // 监听socket和连接socket不一样，后者用于数据传输
struct sockaddr_in addr;
int data_listen;

char workdir[256] = "/home/ftp";
char local_addr[15] = "172,24,153,139";

struct sockaddr_in port_addr_data;

int readMsg(int fd, char *sentence, int *len)
{
	// 榨干socket传来的内容
	int p = 0;
	while (1)
	{
		int n = read(fd, sentence + p, 8191 - p);
		if (n < 0)
		{
			printf("Error read(): %s(%d)\r\n", strerror(errno), errno);
			close(fd);
			continue;
		}
		else if (n == 0)
		{
			break;
		}
		else
		{
			p += n;
			while (sentence[p - 1] == 0)
			{
				p--;
			}
			if (sentence[p - 1] == '\n')
			{
				break;
			}
		}
	}
	// socket接收到的字符串并不会添加'\0'
	sentence[p - 1] = '\0';
	if (len != NULL)
		*len = p - 1;
}

int writeMsg(int fd, char *sentence, int len)
{
	if (len == 0)
		len = strlen(sentence);
	// 发送字符串到socket
	int p = 0;
	while (p < len)
	{
		int n = write(fd, sentence + p, len + 1 - p);
		if (n < 0)
		{
			printf("Error write(): %s(%d)\r\n", strerror(errno), errno);
			return 1;
		}
		else
		{
			p += n;
		}
	}
}

int strip(char *sentence, int len)
{
	if (len == 0)
		len = strlen(sentence);
	int p = len - 1;
	while (sentence[p] == ' ' || sentence[p] == '\n' || sentence[p] == '\r')
	{
		p--;
	}
	sentence[p + 1] = '\0';
	return p + 1;
}

int arguments_break(char *arguments, char *next)
{
	char *p = arguments;
	int count = 0;
	while (*p != '\0')
	{
		if (*p == ' ')
			count++;
		p++;
	}
	if (count == 0)
	{
		return 0;
	}
	else
	{
		strncpy(next, arguments, p - arguments);
		return p - arguments;
	}
}

int proc_USER(char *arguments)
{
	if (status_user != STATUS_user_not_logged_in)
	{
		writeMsg(connfd, "503 Bad sequence of commands.\r\n", 0);
	}
	char next[256];
	int len = arguments_break(arguments, next);
	if (len != 0)
	{
		writeMsg(connfd, "501 Syntax error in parameters or arguments.\r\n", 0);
	}
	else
	{
		if (strcmp(arguments, "anonymous") == 0)
		{
			status_user = STATUS_user_need_password;
			user_name = malloc(10 * sizeof(char));
			strcpy(user_name, "anonymous");
			writeMsg(connfd, "331 Guest login ok, send your complete e-mail address as password.\r\n", 0);
		}
		else
		{
			// user not exist
			writeMsg(connfd, "530 It is not a valid user.\r\n", 0);
		}
	}
}

int proc_PASS(char *arguments)
{
	if (status_user != STATUS_user_need_password)
	{
		writeMsg(connfd, "503 Bad sequence of commands.\r\n", 0);
	}
	else if (status_user == STATUS_user_need_password)
	{
		if (strcmp(user_name, "anonymous") == 0)
		{
			user_email = malloc(strlen(arguments) * sizeof(char));
			strcpy(user_email, arguments);
			status_user = STATUS_user_logged_in;
			char welcome[512];
			sprintf(welcome, "230-\r\n230-Welcome to\r\n230- School of Software\r\n230- FTP Archives at ftp.ssast.org\r\n230-\r\n230-This site is provided as a public service by School of\r\n230-Software. Use in violation of any applicable laws is strictly\r\n230-prohibited. We make no guarantees, explicit or implicit, about the\r\n230-contents of this site. Use at your own risk.\r\n230-\r\n230 Guest login ok, access restrictions apply.\r\n");
			writeMsg(connfd, welcome, 0);
		}
	}
}

int proc_PORT(char *argumemnts)
{
	if (status_user != STATUS_user_logged_in)
	{
		writeMsg(connfd, "530 Please login with USER and PASS.\r\n", 0);
		return -1;
	}
	mode_transfer = MODE_transfer_port;
	int parameter[6];
	sscanf(argumemnts, "%d,%d,%d,%d,%d,%d", &parameter[0], &parameter[1], &parameter[2], &parameter[3], &parameter[4], &parameter[5]);
	char ip[16];
	sprintf(ip, "%d.%d.%d.%d", parameter[0], parameter[1], parameter[2], parameter[3]);
	int port = parameter[4] * 256 + parameter[5];

	memset(&port_addr_data, 0, sizeof(port_addr_data));
	port_addr_data.sin_family = AF_INET;
	port_addr_data.sin_port = htons(port);
	if (inet_pton(AF_INET, ip, &port_addr_data.sin_addr) <= 0)
	{
		printf("Error inet_pton(): %s(%d)\r\n", strerror(errno), errno);
		return 1;
	}
	writeMsg(connfd, "200 PORT command successful.\r\n", 0);
}

int proc_PASV(char *arguments)
{
	mode_transfer = MODE_transfer_pasv;
	int sockfd_data;
	if ((sockfd_data = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Error socket(): %s(%d)\r\n", strerror(errno), errno);
		return 1;
	}
	struct sockaddr_in addr_data;
	memset(&addr_data, 0, sizeof(addr_data));
	addr_data.sin_family = AF_INET;
	char msg[256];
	for (int i = 20000; i < 65535; i++)
	{
		addr_data.sin_port = htons(i);
		if (bind(sockfd_data, (struct sockaddr *)&addr_data, sizeof(addr_data)) == 0)
		{
			if (listen(sockfd_data, 10) == -1)
			{
				printf("Error listen(): %s(%d)\r\n", strerror(errno), errno);
				return 1;
			}
			sprintf(msg, "227 Entering Passive Mode (%s,%d,%d)\r\n", local_addr, i / 256, i % 256);
			break;
		}
	}
	data_listen = sockfd_data;
	writeMsg(connfd, msg, 0);
}

int proc_RETR(char *arguments)
{
	if (status_user != STATUS_user_logged_in)
	{
		writeMsg(connfd, "530 Please login with USER and PASS.\r\n", 0);
		return -1;
	}
	else if (mode_transfer == MODE_transfer_undefined)
	{
		writeMsg(connfd, "503 Bad sequence of commands.\r\n", 0);
		return -1;
	}
	int datafd;
	if (mode_transfer == MODE_transfer_pasv)
	{
		// wait
		if ((datafd = accept(data_listen, NULL, NULL)) == -1)
		{
			printf("Error accept(): %s(%d)\r\n", strerror(errno), errno);
			return 1;
		}
	}
	else if (mode_transfer == MODE_transfer_port)
	{
		if ((datafd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
		{
			printf("Error socket(): %s(%d)\r\n", strerror(errno), errno);
			return 1;
		}
		if (connect(datafd, (struct sockaddr *)&port_addr_data, sizeof(port_addr_data)) < 0)
		{
			printf("Error connect(): %s(%d)\r\n", strerror(errno), errno);
			return 1;
		}
	}
	mode_transfer = MODE_transfer_undefined; // clear mode
	if (datafd == -1)
	{
		writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
		return -1;
	}
	char filename[256];
	sprintf(filename, "%s/%s", workdir, arguments);
	FILE *filefp;

	if ((filefp = fopen(filename, "r")) == NULL)
	{
		writeMsg(connfd, "550 File not found.\r\n", 0);
		return -1;
	}
	// accessibility check
	if (access(filename, R_OK) == -1)
	{
		writeMsg(connfd, "550 Permission denied.\r\n", 0);
		return -1;
	}
	// get file size
	fseek(filefp, 0, SEEK_END);
	int file_size = ftell(filefp);
	fseek(filefp, 0, SEEK_SET);
	char msg[256];
	sprintf(msg, "150 Opening BINARY mode data connection for %s (%d bytes).\r\n", arguments, file_size);
	writeMsg(connfd, msg, 0);
	// send file
	char buffer[8192];
	int n;
	while ((n = fread(buffer, 1, 8192, filefp)) > 0)
	{
		if (write(datafd, buffer, n) == -1)
		{
			printf("Error write(): %s(%d)\r\n", strerror(errno), errno);
			return 1;
		}
	}
	writeMsg(connfd, "226 Transfer complete.\r\n", 0);
	fclose(filefp);
	close(datafd);
}

int main(int argc, char **argv)
{
	printf("*****************\n");
	printf("*FTP SERVER VIEW*\n");
	printf("*****************\r\n");
	char sentence[8192];
	int p;
	int len;

	// 创建socket
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Error socket(): %s(%d)\r\n", strerror(errno), errno);
		return 1;
	}

	// 设置本机的ip和port
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = 6789;
	addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听"0.0.0.0"

	// 将本机的ip和port与socket绑定
	if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		printf("Error bind(): %s(%d)\r\n", strerror(errno), errno);
		return 1;
	}

	// 开始监听socket
	if (listen(listenfd, 10) == -1)
	{
		printf("Error listen(): %s(%d)\r\n", strerror(errno), errno);
		return 1;
	}

	// 持续监听连接请求
	while (1)
	{
		// 等待client的连接 -- 阻塞函数
		if ((connfd = accept(listenfd, NULL, NULL)) == -1)
		{
			printf("Error accept(): %s(%d)\r\n", strerror(errno), errno);
			continue;
		}

		writeMsg(connfd, "220 ftp.ssast.org FTP server ready.\r\n", 0);

		while (1) // 每次处理一条语句
		{
			readMsg(connfd, sentence, &len);
			// TODO: 处理语句
			printf("Received: %s\r\n", sentence);
			char command[5];
			char argument[256];
			strncpy(command, sentence, 4);
			if (command[3] == ' ')
			{
				command[3] = '\0';
				if (len > 4)
					strncpy(argument, sentence + 4, len - 3);
			}
			else
			{
				command[4] = '\0';
				if (len > 5)
					strncpy(argument, sentence + 5, len - 4);
			}
			strip(argument, 0);
			if (!strcmp(command, "USER"))
			{
				proc_USER(argument);
			}
			else if (!strcmp(command, "PASS"))
			{
				proc_PASS(argument);
			}
			else if (!strcmp(command, "PORT"))
			{
				proc_PORT(argument);
			}
			else if (!strcmp(command, "PASV"))
			{
				proc_PASV(argument);
			}
			else if (!strcmp(command, "RETR"))
			{
				proc_RETR(argument);
			}
			else
			{
				printf("Unknown command: %s\r\n", command);
				writeMsg(connfd, "500 Unknown command.\r\n", 0);
			}
		}

		// // 字符串处理
		// for (p = 0; p < len; p++)
		// {
		// 	sentence[p] = toupper(sentence[p]);
		// }

		// 发送字符串到socket
		// p = 0;
		// while (p < len)
		// {
		// 	int n = write(connfd, sentence + p, len + 1 - p);
		// 	if (n < 0)
		// 	{
		// 		printf("Error write(): %s(%d)\n", strerror(errno), errno);
		// 		return 1;
		// 	}
		// 	else
		// 	{
		// 		p += n;
		// 	}
		// }

		close(connfd);
	}

	close(listenfd);
}
