#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <memory.h>
#include <stdio.h>

#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QHeaderView>

void button_connect_clicked();
int read_reply(char *str, int max_len);

class FtpClientUI
{
public:
    FtpClientUI()
    {
        // Initialize pointers to null
        window = new QWidget();
        windowLayout = new QHBoxLayout();
        mainLayout = new QVBoxLayout();

        // FTP server address and port input
        serverLayout = new QHBoxLayout();
        serverAddrInput = new QLineEdit();
        serverPortInput = new QLineEdit();
        connectButton = new QPushButton();
        serverAddrInput->setPlaceholderText("FTP Server Address");
        serverPortInput->setPlaceholderText("Port");
        connectButton->setText("Connect");
        serverLayout->addWidget(serverAddrInput);
        serverLayout->addWidget(serverPortInput);
        serverLayout->addWidget(connectButton);
        mainLayout->addLayout(serverLayout);
        connectButton->connect(connectButton, &QPushButton::clicked, button_connect_clicked);

        // Username and password input
        loginLayout = new QHBoxLayout();
        usernameInput = new QLineEdit();
        passwordInput = new QLineEdit();
        loginButton = new QPushButton();
        usernameInput->setPlaceholderText("Username");
        passwordInput->setPlaceholderText("Password");
        passwordInput->setEchoMode(QLineEdit::Password);
        loginButton->setText("Login");
        loginLayout->addWidget(usernameInput);
        loginLayout->addWidget(passwordInput);
        loginLayout->addWidget(loginButton);
        mainLayout->addLayout(loginLayout);

        // Port mode and PASV mode selection
        modeLayout = new QHBoxLayout();
        portModeButton = new QRadioButton();
        pasvModeButton = new QRadioButton();
        portModeButton->setText("Port Mode");
        pasvModeButton->setText("PASV Mode");
        modeLayout->addWidget(portModeButton);
        modeLayout->addWidget(pasvModeButton);
        mainLayout->addLayout(modeLayout);

        // Server file list
        serverFileLayout = new QVBoxLayout();
        serverLabel = new QLabel();
        serverPathInput = new QLineEdit();
        serverFileTable = new QTableWidget();
        serverLabel->setText("Server Files");
        serverPathInput->setPlaceholderText("Server Path");
        serverFileTable->setColumnCount(1);
        serverFileTable->setHorizontalHeaderLabels({"Filename"});
        serverFileTable->horizontalHeader()->setStretchLastSection(true);
        serverFileLayout->addWidget(serverLabel);
        serverFileLayout->addWidget(serverPathInput);
        serverFileLayout->addWidget(serverFileTable);
        mainLayout->addLayout(serverFileLayout);

        // Client folder file list
        clientFileLayout = new QVBoxLayout();
        clientLabel = new QLabel();
        clientPathInput = new QLineEdit();
        clientFileTable = new QTableWidget();
        clientLabel->setText("Client Files");
        clientPathInput->setPlaceholderText("Client Path");
        clientFileTable->setColumnCount(1);
        clientFileTable->setHorizontalHeaderLabels({"Filename"});
        clientFileTable->horizontalHeader()->setStretchLastSection(true);
        clientFileLayout->addWidget(clientLabel);
        clientFileLayout->addWidget(clientPathInput);
        clientFileLayout->addWidget(clientFileTable);
        mainLayout->addLayout(clientFileLayout);

        // Upload and download buttons
        transferLayout = new QHBoxLayout();
        uploadButton = new QPushButton();
        downloadButton = new QPushButton();
        uploadButton->setText("Upload");
        downloadButton->setText("Download");
        transferLayout->addWidget(uploadButton);
        transferLayout->addWidget(downloadButton);
        mainLayout->addLayout(transferLayout);

        windowLayout->addLayout(mainLayout);

        // Shell message display area
        shellLayout = new QVBoxLayout();
        shellLabel = new QLabel();
        shellInput = new QTextEdit();
        shellLabel->setText("Shell Messages");
        shellLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        shellInput->setReadOnly(true);
        shellInput->setAlignment(Qt::AlignTop);
        shellInput->setWordWrapMode(QTextOption::WordWrap);
        shellInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        shellLayout->addWidget(shellLabel);
        shellLayout->addWidget(shellInput);

        windowLayout->addLayout(shellLayout);

        window->setLayout(windowLayout);
    }

    ~FtpClientUI()
    {
        delete serverAddrInput;
        delete serverPortInput;
        delete connectButton;
        delete usernameInput;
        delete passwordInput;
        delete loginButton;
        delete portModeButton;
        delete pasvModeButton;
        delete serverLabel;
        delete serverPathInput;
        delete serverFileTable;
        delete clientLabel;
        delete clientPathInput;
        delete clientFileTable;
        delete uploadButton;
        delete downloadButton;
        delete shellLabel;
        delete shellInput;

        delete serverLayout;
        delete loginLayout;
        delete modeLayout;
        delete serverFileLayout;
        delete clientFileLayout;
        delete transferLayout;
        delete shellLayout;

        delete mainLayout;
        delete windowLayout;
        delete window;
    }

    QWidget *getWindow()
    {
        return window;
    }

public:
    QWidget *window;
    QHBoxLayout *windowLayout;
    QVBoxLayout *mainLayout;

    QHBoxLayout *serverLayout;
    QLineEdit *serverAddrInput;
    QLineEdit *serverPortInput;
    QPushButton *connectButton;

    QHBoxLayout *loginLayout;
    QLineEdit *usernameInput;
    QLineEdit *passwordInput;
    QPushButton *loginButton;

    QHBoxLayout *modeLayout;
    QRadioButton *portModeButton;
    QRadioButton *pasvModeButton;

    QVBoxLayout *serverFileLayout;
    QLabel *serverLabel;
    QLineEdit *serverPathInput;
    QTableWidget *serverFileTable;

    QVBoxLayout *clientFileLayout;
    QLabel *clientLabel;
    QLineEdit *clientPathInput;
    QTableWidget *clientFileTable;

    QHBoxLayout *transferLayout;
    QPushButton *uploadButton;
    QPushButton *downloadButton;

    QVBoxLayout *shellLayout;
    QLabel *shellLabel;
    QTextEdit *shellInput;
};

FtpClientUI *ui;

class FtpState
{
public:
    int sockfd;
    struct sockaddr_in addr;
};

FtpState state;

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
        ui->shellInput->setText("Error connecting to server");
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

    return app.exec();
}
