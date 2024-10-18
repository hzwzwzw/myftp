#include <iostream>
#include <string>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <memory.h>
#include <stdio.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <QRegularExpression>
#include <QNetworkInterface>
#include <QMessageBox>

#define MODE_transfer_undefined 0
#define MODE_transfer_port 1
#define MODE_transfer_pasv 2

class FtpState
{
public:
    int sockfd = -1;
    struct sockaddr_in addr;
    int mode_transfer = MODE_transfer_pasv;
    int sockfd_data = -1;
    struct sockaddr_in addr_data;
    int port_listen = -1;
    char dir[256];
};

bool gui_mode = false;

char local_dir[256];

FtpState state;

int read_reply(char *str, int max_len);
void gui_list_server();
void gui_list_local();

void set_data_socket();
void connect_to_data_socket();

void error(const char *msg, const char *err, int errnum);
int read_reply(char *str, int max_len);

int connect_to_server(char *ip_addr, int port)
{
    // 创建socket
    if ((state.sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    {
        error("Error socket.", strerror(errno), errno);
        return 1;
    }

    // 设置目标主机的ip和port
    memset(&state.addr, 0, sizeof(state.addr));
    state.addr.sin_family = AF_INET;
    state.addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip_addr, &state.addr.sin_addr) <= 0)
    { // 转换ip地址:点分十进制-->二进制
        error("Error inet_pton.", strerror(errno), errno);
        return 1;
    }

    // 连接上目标主机（将socket和目标主机连接）-- 阻塞函数
    if (connect(state.sockfd, (struct sockaddr *)&state.addr, sizeof(state.addr)) < 0)
    {
        error("Error connect.", strerror(errno), errno);
        return 1;
    }
    char buf[8192];
    read_reply(buf, 8192);
    return 0;
}

