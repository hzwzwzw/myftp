#include <stdio.h>

int main()
{
    char path[256] = "/home/ftp/a.txt";
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        printf("Error fopen()\r\n");
        return 1;
    }
    else
    {
        printf("File opened successfully.\r\n");
    }
    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    printf("File size: %d bytes.\r\n", file_size);
}