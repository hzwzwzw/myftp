#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <signal.h>

#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#define STATUS_user_not_logged_in 0
#define STATUS_user_need_password 1
#define STATUS_user_logged_in 2
#define MODE_transfer_undefined 0
#define MODE_transfer_port 1
#define MODE_transfer_pasv 2

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
    return 0;
}

int writeMsg(int fd, char *sentence, int len)
{
    if (len == 0)
        len = strlen(sentence);
    // 发送字符串到socket
    int p = 0;
    while (p < len)
    {
        int n = write(fd, sentence + p, len - p);
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

    return 0;
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

int getrealdir(const char *dir, char *realdir, const char *rootdir, const char *workdir)
{
    char tmp[512];
    if (dir[0] == '/')
    {
        snprintf(tmp, sizeof(tmp), "%s%s", rootdir, dir);
    }
    else
    {
        snprintf(tmp, sizeof(tmp), "%s/%s", workdir, dir);
    }
    realpath(tmp, realdir);
    // printf("dir: %s\ncombdir: %s\nrealdir: %s\n", dir, tmp, realdir);
    return 0;
}

int checksubdir(const char *dir, const char *rootdir, const char *workdir)
{
    char realdir[256];
    getrealdir(dir, realdir, rootdir, workdir);
    if (strncmp(realdir, rootdir, strlen(rootdir)) != 0)
    {
        return 1;
    }
    return 0;
}

void getIP(char *address)
{
    // 获取服务器的IP地址
    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // 只获取IPv4地址
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo(hostname, NULL, &hints, &res);

    struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
    strcpy(address, inet_ntoa(addr->sin_addr));
}