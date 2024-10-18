#include "main.h"

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
