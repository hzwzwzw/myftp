#include <sys/socket.h>
#include <netinet/in.h>

#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

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

char rootdir[256] = "/home/ftp";
char workdir[256] = "/home/ftp";
char local_addr[15] = "172,24,153,139";

long rest_index = 0;

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
			return -1;
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

int getrealdir(char *dir, char *realdir)
{
	char tmp[256];
	if (dir[0] == '/')
	{
		sprintf(tmp, "%s%s", rootdir, dir);
	}
	else
	{
		sprintf(tmp, "%s/%s", workdir, dir);
	}
	realpath(tmp, realdir);
	printf("dir: %s\ncombdir: %s\nrealdir: %s\n", dir, tmp, realdir);
}

int checksubdir(char *dir)
{
	char realdir[256];
	getrealdir(dir, realdir);
	if (strncmp(realdir, rootdir, strlen(rootdir)) != 0)
	{
		return 1;
	}
	return 0;
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
	long file_size = ftell(filefp);
	fseek(filefp, rest_index, SEEK_SET);
	char msg[256];
	sprintf(msg, "150-Opening BINARY mode data connection for %s (%ld bytes).\r\n", arguments, file_size);
	writeMsg(connfd, msg, 0);
	// send file
	char buffer[8192];
	memset(buffer, 0, 8192);
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
	rest_index = 0;
	fclose(filefp);
	close(datafd);
}

int proc_STOR(char *arguments, int appe)
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
	if (appe)
	{
		if ((filefp = fopen(filename, "a")) == NULL)
		{
			writeMsg(connfd, "550 Permission denied.\r\n", 0);
			return -1;
		}
	}
	else
	{
		if ((filefp = fopen(filename, "w")) == NULL)
		{
			writeMsg(connfd, "550 Permission denied.\r\n", 0);
			return -1;
		}
	}
	char msg[256];
	sprintf(msg, "150-Opening BINARY mode data connection for %s.\r\n", arguments);
	writeMsg(connfd, msg, 0);
	// receive file
	char buffer[8192];
	memset(buffer, 0, 8192);
	int n;
	long size = 0;
	while ((n = read(datafd, buffer, 8192)) > 0)
	{
		if (fwrite(buffer, 1, n, filefp) == -1)
		{
			printf("Error fwrite(): %s(%d)\r\n", strerror(errno), errno);
			return 1;
		}
		size += n;
	}
	sprintf(msg, "226 Transfer complete. %ld bytes transmitted.\r\n", size);
	writeMsg(connfd, msg, 0);
	fclose(filefp);
	close(datafd);
}

