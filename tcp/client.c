#include <sys/socket.h>
#include <netinet/in.h>

#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <memory.h>
#include <stdio.h>

#define MODE_transfer_undefined 0
#define MODE_transfer_port 1
#define MODE_transfer_pasv 2

int sockfd_data;
int mode_transfer = MODE_transfer_undefined;
char savedir[256] = "/home/kirotta/Downloads";

int port_listen;

int main(int argc, char **argv)
{
	printf("*****************\n");
	printf("*FTP CLIENT VIEW*\n");
	printf("*****************\r\n");
	int sockfd;
	struct sockaddr_in addr;
	char sentence[8192];
	int len;
	int p;

	// 创建socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	// 设置目标主机的ip和port
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = 6789;
	if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0)
	{ // 转换ip地址:点分十进制-->二进制
		printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	// 连接上目标主机（将socket和目标主机连接）-- 阻塞函数
	if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		printf("Error connect(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	while (1)
	{
		// 榨干socket接收到的内容
		p = 0;
		while (1)
		{
			int n = read(sockfd, sentence + p, 8191 - p);
			if (n < 0)
			{
				printf("Error read(): %s(%d)\r\n", strerror(errno), errno); // read不保证一次读完，可能中途退出
				return 1;
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

		// 注意：read并不会将字符串加上'\0'，需要手动添加
		sentence[p - 1] = '\0';

		printf("FROM SERVER: %s \r\n", sentence);

		// if sentence is 227
		if (sentence[0] == '2' && sentence[1] == '2' && sentence[2] == '7')
		{
			// create a new socket
			if ((sockfd_data = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
			{
				printf("Error socket(): %s(%d)\r\n", strerror(errno), errno);
				return 1;
			}
			struct sockaddr_in addr_data;
			memset(&addr_data, 0, sizeof(addr_data));
			addr_data.sin_family = AF_INET;
			char ip[16];
			int port;
			int temp[6];
			sscanf(sentence, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &temp[0], &temp[1], &temp[2], &temp[3], &temp[4], &temp[5]);
			sprintf(ip, "%d.%d.%d.%d", temp[0], temp[1], temp[2], temp[3]);
			port = temp[4] * 256 + temp[5];
			addr_data.sin_port = htons(port);
			if (inet_pton(AF_INET, ip, &addr_data.sin_addr) <= 0)
			{
				printf("Error inet_pton(): %s(%d)\r\n", strerror(errno), errno);
				return 1;
			}
			if (connect(sockfd_data, (struct sockaddr *)&addr_data, sizeof(addr_data)) < 0)
			{
				printf("Error connect(): %s(%d)\r\n", strerror(errno), errno);
				return 1;
			}
			printf("Connected to server data port %d\r\n", port);
		}

		// 获取键盘输入
		fgets(sentence, 4096, stdin);
		len = strlen(sentence);
		sentence[len] = '\n';
		sentence[len + 1] = '\0';

		// if command is PORT
		char command[4];
		sscanf(sentence, "%s", command);
		if (strcmp(command, "PORT") == 0)
		{
			int arg[6];
			sscanf(sentence, "PORT %d,%d,%d,%d,%d,%d", &arg[0], &arg[1], &arg[2], &arg[3], &arg[4], &arg[5]);
			int port = arg[4] * 256 + arg[5];
			char ip[16];
			sprintf(ip, "%d.%d.%d.%d", arg[0], arg[1], arg[2], arg[3]);
			if ((port_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
			{
				printf("Error socket(): %s(%d)\r\n", strerror(errno), errno);
				return 1;
			}
			struct sockaddr_in addr_data;
			memset(&addr_data, 0, sizeof(addr_data));
			addr_data.sin_family = AF_INET;
			addr_data.sin_port = htons(port);
			addr_data.sin_addr.s_addr = inet_addr(ip);
			if (bind(port_listen, (struct sockaddr *)&addr_data, sizeof(addr_data)) == -1)
			{
				printf("Error bind(): %s(%d)\r\n", strerror(errno), errno);
				return 1;
			}
			if (listen(port_listen, 10) == -1)
			{
				printf("Error listen(): %s(%d)\r\n", strerror(errno), errno);
				return 1;
			}
			printf("PORT command successful.\r\n");
			mode_transfer = MODE_transfer_port;
		}
		else if (strcmp(command, "PASV") == 0)
		{
			mode_transfer = MODE_transfer_pasv;
		}

		// 把键盘输入写入socket
		p = 0;
		while (p < len)
		{
			int n = write(sockfd, sentence + p, len + 1 - p); // write函数不保证所有的数据写完，可能中途退出
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
		if (strcmp(command, "RETR") == 0)
		{
			if (mode_transfer == MODE_transfer_port)
			{
				if ((sockfd_data = accept(port_listen, NULL, NULL)) == -1)
				{
					printf("Error accept(): %s(%d)\r\n", strerror(errno), errno);
					return 1;
				}
			}
			// get filename
			char filename[256];
			sscanf(sentence, "RETR %s", filename);
			char path[256];
			sprintf(path, "%s/%s", savedir, filename);
			FILE *file = fopen(path, "wb+");
			if (file == NULL)
			{
				printf("Error fopen(): %s(%d)\r\n", strerror(errno), errno);
				return 1;
			}
			// read from data socket
			while (1)
			{
				char buffer[8192];
				int n = read(sockfd_data, buffer, 8192);
				if (n < 0)
				{
					printf("Error read(): %s(%d)\r\n", strerror(errno), errno);
					return 1;
				}
				else if (n == 0)
				{
					break;
				}
				else
				{
					fwrite(buffer, 1, n, file);
				}
			}
			fclose(file);
			close(sockfd_data);
			printf("Transfer complete.\r\n");
		}
		else if (strcmp(command, "STOR") == 0)
		{
			if (mode_transfer == MODE_transfer_port)
			{
				if ((sockfd_data = accept(port_listen, NULL, NULL)) == -1)
				{
					printf("Error accept(): %s(%d)\r\n", strerror(errno), errno);
					return 1;
				}
			}
			// get filename
			char filename[256];
			sscanf(sentence, "STOR %s", filename);
			char path[256];
			sprintf(path, "%s/%s", savedir, filename);
			FILE *file = fopen(path, "rb");
			if (file == NULL)
			{
				printf("Error fopen(): %s(%d)\r\n", strerror(errno), errno);
				return 1;
			}
			// write to data socket
			while (1)
			{
				char buffer[8192];
				int n = fread(buffer, 1, 8192, file);
				if (n < 0)
				{
					printf("Error fread(): %s(%d)\r\n", strerror(errno), errno);
					return 1;
				}
				else if (n == 0)
				{
					break;
				}
				else
				{
					write(sockfd_data, buffer, n);
				}
			}
			fclose(file);
			close(sockfd_data);
			printf("Transfer complete.\r\n");
		}
	}
	close(sockfd);

	return 0;
}
