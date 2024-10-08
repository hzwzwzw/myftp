#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QHeaderView>

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
        shellInput = new QLineEdit();
        shellLabel->setText("Shell Messages");
        shellInput->setReadOnly(true);
        shellInput->setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Expanding);
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

private:
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
    QLineEdit *shellInput;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    FtpClientUI ui;
    ui.getWindow()->show();

    return app.exec();
}
