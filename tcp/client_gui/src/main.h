#include "ftpClientBase.h"
#include "ui.h"

FtpClientUI *ui;

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
    // if (state.sockfd == -1)
    // {
    //     error("Please connect to server first.", "", 0);
    //     return;
    // }
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
