#include <iostream>
#include <string>

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

bool gui_mode = false;

char local_dir[256];

FtpState state;

void gui_list_server();
void gui_list_local();

void set_data_socket();
void connect_to_data_socket();

void error(const char *message, const char *strerror_, int errorno)
{
    if (gui_mode)
        QMessageBox::critical(ui->getWindow(), "Error", message);
    fprintf(stderr, "Error: %s, %s (%d)\r\n", message, strerror_, errorno);
}

void gui_cwd_server()
{
    char path[256];
    strcpy(path, ui->serverPathInput->text().toUtf8().data());
    char buf[256 + 8];
    snprintf(buf, sizeof(buf), "CWD %s\r\n", path);
    write(state.sockfd, buf, strlen(buf));
    read_reply(buf, 256);
    gui_list_server();
    // use PWD to get current directory
    write(state.sockfd, "PWD\r\n", strlen("PWD\r\n"));
    read_reply(buf, 256);
    if (strncmp(buf, "257", 3) == 0)
    {
        char *dir;
        dir = strstr(buf, "\"") + 1;
        dir[strstr(dir, "\"") - dir] = 0;
        // printf("Current directory: %s\r\n", dir);
        ui->serverPathInput->setText(dir);
        strcpy(state.dir, dir);
    }
}

void gui_cwd_local()
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
    gui_list_local();
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

void gui_download()
{
    QString filename = ui->serverFileTable->item(ui->serverFileTable->currentRow(), 0)->text();
    // check if same filename exists in local
    char path[512 + 2];
    snprintf(path, sizeof(path), "%s/%s", local_dir, filename.toUtf8().data());
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
    set_data_socket();
    download(filename, rest);
    gui_cwd_local();
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

void gui_upload()
{
    QString filename = ui->clientFileTable->item(ui->clientFileTable->currentRow(), 0)->text();
    bool appe = false;
    long appe_begin = 0;
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
    set_data_socket();
    upload(filename, appe, appe_begin);
    gui_cwd_server();
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
    gui_list_server();
}

void button_server_enter_clicked()
{
    gui_cwd_server();
}

void button_server_parent_clicked()
{
    char path[256 + 5];
    snprintf(path, sizeof(path), "%s/..", state.dir);
    ui->serverPathInput->setText(path);
    gui_cwd_server();
}

void button_client_enter_clicked()
{
    gui_cwd_local();
}

void button_client_parent_clicked()
{
    char path[256 + 5];
    snprintf(path, sizeof(path), "%s/..", local_dir);
    ui->clientPathInput->setText(path);
    gui_cwd_local();
}

void button_server_doubleclicked(int row, int column)
{
    QString filename = ui->serverFileTable->item(row, 0)->text();
    if (ui->serverFileTable->item(row, 3)->text()[0] == 'd')
    {
        char tmp[512 + 2];
        snprintf(tmp, sizeof(tmp), "%s/%s", state.dir, filename.toUtf8().data());
        ui->serverPathInput->setText(tmp);
        gui_cwd_server();
    }
    else
    {
        gui_download();
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
        gui_cwd_local();
    }
    else
    {
        gui_upload();
    }
}

void button_download()
{
    gui_download();
}

void button_upload()
{
    gui_upload();
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
    // printf("Local IP: %s\n", address);
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

void gui_list_server()
{
    set_data_socket();
    QString qs = list_server();
    if (qs == "")
    {
        return;
    }
    ui->serverFileTable->clear();
    ui->serverFileTable->setRowCount(0);
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

void gui_list_local()
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
    memset(str, 0, max_len);
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
                p--;
            if (str[p] == 0)
                break;
        }
    }
    printf("%s", str);
    if (gui_mode)
    {
        ui->shellInput->setText(ui->shellInput->toPlainText() + str);
        ui->window->repaint();
    }
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
        else if (strcmp(argv[i], "-gui") == 0)
        {
            gui_mode = true;
        }
        else
        {
            printf("Unknown option: %s\n", argv[i]);
            return 1;
        }
    }
    // // get /home/username/Downloads
    // char home[128];
    // strcpy(home, getenv("HOME"));
    // snprintf(local_dir, sizeof(local_dir), "%s/Downloads", home);
    getcwd(local_dir, sizeof(local_dir));

    if (gui_mode)
    {
        QApplication app(argc, argv);
        ui = new FtpClientUI();
        ui->getWindow()->show();
        ui->serverAddrInput->setText(addr);
        ui->serverPortInput->setText(port);
        ui->clientPathInput->setText(local_dir);
        gui_list_local();
        return app.exec();
    }
    else
    {
        if (connect_to_server(addr, atoi(port)) != 0)
        {
            return 1;
        }
        // printf("\n");
        char buf[256];
        bool rest = false;
        while (true)
        {
            memset(buf, 0, 256);
            // std::cout << "ftp> ";
            std::cin.getline(buf, sizeof(buf));
            if (buf == "exit")
            {
                break;
            }
            char command[256], argument[256];
            memset(command, 0, 256);
            memset(argument, 0, 256);
            sscanf(buf, "%s %s", command, argument);
            // some special commands needs extra process or redirect
            if (strcmp(command, "RETR") == 0)
            {
                download(argument, rest);
                rest = false;
            }
            else if (strcmp(command, "STOR") == 0)
            {
                upload(argument, false, 0);
            }
            else if (strcmp(command, "PORT") == 0)
            {
                if (strlen(argument) != 0)
                {
                    // like a,b,c,d,p,q
                    int a, b, c, d, p, q;
                    sscanf(argument, "%d,%d,%d,%d,%d,%d", &a, &b, &c, &d, &p, &q);
                    char ip[16];
                    snprintf(ip, 16, "%d.%d.%d.%d", a, b, c, d);
                    set_port(ip, p * 256 + q);
                }
                else
                {
                    set_port();
                }
            }
            else if (strcmp(command, "PASV") == 0)
            {
                set_pasv();
            }
            else if (strcmp(command, "REST") == 0)
            {
                write(state.sockfd, buf, strlen(buf));
                write(state.sockfd, "\r\n", strlen("\r\n"));
                memset(buf, 0, 256);
                read_reply(buf, 256);
                // printf("\r\n");
                rest = true;
            }
            else if (strcmp(command, "APPE") == 0)
            {
                upload(argument, true, 0);
            }
            else if (strcmp(command, "LIST") == 0)
            {
                list_server();
            }
            else
            {
                write(state.sockfd, buf, strlen(buf));
                write(state.sockfd, "\r\n", strlen("\r\n"));
                bool finished = false;
                bool first = true;
                do
                {
                    memset(buf, 0, 256);
                    read_reply(buf, sizeof(buf));
                    // find if output finished
                    if (first && buf[3] == ' ')
                    {
                        break;
                    }
                    for (int i = 0; buf[i] && buf[i + 4]; i++)
                    {
                        if (buf[i] == '\n' && buf[i + 4] == ' ')
                        {
                            finished = true;
                            break;
                        }
                    }
                    if (buf[strlen(buf) - 1] == '\n')
                    {
                        first = true;
                    }
                } while (!finished);
                // printf("\r\n");
                if (strcmp(command, "QUIT") == 0)
                {
                    break;
                }
            }
        }
    }
}
