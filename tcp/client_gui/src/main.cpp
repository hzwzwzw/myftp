#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <memory.h>
#include <stdio.h>

#include <sys/stat.h>

#include "ftpclientui.h"

#include <QRegularExpression>
#include <QNetworkInterface>

#define MODE_transfer_undefined 0
#define MODE_transfer_port 1
#define MODE_transfer_pasv 2

int read_reply(char *str, int max_len);

FtpClientUI *ui;

class FtpState
{
public:
    int sockfd;
    struct sockaddr_in addr;
    int mode_transfer = MODE_transfer_pasv;
    int sockfd_data;
    struct sockaddr_in addr_data;
    int port_listen;
    char dir[256];
};

char local_dir[256];

FtpState state;

void list_server();
void list_local();

void cwd_server()
{
    char path[256];
    strcpy(path, ui->serverPathInput->text().toUtf8().data());
    char buf[256];
    sprintf(buf, "CWD %s\r\n", path);
    write(state.sockfd, buf, strlen(buf));
    read_reply(buf, 256);
    list_server();
    // use PWD to get current directory
    write(state.sockfd, "PWD\r\n", 5);
    read_reply(buf, 256);
    if (strncmp(buf, "257", 3) == 0)
    {
        char *dir;
        dir = strstr(buf, "\"") + 1;
        dir[strstr(dir, "\"") - dir] = 0;
        // ascii to utf-8
        printf("Current directory: %s\r\n", dir);
        ui->serverPathInput->setText(dir);
        strcpy(state.dir, dir);
    }
}

void cwd_local()
{
    char path[256];
    strcpy(path, ui->clientPathInput->text().toUtf8().data());
    char tmp[256];
    if (path[0] == '/')
        strcpy(tmp, path);
    else
        sprintf(tmp, "%s/%s", local_dir, path);

    char real[256];
    if (realpath(tmp, real) == NULL)
    {
        printf("Error realpath(): %s(%d)\r\n", strerror(errno), errno);
        return;
    }
    // check if it exits and is a directory
    struct stat st;
    if (stat(real, &st) == -1)
    {
        printf("Error stat(): %s(%d)\r\n", strerror(errno), errno);
        return;
    }
    if (!S_ISDIR(st.st_mode))
    {
        printf("Error: not a directory\r\n");
        return;
    }
    strcpy(local_dir, real);
    ui->clientPathInput->setText(local_dir);
    list_local();
}