int proc_PWD(char *arguments)
{
	if (status_user != STATUS_user_logged_in)
	{
		writeMsg(connfd, "530 Please login with USER and PASS.\r\n", 0);
		return -1;
	}
	char msg[256];
	if (strcmp(workdir, rootdir) == 0)
		sprintf(msg, "257 current directory is \"%s\".\r\n", "/");
	else
		sprintf(msg, "257 current directory is \"%s\".\r\n", workdir + strlen(rootdir));
	writeMsg(connfd, msg, 0);
}
int proc_CWD(char *arguments)
{
	if (status_user != STATUS_user_logged_in)
	{
		writeMsg(connfd, "530 Please login with USER and PASS.\r\n", 0);
		return -1;
	}
	char realdir[256];
	getrealdir(arguments, realdir);
	if (checksubdir(arguments))
	{
		writeMsg(connfd, "550 Permission denied.\r\n", 0);
	}
	else if (access(realdir, F_OK) != 0)
	{
		writeMsg(connfd, "550 Directory not found.\r\n", 0);
	}
	else
	{
		struct stat statbuf;
		if (stat(realdir, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode))
		{
			writeMsg(connfd, "550 Not a directory.\r\n", 0);
		}
		else
		{
			strcpy(workdir, realdir);
			writeMsg(connfd, "250 Directory successfully changed.\r\n", 0);
		}
	}
}
int proc_MKD(char *arguments)
{
	if (status_user != STATUS_user_logged_in)
	{
		writeMsg(connfd, "530 Please login with USER and PASS.\r\n", 0);
		return -1;
	}
	char realdir[256];
	getrealdir(arguments, realdir);
	if (checksubdir(arguments))
	{
		writeMsg(connfd, "550 Permission denied. An illeagl path\r\n", 0);
	}
	else
	{
		if (mkdir(realdir, 0777) == -1)
		{
			writeMsg(connfd, "550 Permission denied.\r\n", 0);
		}
		else
		{
			writeMsg(connfd, "257 Directory created.\r\n", 0);
		}
	}
}
int proc_RMD(char *arguments)
{
	if (status_user != STATUS_user_logged_in)
	{
		writeMsg(connfd, "530 Please login with USER and PASS.\r\n", 0);
		return -1;
	}
	char realdir[256];
	getrealdir(arguments, realdir);
	if (checksubdir(arguments))
	{
		writeMsg(connfd, "550 Permission denied.\r\n", 0);
	}
	else
	{
		if (rmdir(realdir) == -1)
		{
			writeMsg(connfd, "550 Permission denied.\r\n", 0);
		}
		else
		{
			writeMsg(connfd, "250 Directory deleted.\r\n", 0);
		}
	}
}
int proc_LIST(char *arguments)
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
	char msg[256];
	sprintf(msg, "150 Here comes the directory listing.\r\n");
	writeMsg(connfd, msg, 0);
	// send list
	char buffer[8192];
	memset(buffer, 0, 8192);
	char command[256];
	sprintf(command, "ls -l %s", workdir);
	FILE *filefp = popen(command, "r");
	int n;
	while ((n = fread(buffer, 1, 8192, filefp)) > 0)
	{
		if (write(datafd, buffer, n) == -1)
		{
			printf("Error write(): %s(%d)\r\n", strerror(errno), errno);
			return 1;
		}
	}
	writeMsg(connfd, "226 Directory send OK.\r\n", 0);
	pclose(filefp);
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
	addr.sin_port = 101;
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

		int proc = fork();

		if (proc != 0)
		{
			close(connfd);
			continue;
		}
		else
		{

			writeMsg(connfd, "220 ftp.ssast.org FTP server ready.\r\n", 0);

			while (1) // 每次处理一条语句
			{
				memset(sentence, 0, 8192);
				if (-1 == readMsg(connfd, sentence, &len))
				{
					break;
				}
				// TODO: 处理语句
				printf("Received: %s\r\n", sentence);
				char command[5];
				char argument[256];
				memset(command, 0, 5);
				memset(argument, 0, 256);
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
				strip(command, 0);
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
				else if (!strcmp(command, "STOR"))
				{
					proc_STOR(argument, 0);
				}
				else if (!strcmp(command, "REST"))
				{
					sscanf(argument, "%ld", &rest_index);
					writeMsg(connfd, "350 Restart position accepted.\r\n", 0);
				}
				else if (!strcmp(command, "PWD"))
				{
					proc_PWD(argument);
				}
				else if (!strcmp(command, "CWD"))
				{
					proc_CWD(argument);
				}
				else if (!strcmp(command, "MKD"))
				{
					proc_MKD(argument);
				}
				else if (!strcmp(command, "RMD"))
				{
					proc_RMD(argument);
				}
				else if (!strcmp(command, "LIST"))
				{
					proc_LIST(argument);
				}
				else if (!strcmp(command, "SYST"))
				{
					writeMsg(connfd, "215 UNIX Type: L8\r\n", 0);
				}
				else if (!strcmp(command, "TYPE"))
				{
					if (!strcmp(argument, "I"))
						writeMsg(connfd, "200 Type set to I.\r\n", 0);
					else
						writeMsg(connfd, "504 Unknown parameter.\r\n", 0);
				}
				else if (!strcmp(command, "QUIT"))
				{
					writeMsg(connfd, "221 Goodbye.\r\n", 0);
					if (data_listen != -1)
						close(data_listen);
					break;
				}
				else if (!strcmp(command, "ABOR"))
				{
					writeMsg(connfd, "226 Abort successful.\r\n", 0);
					if (data_listen != -1)
						close(data_listen);
					break;
				}
				else if (!strcmp(command, "APPE"))
				{
					proc_STOR(argument, 1);
				}
				else
				{
					printf("Unknown command: %s\r\n", command);
					writeMsg(connfd, "500 Unknown command.\r\n", 0);
				}
			}

			close(connfd);
			close(listenfd);
			exit(0);
		}
	}

	close(listenfd);
}
