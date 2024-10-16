#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <memory.h>
#include <stdio.h>

#include <sys/stat.h>
#include <fcntl.h>

#include "ftpclientui.h"

#include <QRegularExpression>
#include <QNetworkInterface>
#include <QMessageBox>

#define MODE_transfer_undefined 0
#define MODE_transfer_port 1
#define MODE_transfer_pasv 2

int read_reply(char *str, int max_len);

FtpClientUI *ui;

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

char local_dir[256];

FtpState state;

void list_server();
void list_local();

void set_data_socket();
void connect_to_data_socket();

void error(const char *message, const char *strerror_, int errorno)
{
    QMessageBox::critical(ui->getWindow(), "Error", message);
    printf("Error: %s, %s (%d)\r\n", message, strerror_, errorno);
}

void cwd_server()
{
    char path[256];
    strcpy(path, ui->serverPathInput->text().toUtf8().data());
    char buf[256 + 8];
    snprintf(buf, sizeof(buf), "CWD %s\r\n", path);
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
    char tmp[512 + 2];
    if (path[0] == '/')
        strcpy(tmp, path);
    else
        snprintf(tmp, sizeof(tmp), "%s/%s", local_dir, path);

    char real[256];
    if (realpath(tmp, real) == NULL)
    {
        error("Error realpath()\r\n", strerror(errno), errno);
        return;
    }
    // check if it exits and is a directory
    struct stat st;
    if (stat(real, &st) == -1)
    {
        error("Error stat()\r\n", strerror(errno), errno);
        return;
    }
    if (!S_ISDIR(st.st_mode))
    {
        error("Error: not a directory\r\n", "", 0);
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
        error("Error socket.", strerror(errno), errno);
        return 1;
    }

    // 设置目标主机的ip和port
    memset(&state.addr, 0, sizeof(state.addr));
    state.addr.sin_family = AF_INET;
    state.addr.sin_port = port;
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

void download()
{
    set_data_socket();
    QString filename = ui->serverFileTable->item(ui->serverFileTable->currentRow(), 0)->text();
    char filename_[256];
    strcpy(filename_, filename.toUtf8().data());
    // check if same filename exists in local
    char path[512 + 2];
    snprintf(path, sizeof(path), "%s/%s", local_dir, filename_);
    struct stat st;
    bool rest = false;
    if (stat(path, &st) != -1)
    {
        // check if filesize is the same
        long server_file_size = ui->serverFileTable->item(ui->serverFileTable->currentRow(), 1)->text().toLong();
        if (st.st_size >= server_file_size)
        {
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(ui->getWindow(), "File exists", "File exists. Overwrite?", QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::No)
            {
                return;
            }
        }
        else
        {
            QMessageBox::StandardButton reply;
            QString msg = "File exists and size < that on server. Append or Overwrite?\nYes: append, No: overwrite, Cancel: do nothing.";
            reply = QMessageBox::question(ui->getWindow(), "File exists", msg, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            if (reply == QMessageBox::Cancel)
            {
                return;
            }
            else if (reply == QMessageBox::Yes)
            {
                // warning that
                reply = QMessageBox::question(ui->getWindow(), "Warning", "Wrong appending may cause file corruption. Continue?", QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::No)
                {
                    return;
                }
                // REST
                char buf[256];
                sprintf(buf, "REST %ld\r\n", st.st_size);
                write(state.sockfd, buf, strlen(buf));
                read_reply(buf, 256);
                rest = true;
            }
        }
    }
    char buf[256];
    sprintf(buf, "RETR %s\r\n", filename.toUtf8().data());
    write(state.sockfd, buf, strlen(buf));
    connect_to_data_socket();
    bool continueread = true;
    read_reply(buf, 256);
    if (buf[3] == ' ')
    {
        continueread = false;
    }
    ui->window->repaint();
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
    while (continueread)
    {
        read_reply(buf, 256);
        if (buf[3] == ' ')
        {
            break;
        }
    }
    printf("Transfer complete.\r\n");
    cwd_local();
}

void upload()
{
    set_data_socket();
    QString filename = ui->clientFileTable->item(ui->clientFileTable->currentRow(), 0)->text();
    bool appe = false;
    long appe_begin;
    // check if same filename exists in server
    for (int i = 0; i < ui->serverFileTable->rowCount(); i++)
    {
        if (filename == ui->serverFileTable->item(i, 0)->text())
        {
            long local_file_size = ui->clientFileTable->item(ui->clientFileTable->currentRow(), 1)->text().toLong();
            long server_file_size = ui->serverFileTable->item(i, 1)->text().toLong();
            if (local_file_size <= server_file_size)
            {
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(ui->getWindow(), "File exists", "File exists. Overwrite?", QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::No)
                {
                    return;
                }
            }
            else
            {
                QMessageBox::StandardButton reply;
                QString msg = "File exists and size < that on client. Append or Overwrite?\nYes: append, No: overwrite, Cancel: do nothing.";
                reply = QMessageBox::question(ui->getWindow(), "File exists", msg, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
                if (reply == QMessageBox::Cancel)
                {
                    return;
                }
                else if (reply == QMessageBox::Yes)
                {
                    // warning that
                    reply = QMessageBox::question(ui->getWindow(), "Warning", "Wrong appending may cause file corruption. Continue?", QMessageBox::Yes | QMessageBox::No);
                    if (reply == QMessageBox::No)
                    {
                        return;
                    }
                    appe = true;
                    appe_begin = server_file_size;
                }
            }
        }
    }
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
    bool continueread = true;
    read_reply(buf, 256);
    if (buf[3] == ' ')
    {
        continueread = false;
    }
    ui->window->repaint();
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
    while (continueread)
    {
        read_reply(buf, 256);
        if (buf[3] == ' ')
        {
            break;
        }
    }
    printf("Transfer complete.\r\n");
    cwd_server();
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
    if (state.sockfd == -1)
    {
        error("Please connect to server first.", "", 0);
        return;
    }
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
    char path[256 + 5];
    snprintf(path, sizeof(path), "%s/..", state.dir);
    ui->serverPathInput->setText(path);
    cwd_server();
}

void button_client_enter_clicked()
{
    cwd_local();
}

void button_client_parent_clicked()
{
    char path[256 + 5];
    snprintf(path, sizeof(path), "%s/..", local_dir);
    ui->clientPathInput->setText(path);
    cwd_local();
}

void button_server_doubleclicked(int row, int column)
{
    QString filename = ui->serverFileTable->item(row, 0)->text();
    if (ui->serverFileTable->item(row, 3)->text()[0] == 'd')
    {
        char tmp[512 + 2];
        snprintf(tmp, sizeof(tmp), "%s/%s", state.dir, filename.toUtf8().data());
        ui->serverPathInput->setText(tmp);
        cwd_server();
    }
    else
    {
        download();
    }
}

void button_client_doubleclicked(int row, int column)
{
    QString filename = ui->clientFileTable->item(row, 0)->text();
    if (ui->clientFileTable->item(row, 3)->text()[0] == 'd')
    {
        char tmp[512 + 2];
        snprintf(tmp, sizeof(tmp), "%s/%s", local_dir, filename.toUtf8().data());
        ui->clientPathInput->setText(tmp);
        cwd_local();
    }
    else
    {
        upload();
    }
}

void button_download()
{
    download();
}

void button_upload()
{
    upload();
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
                error("Error read.", strerror(errno), errno);
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
    char command[256 + 10];
    sprintf(command, "ls -l %s", local_dir);
    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        error("Error popen.", strerror(errno), errno);
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
        error("Error sockfd.", "", 0);
        return 1;
    }
    int p = 0;
    while (1)
    {
        int n = read(state.sockfd, str + p, max_len - p);
        if (n < 0)
        {
            error("Error read.", strerror(errno), errno);
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
    char addr[16] = "127.0.0.1";
    char port[6] = "21";
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-ip") == 0)
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                strcpy(addr, argv[++i]);
            }
            else
            {
                printf("Missing argument for -ip\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-port") == 0)
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                strcpy(port, argv[++i]);
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
    QApplication app(argc, argv);
    ui = new FtpClientUI();
    ui->getWindow()->show();
    ui->serverAddrInput->setText(addr);
    ui->serverPortInput->setText(port);
    // get /home/username/Downloads
    char home[128];
    strcpy(home, getenv("HOME"));
    snprintf(local_dir, sizeof(local_dir), "%s/Downloads", home);
    ui->clientPathInput->setText(local_dir);
    list_local();
    return app.exec();
}