int connect_to_server(char *ip_addr, int port)
{
    // 创建socket
    if ((state.sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    {
        printf("Error socket(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }

    // 设置目标主机的ip和port
    memset(&state.addr, 0, sizeof(state.addr));
    state.addr.sin_family = AF_INET;
    state.addr.sin_port = port;
    if (inet_pton(AF_INET, ip_addr, &state.addr.sin_addr) <= 0)
    { // 转换ip地址:点分十进制-->二进制
        printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }

    // 连接上目标主机（将socket和目标主机连接）-- 阻塞函数
    if (connect(state.sockfd, (struct sockaddr *)&state.addr, sizeof(state.addr)) < 0)
    {
        printf("Error connect(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    char buf[8192];
    read_reply(buf, 8192);
    return 0;
}

void button_connect_clicked()
{
    char *ip_addr = ui->serverAddrInput->text().toUtf8().data();
    int port = ui->serverPortInput->text().toInt();
    if (connect_to_server(ip_addr, port) != 0)
    {
        ui->shellInput->setText("Error connecting to server\n");
    }
}

void button_login_clicked()
{
    const char *username = ui->usernameInput->text().toUtf8().data();
    const char *password = ui->passwordInput->text().toUtf8().data();
    char buf[8192];
    sprintf(buf, "USER %s\r\n", username);
    write(state.sockfd, buf, strlen(buf));
    read_reply(buf, 8192);
    if (buf[0] == '5')
    {
        return;
    }
    if (strlen(password) != 0)
    {
        sprintf(buf, "PASS %s\r\n", password);
        write(state.sockfd, buf, strlen(buf));
        read_reply(buf, 8192);
    }
    list_server();
}

void button_server_enter_clicked()
{
    cwd_server();
}

void button_server_parent_clicked()
{
    char path[256];
    sprintf(path, "%s/..", state.dir);
    ui->serverPathInput->setText(path);
    cwd_server();
}

void button_client_enter_clicked()
{
    cwd_local();
}

void button_client_parent_clicked()
{
    char path[256];
    sprintf(path, "%s/..", local_dir);
    ui->clientPathInput->setText(path);
    cwd_local();
}

void port_checked()
{
    state.mode_transfer = MODE_transfer_port;
}

void pasv_checked()
{
    state.mode_transfer = MODE_transfer_pasv;
}

void getLocalIP(char *address)
{
    // called after state.addr is inited.
    inet_ntop(AF_INET, &state.addr.sin_addr, address, INET_ADDRSTRLEN);
    printf("Local IP: %s\n", address);
    // QString localIP = "";
    // QList<QHostAddress> list = QNetworkInterface::allAddresses();
    // for (int nIter = 0; nIter < list.count(); nIter++)
    // {
    //     if (!list[nIter].isLoopback() &&
    //         list[nIter].protocol() == QAbstractSocket::IPv4Protocol &&
    //         !list[nIter].isLinkLocal())
    //     {
    //         localIP = list[nIter].toString();
    //         printf("Local IP: %s\n", localIP.toUtf8().data());
    //     }
    // }
    // strcpy(addr, localIP.toUtf8().data());
}

void set_port()
{
    char ip[16];
    getLocalIP(ip);
    int port = 20000;
    for (; port < 65536; port++)
    {
        if ((state.port_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        {
            printf("Error socket(): %s(%d)\r\n", strerror(errno), errno);
            continue;
        }
        struct sockaddr_in addr_data;
        memset(&addr_data, 0, sizeof(addr_data));
        addr_data.sin_family = AF_INET;
        addr_data.sin_port = htons(port);
        addr_data.sin_addr.s_addr = inet_addr(ip);
        if (bind(state.port_listen, (struct sockaddr *)&addr_data, sizeof(addr_data)) == -1)
        {
            printf("Error bind(): %s(%d)\r\n", strerror(errno), errno);
            continue;
        }
        if (listen(state.port_listen, 10) == -1)
        {
            printf("Error listen(): %s(%d)\r\n", strerror(errno), errno);
            continue;
        }
        break;
    }
    printf("PORT command successful at %s:%d.\r\n", ip, port);
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
    printf("PORT command successful.\r\n");
}

void set_pasv()
{
    write(state.sockfd, "PASV\r\n", 6);
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
        sscanf(buf, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &temp[0], &temp[1], &temp[2], &temp[3], &temp[4], &temp[5]);
        char ip[16];
        sprintf(ip, "%d.%d.%d.%d", temp[0], temp[1], temp[2], temp[3]);
        int port = temp[4] * 256 + temp[5];
        if ((state.sockfd_data = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        {
            printf("Error socket(): %s(%d)\r\n", strerror(errno), errno);
            return;
        }
        memset(&state.addr_data, 0, sizeof(state.addr_data));
        state.addr_data.sin_family = AF_INET;
        state.addr_data.sin_port = htons(port);
        if (inet_pton(AF_INET, ip, &state.addr_data.sin_addr) <= 0)
        {
            printf("Error inet_pton(): %s(%d)\r\n", strerror(errno), errno);
            return;
        }
    }
}

void connect_to_data_socket()
{
    if (state.mode_transfer == MODE_transfer_port)
    {
        if ((state.sockfd_data = accept(state.port_listen, NULL, NULL)) == -1)
        {
            printf("Error accept(): %s(%d)\r\n", strerror(errno), errno);
            return;
        }
    }
    else
    {
        if (connect(state.sockfd_data, (struct sockaddr *)&state.addr_data, sizeof(state.addr_data)) < 0)
        {
            printf("Error connect(): %s(%d)\r\n", strerror(errno), errno);
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

void list_server()
{
    set_data_socket();
    write(state.sockfd, "LIST\r\n", 6);
    connect_to_data_socket();
    char buf[8192];
    read_reply(buf, 8192);
    if (strncmp(buf, "150", 3) == 0)
    {
        ui->serverFileTable->clear();
        ui->serverFileTable->setRowCount(0);
        char buffer[8192];
        QString qs = "";
        while (1)
        {
            memset(buffer, 0, 8192);
            int n = read(state.sockfd_data, buffer, 8192);
            if (n < 0)
            {
                printf("Error read(): %s(%d)\r\n", strerror(errno), errno);
                return;
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
        read_reply(buf, 8192);
        QStringList lines = qs.split("\n");
        for (const QString &line : lines)
        {
            if (!line.isEmpty())
            {
                if (line.indexOf("total") == 0)
                    continue;

                QStringList parts = line.split(QRegularExpression("\\s+"));

                ui->serverFileTable->insertRow(ui->serverFileTable->rowCount());
                ui->serverFileTable->setItem(ui->serverFileTable->rowCount() - 1, 0, new QTableWidgetItem(parts[8]));
                ui->serverFileTable->setItem(ui->serverFileTable->rowCount() - 1, 1, new QTableWidgetItem(parts[4]));
                ui->serverFileTable->setItem(ui->serverFileTable->rowCount() - 1, 2, new QTableWidgetItem(parts[5] + " " + parts[6] + " " + parts[7]));
                ui->serverFileTable->setItem(ui->serverFileTable->rowCount() - 1, 3, new QTableWidgetItem(parts[0]));
                ui->serverFileTable->setItem(ui->serverFileTable->rowCount() - 1, 4, new QTableWidgetItem(parts[1]));
                ui->serverFileTable->setItem(ui->serverFileTable->rowCount() - 1, 5, new QTableWidgetItem(parts[2]));
                ui->serverFileTable->setItem(ui->serverFileTable->rowCount() - 1, 6, new QTableWidgetItem(parts[3]));

                QStringList title = {"Name", "Size", "Time", "Permission", "Type", "Owner", "Group"};
                ui->serverFileTable->setHorizontalHeaderLabels(title);
            }
        }
    }
}

void list_local()
{
    char command[256];
    sprintf(command, "ls -l %s", local_dir);
    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        printf("Error popen(): %s(%d)\r\n", strerror(errno), errno);
        return;
    }
    ui->clientFileTable->clear();
    ui->clientFileTable->setRowCount(0);
    char buffer[8192];
    QString qs = "";
    while (fgets(buffer, 8192, fp) != NULL)
    {
        qs.append(buffer);
    }
    pclose(fp);
    QStringList lines = qs.split("\n");
    for (const QString &line : lines)
    {
        if (!line.isEmpty())
        {
            if (line.indexOf("total") == 0)
                continue;

            QStringList parts = line.split(QRegularExpression("\\s+"));

            ui->clientFileTable->insertRow(ui->clientFileTable->rowCount());
            ui->clientFileTable->setItem(ui->clientFileTable->rowCount() - 1, 0, new QTableWidgetItem(parts[8]));
            ui->clientFileTable->setItem(ui->clientFileTable->rowCount() - 1, 1, new QTableWidgetItem(parts[4]));
            ui->clientFileTable->setItem(ui->clientFileTable->rowCount() - 1, 2, new QTableWidgetItem(parts[5] + " " + parts[6] + " " + parts[7]));
            ui->clientFileTable->setItem(ui->clientFileTable->rowCount() - 1, 3, new QTableWidgetItem(parts[0]));
            ui->clientFileTable->setItem(ui->clientFileTable->rowCount() - 1, 4, new QTableWidgetItem(parts[1]));
            ui->clientFileTable->setItem(ui->clientFileTable->rowCount() - 1, 5, new QTableWidgetItem(parts[2]));
            ui->clientFileTable->setItem(ui->clientFileTable->rowCount() - 1, 6, new QTableWidgetItem(parts[3]));

            QStringList title = {"Name", "Size", "Time", "Permission", "Type", "Owner", "Group"};
            ui->clientFileTable->setHorizontalHeaderLabels(title);
        }
    }
}

int read_reply(char *str, int max_len)
{
    if (state.sockfd == -1)
    {
        printf("Error read(): socket not connected\n");
        return 1;
    }
    int p = 0;
    while (1)
    {
        int n = read(state.sockfd, str + p, max_len - p);
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
            while (str[p - 1] == 0)
            {
                p--;
            }
            if (str[p - 1] == '\n')
            {
                break;
            }
        }
    }
    str[p - 1] = 0;
    printf("FROM SERVER: %s \r\n", str);
    ui->shellInput->setText(ui->shellInput->toPlainText() + str);
    return 0;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ui = new FtpClientUI();
    ui->getWindow()->show();
    // get /home/username/Downloads
    char home[256];
    strcpy(home, getenv("HOME"));
    sprintf(local_dir, "%s/Downloads", home);
    ui->clientPathInput->setText(local_dir);
    list_local();
    return app.exec();
}
