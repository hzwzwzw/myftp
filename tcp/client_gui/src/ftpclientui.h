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
void button_login_clicked();

void button_server_enter_clicked();
void button_server_parent_clicked();
void button_client_enter_clicked();
void button_client_parent_clicked();

void port_checked();
void pasv_checked();

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
        serverAddrInput->setText("127.0.0.1");
        serverPortInput->setPlaceholderText("Port");
        serverPortInput->setText("21");
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
        usernameInput->setText("anonymous");
        passwordInput->setPlaceholderText("Password");
        passwordInput->setText("123");
        passwordInput->setEchoMode(QLineEdit::Password);
        loginButton->setText("Login");
        loginLayout->addWidget(usernameInput);
        loginLayout->addWidget(passwordInput);
        loginLayout->addWidget(loginButton);
        mainLayout->addLayout(loginLayout);
        loginButton->connect(loginButton, &QPushButton::clicked, button_login_clicked);

        // Port mode and PASV mode selection
        modeLayout = new QHBoxLayout();
        portModeButton = new QRadioButton();
        pasvModeButton = new QRadioButton();
        portModeButton->setText("PORT Mode");
        pasvModeButton->setText("PASV Mode");
        pasvModeButton->setChecked(true);
        modeLayout->addWidget(portModeButton);
        modeLayout->addWidget(pasvModeButton);
        mainLayout->addLayout(modeLayout);
        portModeButton->connect(portModeButton, &QRadioButton::clicked, port_checked);
        pasvModeButton->connect(pasvModeButton, &QRadioButton::clicked, pasv_checked);

        // Server file list
        serverFileLayout = new QVBoxLayout();
        serverInputLayout = new QHBoxLayout();
        serverLabel = new QLabel();
        serverPathInput = new QLineEdit();
        serverCwdButton = new QPushButton();
        serverParentButton = new QPushButton();
        serverFileTable = new QTableWidget();
        serverLabel->setText("Server Files");
        serverPathInput->setPlaceholderText("Server Path");
        serverPathInput->setText("/");
        serverCwdButton->setText("Enter");
        serverParentButton->setText("..");
        serverFileTable->setColumnCount(7);
        serverFileTable->horizontalHeader()->setStretchLastSection(true);
        serverFileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        serverFileLayout->addWidget(serverLabel);
        serverInputLayout->addWidget(serverPathInput);
        serverInputLayout->addWidget(serverCwdButton);
        serverInputLayout->addWidget(serverParentButton);
        serverFileLayout->addLayout(serverInputLayout);
        serverFileLayout->addWidget(serverFileTable);
        mainLayout->addLayout(serverFileLayout);
        serverCwdButton->connect(serverCwdButton, &QPushButton::clicked, button_server_enter_clicked);
        serverParentButton->connect(serverParentButton, &QPushButton::clicked, button_server_parent_clicked);

        // Client folder file list
        clientFileLayout = new QVBoxLayout();
        clientInputLayout = new QHBoxLayout();
        clientLabel = new QLabel();
        clientPathInput = new QLineEdit();
        clientCwdButton = new QPushButton();
        clientParentButton = new QPushButton();
        clientFileTable = new QTableWidget();
        clientLabel->setText("Client Files");
        clientPathInput->setPlaceholderText("Client Path");
        clientPathInput->setText("/");
        clientCwdButton->setText("Enter");
        clientParentButton->setText("..");
        clientFileTable->setColumnCount(7);
        clientFileTable->horizontalHeader()->setStretchLastSection(true);
        clientFileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        clientFileLayout->addWidget(clientLabel);
        clientInputLayout->addWidget(clientPathInput);
        clientInputLayout->addWidget(clientCwdButton);
        clientInputLayout->addWidget(clientParentButton);
        clientFileLayout->addLayout(clientInputLayout);
        clientFileLayout->addWidget(clientFileTable);
        mainLayout->addLayout(clientFileLayout);
        clientCwdButton->connect(clientCwdButton, &QPushButton::clicked, button_client_enter_clicked);
        clientParentButton->connect(clientParentButton, &QPushButton::clicked, button_client_parent_clicked);

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

        window->resize(1200, 1080);
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
    QHBoxLayout *serverInputLayout;
    QLabel *serverLabel;
    QPushButton *serverCwdButton;
    QPushButton *serverParentButton;
    QLineEdit *serverPathInput;
    QTableWidget *serverFileTable;

    QVBoxLayout *clientFileLayout;
    QHBoxLayout *clientInputLayout;
    QLabel *clientLabel;
    QPushButton *clientCwdButton;
    QPushButton *clientParentButton;
    QLineEdit *clientPathInput;
    QTableWidget *clientFileTable;

    QHBoxLayout *transferLayout;
    QPushButton *uploadButton;
    QPushButton *downloadButton;

    QVBoxLayout *shellLayout;
    QLabel *shellLabel;
    QTextEdit *shellInput;
};