void download(QString filename, bool rest)
{
    char path[512 + 2];
    snprintf(path, sizeof(path), "%s/%s", local_dir, filename.toUtf8().data());
    char buf[256];
    sprintf(buf, "RETR %s\r\n", filename.toUtf8().data());
    write(state.sockfd, buf, strlen(buf));
    connect_to_data_socket();
    read_reply(buf, 256);
    FILE *file;
    if (!rest)
    {
        file = fopen(path, "wb+");
    }
    else
    {
        file = fopen(path, "ab+");
    }
    if (file == NULL)
    {
        error("Error fopen.", strerror(errno), errno);
        return;
    }
    while (1)
    {
        char buffer[8192];
        memset(buffer, 0, 8192);
        int n = read(state.sockfd_data, buffer, 8192);
        if (n < 0)
        {
            error("Error read.", strerror(errno), errno);
            return;
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
    close(state.sockfd_data);
    if (QString(buf).indexOf("\n22") == -1)
        read_reply(buf, 256);

    // printf("Transfer complete.\r\n");
}

void upload(QString filename, bool appe, long appe_begin)
{
    char buf[256];
    if (!appe)
    {
        sprintf(buf, "STOR %s\r\n", filename.toUtf8().data());
    }
    else
    {
        sprintf(buf, "APPE %s\r\n", filename.toUtf8().data());
    }
    write(state.sockfd, buf, strlen(buf));
    connect_to_data_socket();
    read_reply(buf, 256);
    char path[512 + 2];
    snprintf(path, sizeof(path), "%s/%s", local_dir, filename.toUtf8().data());
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        error("Error fopen.", strerror(errno), errno);
        return;
    }
    if (appe)
    {
        fseek(file, appe_begin, SEEK_SET);
    }
    while (1)
    {
        char buffer[8192];
        memset(buffer, 0, 8192);
        int n = fread(buffer, 1, 8192, file);
        if (n < 0)
        {
            error("Error fread.", strerror(errno), errno);
            return;
        }
        else if (n == 0)
        {
            break;
        }
        else
        {
            write(state.sockfd_data, buffer, n);
        }
    }
    fclose(file);
    close(state.sockfd_data);
    if (QString(buf).indexOf("\n22") == -1)
        read_reply(buf, 256);
    // printf("Transfer complete.\r\n");
}

void getLocalIP(char *address)
{
    // called after state.addr is inited.
    // inet_ntop(AF_INET, &state.addr.sin_addr, address, INET_ADDRSTRLEN);

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

void set_port(char *addr = NULL, int specified_port = 0)
{
    char ip[16];
    if (addr == NULL)
    {
        getLocalIP(ip);
    }
    else
    {
        strcpy(ip, addr);
    }
    if ((state.port_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    {
        error("Error socket(): %s(%d)\r\n", strerror(errno), errno);
        return;
    }
    int port = 20000;
    if (specified_port != 0)
    {
        port = specified_port;
        struct sockaddr_in addr_data;
        memset(&addr_data, 0, sizeof(addr_data));
        addr_data.sin_family = AF_INET;
        addr_data.sin_port = htons(port);
        addr_data.sin_addr.s_addr = inet_addr(ip);
        if (bind(state.port_listen, (struct sockaddr *)&addr_data, sizeof(addr_data)) == -1)
        {
            error("Error bind(): %s(%d)\r\n", strerror(errno), errno);
            return;
        }
        if (listen(state.port_listen, 10) == -1)
        {
            error("Error listen(): %s(%d)\r\n", strerror(errno), errno);
            return;
        }
    }
    else
    {
        for (; port < 65536; port++)
        {
            struct sockaddr_in addr_data;
            memset(&addr_data, 0, sizeof(addr_data));
            addr_data.sin_family = AF_INET;
            addr_data.sin_port = htons(port);
            addr_data.sin_addr.s_addr = inet_addr(ip);
            if (bind(state.port_listen, (struct sockaddr *)&addr_data, sizeof(addr_data)) == -1)
            {
                error("Error bind(): %s(%d)\r\n", strerror(errno), errno);
                continue;
            }
            if (listen(state.port_listen, 10) == -1)
            {
                error("Error listen(): %s(%d)\r\n", strerror(errno), errno);
                continue;
            }
            break;
        }
    }
    char buf[256];
    char ip_comma[16];
    strcpy(ip_comma, ip);
    for (int i = 0; i < strlen(ip_comma); i++)
    {
        if (ip_comma[i] == '.')
        {
            ip_comma[i] = ',';
        }
    }
    sprintf(buf, "PORT %s,%d,%d\r\n", ip_comma, port / 256, port % 256);
    write(state.sockfd, buf, strlen(buf));
    read_reply(buf, 256);
    if (strncmp(buf, "200", 3) == 0)
    {
        // printf("PORT command successful.\r\n");
        state.mode_transfer = MODE_transfer_port;
    }
    else
    {
        error("Error PORT command.", buf, 0);
        // TODO: EXCEPTION
    }
}

void set_pasv()
{
    write(state.sockfd, "PASV\r\n", strlen("PASV\r\n"));
    char buf[8192];
    read_reply(buf, 8192);
    if (buf[0] == '5')
    {
        // TODO: EXCEPTION
        return;
    }
    if (strncmp(buf, "227", 3) == 0)
    {
        int temp[6];
        QString qs = buf;
        int p1 = qs.indexOf('(');
        int p2 = qs.indexOf(')');
        qs = qs.mid(p1 + 1, p2 - p1 - 1);
        QStringList parts = qs.split(",");
        for (int i = 0; i < 6; i++)
        {
            temp[i] = parts[i].toInt();
        }
        char ip[16];
        sprintf(ip, "%d.%d.%d.%d", temp[0], temp[1], temp[2], temp[3]);
        // printf("PASV command successful at %s:%d.\r\n", ip, temp[4] * 256 + temp[5]);
        int port = temp[4] * 256 + temp[5];
        if ((state.sockfd_data = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        {
            error("Error socket.", strerror(errno), errno);
            return;
        }
        memset(&state.addr_data, 0, sizeof(state.addr_data));
        state.addr_data.sin_family = AF_INET;
        state.addr_data.sin_port = htons(port);
        if (inet_pton(AF_INET, ip, &state.addr_data.sin_addr) <= 0)
        {
            error("Error inet_pton.", strerror(errno), errno);
            return;
        }
    }
    state.mode_transfer = MODE_transfer_pasv;
}

void connect_to_data_socket()
{
    if (state.mode_transfer == MODE_transfer_port)
    {
        if ((state.sockfd_data = accept(state.port_listen, NULL, NULL)) == -1)
        {
            error("Error accept.", strerror(errno), errno);
            return;
        }
    }
    else
    {
        if (connect(state.sockfd_data, (struct sockaddr *)&state.addr_data, sizeof(state.addr_data)) < 0)
        {
            error("Error connect.", strerror(errno), errno);
            return;
        }
    }
}

void set_data_socket()
{
    if (state.mode_transfer == MODE_transfer_port)
    {
        set_port();
    }
    else
    {
        set_pasv();
    }
}

QString list_server()
{
    write(state.sockfd, "LIST\r\n", strlen("LIST\r\n"));
    char buf[8192];
    read_reply(buf, 8192);
    connect_to_data_socket();
    QString qs = "";
    if (strncmp(buf, "150", 3) == 0 || strncmp(buf, "125", 3) == 0)
    {
        char buffer[8192];
        while (1)
        {
            memset(buffer, 0, 8192);
            int n = read(state.sockfd_data, buffer, 8192);
            if (n < 0)
            {
                error("Error read.", strerror(errno), errno);
                qs = "";
                return qs;
            }
            else if (n == 0)
            {
                break;
            }
            else
            {
                printf("%s", buffer);
                qs.append(buffer);
            }
        }
        close(state.sockfd_data);
        // if 226 not received
        if (QString(buf).indexOf("\n226") == -1)
        {
            read_reply(buf, 8192);
        }
    }
    return qs;
}
