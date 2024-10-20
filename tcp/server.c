#include "serverInstance.h"
#include <time.h>

void quit(int sig)
{
	printf("Server is shutting down...(%d)\r\n", sig);
	close(connfd);
	close(listenfd);
	exit(0);
}

int main(int argc, char **argv)
{
	printf("*****************\n");
	printf("*FTP SERVER VIEW*\n");
	printf("*****************\r\n");
	char serverip[16];
	getIP(serverip);
	printf("Server IP: %s\r\n", serverip);
	strcpy(rootdir, "/tmp");
	strcpy(workdir, "/tmp");
	int port = 21;
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-root") == 0)
		{
			if (i + 1 < argc && argv[i + 1][0] != '-')
			{
				char argdir[256];
				strncpy(argdir, argv[++i], 256);
				if (argdir[0] != '/')
				{
					char tempdir[256];
					getcwd(tempdir, sizeof(tempdir));
					strcat(tempdir, "/");
					strcat(tempdir, argdir);
					realpath(tempdir, argdir);
				}
				if (access(argdir, F_OK) == -1)
				{
					printf("Directory %s does not exist.\n", argdir);
					return 1;
				}
				strcpy(rootdir, argdir);
				strcpy(workdir, rootdir);
			}
			else
			{
				printf("Missing argument for -root\n");
				return 1;
			}
		}
		else if (strcmp(argv[i], "-port") == 0)
		{
			if (i + 1 < argc && argv[i + 1][0] != '-')
			{
				port = atoi(argv[++i]);
			}
			else
			{
				printf("Missing argument for -port\n");
				return 1;
			}
		}
		else
		{
			printf("Unknown option: %s\n", argv[i]);
			return 1;
		}
	}

	char sentence[8192];
	// int p;
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
	addr.sin_port = htons(port);
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

	signal(SIGINT, quit);

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
			srand(time(NULL));
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
					proc_PASV();
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
					proc_PWD();
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
					proc_LIST();
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
