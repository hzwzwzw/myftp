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

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    QHBoxLayout windowLayout;
    QVBoxLayout mainLayout;

    // FTP server address and port input
    QHBoxLayout serverLayout;
    QLineEdit serverAddrInput;
    QLineEdit serverPortInput;
    QPushButton connectButton;
    serverAddrInput.setPlaceholderText("FTP Server Address");
    serverPortInput.setPlaceholderText("Port");
    connectButton.setText("Connect");
    serverLayout.addWidget(&serverAddrInput);
    serverLayout.addWidget(&serverPortInput);
    serverLayout.addWidget(&connectButton);
    mainLayout.addLayout(&serverLayout);

    // Username and password input
    QHBoxLayout loginLayout;
    QLineEdit usernameInput;
    QLineEdit passwordInput;
    QPushButton loginButton;
    usernameInput.setPlaceholderText("Username");
    passwordInput.setPlaceholderText("Password");
    passwordInput.setEchoMode(QLineEdit::Password);
    loginButton.setText("Login");
    loginLayout.addWidget(&usernameInput);
    loginLayout.addWidget(&passwordInput);
    loginLayout.addWidget(&loginButton);
    mainLayout.addLayout(&loginLayout);

    // Port mode and PASV mode selection
    QHBoxLayout modeLayout;
    QRadioButton portModeButton;
    QRadioButton pasvModeButton;
    portModeButton.setText("Port Mode");
    pasvModeButton.setText("PASV Mode");
    modeLayout.addWidget(&portModeButton);
    modeLayout.addWidget(&pasvModeButton);
    mainLayout.addLayout(&modeLayout);

    // Server file list
    QVBoxLayout serverFileLayout;
    QLabel serverLabel;
    QLineEdit serverPathInput;
    QTableWidget serverFileTable;
    serverLabel.setText("Server Files");
    serverPathInput.setPlaceholderText("Server Path");
    serverFileTable.setColumnCount(1);
    serverFileTable.setHorizontalHeaderLabels({"Filename"});
    serverFileTable.horizontalHeader()->setStretchLastSection(true);
    serverFileLayout.addWidget(&serverLabel);
    serverFileLayout.addWidget(&serverPathInput);
    serverFileLayout.addWidget(&serverFileTable);
    mainLayout.addLayout(&serverFileLayout);

    // Client folder file list
    QVBoxLayout clientFileLayout;
    QLabel clientLabel;
    QLineEdit clientPathInput;
    QTableWidget clientFileTable;
    clientLabel.setText("Client Files");
    clientPathInput.setPlaceholderText("Client Path");
    clientFileTable.setColumnCount(1);
    clientFileTable.setHorizontalHeaderLabels({"Filename"});
    clientFileTable.horizontalHeader()->setStretchLastSection(true);
    clientFileLayout.addWidget(&clientLabel);
    clientFileLayout.addWidget(&clientPathInput);
    clientFileLayout.addWidget(&clientFileTable);
    mainLayout.addLayout(&clientFileLayout);

    // Upload and download buttons
    QHBoxLayout transferLayout;
    QPushButton uploadButton;
    QPushButton downloadButton;
    uploadButton.setText("Upload");
    downloadButton.setText("Download");
    transferLayout.addWidget(&uploadButton);
    transferLayout.addWidget(&downloadButton);
    mainLayout.addLayout(&transferLayout);

    windowLayout.addLayout(&mainLayout);

    // Shell message display area
    QVBoxLayout shellLayout;
    QLabel shellLabel;
    QLineEdit shellInput;
    shellLabel.setText("Shell Messages");
    shellInput.setReadOnly(true);
    shellInput.setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Expanding);
    shellLayout.addWidget(&shellLabel);
    shellLayout.addWidget(&shellInput);

    windowLayout.addLayout(&shellLayout);

    window.setLayout(&windowLayout);
    window.show();

    return app.exec();
}