#include "serverBase.h"

int status_user = STATUS_user_not_logged_in;
int mode_transfer = MODE_transfer_undefined;
char *user_name;
char *user_email;

int listenfd, connfd; // 监听socket和连接socket不一样，后者用于数据传输
struct sockaddr_in addr;
int data_listen = -1;

char rootdir[256] = "/home/ftp";
char workdir[256] = "/home/ftp";

long rest_index = 0;

struct sockaddr_in port_addr_data;

int proc_USER(char *arguments)
{
    if (status_user == STATUS_user_logged_in)
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
            user_name = (char *)malloc(10);
            strcpy(user_name, "anonymous");
            writeMsg(connfd, "331 Guest login ok, send your complete e-mail address as password.\r\n", 0);
        }
        else
        {
            // user not exist
            writeMsg(connfd, "530 It is not a valid user.\r\n", 0);
        }
    }
    return 0;
}

int proc_PASS(char *arguments)
{
    if (status_user != STATUS_user_need_password)
    {
        writeMsg(connfd, "503 Bad sequence of commands.\r\n", 0);
    }
    else
    {
        if (strlen(arguments) == 0)
        {
            writeMsg(connfd, "501 Syntax error in parameters or arguments.\r\n", 0);
        }
        if (strcmp(user_name, "anonymous") == 0)
        {
            user_email = (char *)malloc(strlen(arguments) * sizeof(char));
            strcpy(user_email, arguments);
            status_user = STATUS_user_logged_in;
            char welcome[512];
            sprintf(welcome, "230-\r\n230-Welcome to\r\n230- School of Software\r\n230- FTP Archives at ftp.ssast.org\r\n230-\r\n230-This site is provided as a public service by School of\r\n230-Software. Use in violation of any applicable laws is strictly\r\n230-prohibited. We make no guarantees, explicit or implicit, about the\r\n230-contents of this site. Use at your own risk.\r\n230-\r\n230 Guest login ok, access restrictions apply.\r\n");
            writeMsg(connfd, welcome, 0);
        }
    }
    return 0;
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
    for (int i = 0; i < 6; i++)
    {
        if (parameter[i] < 0 || parameter[i] > 255)
        {
            writeMsg(connfd, "501 Syntax error in parameters or arguments.\r\n", 0);
            return 1;
        }
    }
    char ip[16];
    sprintf(ip, "%d.%d.%d.%d", parameter[0], parameter[1], parameter[2], parameter[3]);
    int port = parameter[4] * 256 + parameter[5];

    memset(&port_addr_data, 0, sizeof(port_addr_data));
    port_addr_data.sin_family = AF_INET;
    port_addr_data.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &port_addr_data.sin_addr) <= 0)
    {
        printf("Error inet_pton(): %s(%d)\r\n", strerror(errno), errno);
        writeMsg(connfd, "502 Command not implemented.\r\n", 0);
        return 1;
    }
    writeMsg(connfd, "200 PORT command successful.\r\n", 0);
    return 0;
}

