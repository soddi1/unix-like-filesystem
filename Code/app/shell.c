#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fs.h"
#include "disk.h"

char LINE[1024];
char COMMAND[1024];
char ARG_1[1024];
char ARG_2[1024];

int copy_in(char *local_path, char *fs_path);
int copy_out(char *fs_path, char *local_path);


int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: ./shell <disk> <number-of-blocks>\n");
        return -1;
    }

    if (disk_init(argv[1], atoi(argv[2])) == -1)
    {
        printf("ERROR: Could not initialize disk.\n");
        return -1;
    }

    printf("Disk Initialized.\n");
    printf("Disk: %s\n", argv[1]);
    printf("Blocks: %d\n", disk_size());

    while (1)
    {
        printf("$ ");
        fflush(stdout);

        char* result = fgets(LINE, sizeof(LINE), stdin);

        if (result == NULL)
        {
            break;
        }

        if (LINE[0] == '\n')
        {
            continue;
        }

        if (LINE[strlen(LINE) - 1] == '\n')
        {
            LINE[strlen(LINE) - 1] = '\0';
        }

        int args = sscanf(LINE, "%s %s %s", COMMAND, ARG_1, ARG_2);

        if (args == 0)
        {
            continue;
        }

        if (strcmp(COMMAND, "exit") == 0)
        {
            break;
        }
        else if (strcmp(COMMAND, "help") == 0)
        {
            printf("Commands:\n");
            printf("    exit\n");
            printf("    help\n");
            printf("    format\n");
            printf("    mount\n");
            printf("    stat\n");
            printf("    ls <path>\n");
            printf("    cat <path>\n");
            printf("    delete <path>\n");
            printf("    copy_in <local_path> <fs_path>\n");
            printf("    copy_out <fs_path> <local_path>\n");
        }
        else if (strcmp(COMMAND, "format") == 0)
        {
            if (args != 1)
            {
                printf("ERROR: Invalid arguments.\n");
                continue;
            }

            printf("Formatting disk...\n");
            
            if (fs_format() == -1)
            {
                printf("ERROR: Could not format disk.\n");
                continue;
            }

            printf("Disk formatted successfully.\n");
        }

        else if (strcmp(COMMAND, "mount") == 0)
        {
            if (args != 1)
            {
                printf("ERROR: Invalid arguments.\n");
                continue;
            }

            printf("Mounting disk...\n");
            
            if (fs_mount() == -1)
            {
                printf("ERROR: Could not mount disk.\n");
                continue;
            }

            printf("Disk mounted successfully.\n");
        }
        else if (strcmp(COMMAND, "stat") == 0)
        {
            fs_stat();
        }
        else if (strcmp(COMMAND, "ls") == 0)
        {
            if (args != 2)
            {
                printf("ERROR: Invalid arguments.\n");
                continue;
            }

            if (fs_list(ARG_1) == -1)
            {
                printf("ERROR: Could not list directory.\n");
                continue;
            }
        }
        else if (strcmp(COMMAND, "cat") == 0)
        {
            if (args != 2)
            {
                printf("ERROR: Invalid arguments.\n");
                continue;
            }

            if (copy_out(ARG_1, "/dev/stdout"))
            {
                printf("ERROR: Could not cat file.\n");
                continue;
            }
        }
        else if (strcmp(COMMAND, "delete") == 0)
        {
            if (args != 2)
            {
                printf("ERROR: Invalid arguments.\n");
                continue;
            }

            if (fs_remove(ARG_1) == -1)
            {
                printf("ERROR: Could not delete file.\n");
                continue;
            }
        }
        else if (strcmp(COMMAND, "copy_in") == 0)
        {
            if (args != 3)
            {
                printf("ERROR: Invalid arguments.\n");
                continue;
            }

            if (copy_in(ARG_1, ARG_2))
            {
                printf("ERROR: Could not copy file.\n");
                continue;
            }
        }
        else if (strcmp(COMMAND, "copy_out") == 0)
        {
            if (args != 3)
            {
                printf("ERROR: Invalid arguments.\n");
                continue;
            }

            if (copy_out(ARG_1, ARG_2))
            {
                printf("ERROR: Could not copy file.\n");
                continue;
            }
        }
        else 
        {
            printf("ERROR: Invalid command.\n");
            continue;
        }

    }

    printf("Exiting...\n");

    if (disk_close() == -1)
    {
        return -1;
    }

    return 0;
}

int copy_in(char *local_path, char *fs_path)
{
    FILE *local_file = fopen(local_path, "r");

    if (local_file == NULL)
    {
        printf("ERROR: Could not open local file.\n");
        return -1;
    }

    fseek(local_file, 0, SEEK_END);
    int local_file_size = ftell(local_file);
    fseek(local_file, 0, SEEK_SET);

    char *local_file_buffer = calloc(local_file_size, sizeof(char));

    if (local_file_buffer == NULL)
    {
        printf("ERROR: Could not allocate buffer for local file.\n");
        return -1;
    }
    if ((int)fread(local_file_buffer, sizeof(char), local_file_size, local_file) != local_file_size)
    {
        printf("ERROR: Could not read local file into buffer.\n");
        return -1;
    }

    if (fclose(local_file) == EOF)
    {
        printf("ERROR: Could not close local file.\n");
        return -1;
    }

    if (fs_write(fs_path, local_file_buffer, local_file_size, 0) == -1)
    {
        printf("ERROR: Could not write local file buffer to FS file.\n");
        return -1;
    }

    free(local_file_buffer);

    return 0;
}

int copy_out(char *fs_path, char *local_path)
{
    FILE *local_file = fopen(local_path, "a");

    if (local_file == NULL)
    {
        printf("ERROR: Could not open local file.\n");
        return -1;
    }

    char *fs_file_buffer = calloc(BLOCK_SIZE, sizeof(char));

    if (fs_file_buffer == NULL)
    {
        printf("ERROR: Could not allocate buffer for FS file.\n");
        return -1;
    }

    off_t offset = 0;
    while (1)
    {
        int bytes_read = fs_read(fs_path, fs_file_buffer, BLOCK_SIZE, offset);

        if (bytes_read == -1)
        {
            printf("ERROR: Could not read FS file into buffer.\n");
            return -1;
        }

        if ((int)fwrite(fs_file_buffer, sizeof(char), bytes_read, local_file) != bytes_read)
        {
            printf("ERROR: Could not write buffer to local file.\n");
            return -1;
        }

        if (bytes_read < BLOCK_SIZE)
        {
            break;
        }

        offset += bytes_read;
    }

    if (fclose(local_file) == EOF)
    {
        printf("ERROR: Could not close local file.\n");
        return -1;
    }

    free(fs_file_buffer);

    return 0;
}