int proc_PASV()
{
    if (status_user != STATUS_user_logged_in)
    {
        writeMsg(connfd, "530 Please login with USER and PASS.\r\n", 0);
        return -1;
    }
    if (data_listen != -1)
    {
        close(data_listen);
    }
    mode_transfer = MODE_transfer_pasv;
    int sockfd_data;
    if ((sockfd_data = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    {
        printf("Error socket(): %s(%d)\r\n", strerror(errno), errno);
        writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
        return 1;
    }
    struct sockaddr_in addr_data;
    memset(&addr_data, 0, sizeof(addr_data));
    addr_data.sin_family = AF_INET;
    char msg[256];
    while (1)
    {
        int i = rand() % 45535 + 20000;
        addr_data.sin_port = htons(i);
        if (bind(sockfd_data, (struct sockaddr *)&addr_data, sizeof(addr_data)) == 0)
        {
            if (listen(sockfd_data, 10) == -1)
            {
                printf("Error listen(): %s(%d)\r\n", strerror(errno), errno);
                writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
                continue;
            }
            char serverip[16];
            getIP(serverip);
            for (int j = 0; j < strlen(serverip); j++)
            {
                if (serverip[j] == '.')
                {
                    serverip[j] = ',';
                }
            }
            sprintf(msg, "227 Entering Passive Mode (%s,%d,%d)\r\n", serverip, i / 256, i % 256);
            break;
        }
    }
    data_listen = sockfd_data;
    writeMsg(connfd, msg, 0);
    return 0;
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
            writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
            return 1;
        }
    }
    else if (mode_transfer == MODE_transfer_port)
    {
        if ((datafd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        {
            printf("Error socket(): %s(%d)\r\n", strerror(errno), errno);
            writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
            return 1;
        }
        if (connect(datafd, (struct sockaddr *)&port_addr_data, sizeof(port_addr_data)) < 0)
        {
            printf("Error connect(): %s(%d)\r\n", strerror(errno), errno);
            writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
            return 1;
        }
    }
    mode_transfer = MODE_transfer_undefined; // clear mode
    if (datafd == -1)
    {
        writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
        return -1;
    }
    char filename[512 + 2];
    snprintf(filename, sizeof(filename), "%s/%s", workdir, arguments);
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
    sprintf(msg, "150 Opening BINARY mode data connection for %s (%ld bytes).\r\n", arguments, file_size);
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
            writeMsg(connfd, "426 Connection closed; transfer aborted.\r\n", 0);
            return 1;
        }
    }
    writeMsg(connfd, "226 Transfer complete.\r\n", 0);
    rest_index = 0;
    fclose(filefp);
    close(datafd);
    return 0;
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
            writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
            return 1;
        }
    }
    else if (mode_transfer == MODE_transfer_port)
    {
        if ((datafd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        {
            printf("Error socket(): %s(%d)\r\n", strerror(errno), errno);
            writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
            return 1;
        }
        if (connect(datafd, (struct sockaddr *)&port_addr_data, sizeof(port_addr_data)) < 0)
        {
            printf("Error connect(): %s(%d)\r\n", strerror(errno), errno);
            writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
            return 1;
        }
    }
    mode_transfer = MODE_transfer_undefined; // clear mode
    if (datafd == -1)
    {
        writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
        return -1;
    }
    char filename[512 + 2];
    snprintf(filename, sizeof(filename), "%s/%s", workdir, arguments);
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
    sprintf(msg, "150 Opening BINARY mode data connection for %s.\r\n", arguments);
    writeMsg(connfd, msg, 0);
    // receive file
    char buffer[8192];
    memset(buffer, 0, 8192);
    int n;
    long size = 0;
    while ((n = read(datafd, buffer, 8192)) > 0)
    {
        if (fwrite(buffer, 1, n, filefp) < 0)
        {
            printf("Error fwrite(): %s(%d)\r\n", strerror(errno), errno);
            writeMsg(connfd, "426 Connection closed; transfer aborted.\r\n", 0);
            return 1;
        }
        size += n;
    }
    sprintf(msg, "226 Transfer complete. %ld bytes transmitted.\r\n", size);
    writeMsg(connfd, msg, 0);
    fclose(filefp);
    close(datafd);
    return 0;
}

int proc_PWD()
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
    return 0;
}

int proc_CWD(char *arguments)
{
    if (status_user != STATUS_user_logged_in)
    {
        writeMsg(connfd, "530 Please login with USER and PASS.\r\n", 0);
        return -1;
    }
    char realdir[256];
    getrealdir(arguments, realdir, rootdir, workdir);
    if (checksubdir(arguments, rootdir, workdir))
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
    return 0;
}

int proc_MKD(char *arguments)
{
    if (status_user != STATUS_user_logged_in)
    {
        writeMsg(connfd, "530 Please login with USER and PASS.\r\n", 0);
        return -1;
    }
    char realdir[256];
    getrealdir(arguments, realdir, rootdir, workdir);
    if (checksubdir(arguments, rootdir, workdir))
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
    return 0;
}

int proc_RMD(char *arguments)
{
    if (status_user != STATUS_user_logged_in)
    {
        writeMsg(connfd, "530 Please login with USER and PASS.\r\n", 0);
        return -1;
    }
    char realdir[256];
    getrealdir(arguments, realdir, rootdir, workdir);
    if (checksubdir(arguments, rootdir, workdir))
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
    return 0;
}

int proc_LIST()
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
    char msg[256];
    sprintf(msg, "150 Here comes the directory listing.\r\n");
    writeMsg(connfd, msg, 0);
    int datafd;
    if (mode_transfer == MODE_transfer_pasv)
    {
        // wait
        if ((datafd = accept(data_listen, NULL, NULL)) == -1)
        {
            printf("Error accept(): %s(%d)\r\n", strerror(errno), errno);
            writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
            return 1;
        }
    }
    else if (mode_transfer == MODE_transfer_port)
    {
        if ((datafd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        {
            printf("Error socket(): %s(%d)\r\n", strerror(errno), errno);
            writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
            return 1;
        }
        if (connect(datafd, (struct sockaddr *)&port_addr_data, sizeof(port_addr_data)) < 0)
        {
            printf("Error connect(): %s(%d)\r\n", strerror(errno), errno);
            writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
            return 1;
        }
    }
    mode_transfer = MODE_transfer_undefined; // clear mode
    if (datafd == -1)
    {
        writeMsg(connfd, "425 Can't open data connection.\r\n", 0);
        return -1;
    }
    // send list
    char buffer[8192];
    memset(buffer, 0, 8192);
    char command[256 + 10];
    snprintf(command, sizeof(command), "ls -l %s", workdir);
    FILE *filefp = popen(command, "r");
    int n;
    while ((n = fread(buffer, 1, 8192, filefp)) > 0)
    {
        if (write(datafd, buffer, n) == -1)
        {
            printf("Error write(): %s(%d)\r\n", strerror(errno), errno);
            writeMsg(connfd, "426 Connection closed; transfer aborted.\r\n", 0);
            return 1;
        }
    }
    writeMsg(connfd, "226 Directory send OK.\r\n", 0);
    pclose(filefp);
    close(datafd);
    return 0;
}
