#include <math.h>
#include <string.h>

#include "fs.h"

int helper_func(int inode);

static int MOUNT_FLAG = 0;
static union block SUPERBLOCK;
static union block BLOCK_BITMAP;
static union block INODE_BITMAP;

#define MAX_TOKENS 100

int fs_format()
{
    if(MOUNT_FLAG==1)
    {
        return -1;
    }

    union block empty_block = {0};

    for (int i=0; i<disk_size(); i++)
    {
        if(disk_write(i, &empty_block)==-1)
        {
            printf("Could not write superblock to disk.\n");
            return -1;
        }
    }

    uint32_t number_of_inodes = ceil((float)disk_size()/INODES_PER_BLOCK);


    union block my_superblock;

    my_superblock.superblock.s_blocks_count = disk_size();
    my_superblock.superblock.s_inodes_count = disk_size();
    my_superblock.superblock.s_block_bitmap = 1;
    my_superblock.superblock.s_inode_bitmap = 2;
    my_superblock.superblock.s_inode_table_block_start = 3;
    my_superblock.superblock.s_data_blocks_start = number_of_inodes+3;

    if (disk_write(0,&my_superblock)==-1)
    {
        printf("Could not write superblock to disk.\n");
        return -1;
    }

    union block b_bmap;
    union block i_bmap;

    for (int i=0; i<FLAGS_PER_BLOCK; i++)
    {
        b_bmap.bitmap[i] = 0;
        i_bmap.bitmap[i] = 0;
    }

    for (int i=0; i<=number_of_inodes+3; i++) {
        b_bmap.bitmap[i] = 1;
    }

    i_bmap.bitmap[0] = 1;

    if (disk_write(1,&b_bmap)==-1)
    {
        printf("Could not write block bitmap to disk.\n");
        return -1;
    }

    if (disk_write(2,&i_bmap)==-1)
    {
        printf("Could not write inode bitmap to disk.\n");
        return -1;
    }

    union block root;
    root.inodes[0].i_is_directory = 1;
    root.inodes[0].i_size = 4096;
    root.inodes[0].i_direct_pointers[0] = number_of_inodes+3;
    for (int i=1; i<INODE_DIRECT_POINTERS; i++)
    {
        root.inodes[0].i_direct_pointers[i] = 0;
    }
    root.inodes[0].i_double_indirect_pointer = 0;
    root.inodes[0].i_single_indirect_pointer = 0;

    for (int i=3; i<number_of_inodes+3; i++)
    {
        union block inode_block = {0};
        for (int j=0; j<INODES_PER_BLOCK; j++)
        {
            if (i==3 && j==0)
            {
                inode_block = root;
            }
            else
            {
                memset(&inode_block.inodes[j], 0, sizeof(struct inode));
            }
        }
        if (disk_write(i,&inode_block)==-1)
        {
            printf("Failed to write inode block to disk.\n");
            return -1;
        }
    }

    union block root_directory;

    for (int i=0; i<DIRECTORY_ENTRIES_PER_BLOCK; i++)
    {
        memset(&root_directory.directory_block.entries[i], 0, sizeof(struct directory_entry));
    }

    if (disk_write(number_of_inodes+3, &root_directory)==-1)
    {
        printf("Could not write root's directory to the disk.\n");
        return -1;
    }

    return 0;
}

int fs_mount()
{

    if (MOUNT_FLAG==1)
    {
        printf("Already mounted.\n");
        return -1;
    }

    union block temp_superblock;

    if (disk_read(0, &temp_superblock)==-1)
    {
        printf("Superblock could not be read.\n");
        return -1;
    }

    SUPERBLOCK = temp_superblock;

    union block temp_block_bmap;

    if (disk_read(1, &temp_block_bmap)==-1)
    {
        printf("Block bit map could not be read.\n");
        return -1;
    }

    BLOCK_BITMAP = temp_block_bmap;

    union block temp_inodes_bmap;

    if (disk_read(2, &temp_inodes_bmap)==-1)
    {
        printf("Inode bit map could not be read.\n");
        return -1;
    }

    INODE_BITMAP = temp_inodes_bmap;

    MOUNT_FLAG = 1;

    return 0;
}

int fs_create(char *path, int is_directory)
{
    if (MOUNT_FLAG == 0)
    {
        printf("Nothing is mounted.\n");
        return -1;
    }

    char path_copy[strlen(path) + 1];
    strcpy(path_copy, path);

    char *token;
    char *tokens[MAX_TOKENS];
    int token_count = 0;

    token = strtok(path_copy, "/");
    while (token != NULL && token_count < MAX_TOKENS)
    {
        tokens[token_count] = malloc(strlen(token) + 1);
        strcpy(tokens[token_count], token);
        token_count++;
        token = strtok(NULL, "/");
    }

    int current_inode = 0;

    for (int i=0; i<token_count; i++)
    {
        int inode_block_number = current_inode/64 + 3;
        int inode_index = current_inode%64;

        union block inode_block;
        if (disk_read(inode_block_number, &inode_block)==-1)
        {
            printf("Could not read inode block.\n");
            return -1;
        }

        int inode_next = -1;

        if (inode_block.inodes[inode_index].i_direct_pointers[0]!=0)
        {
            int file_block_number = inode_block.inodes[inode_index].i_direct_pointers[0];

            union block file_block;

            if (disk_read(file_block_number, &file_block)==-1)
            {
                printf("Could not read data block.\n");
                return -1;
            }

            for (int k=0; k<DIRECTORY_ENTRIES_PER_BLOCK; k++)
            {
                if (strcmp(file_block.directory_block.entries[k].name, tokens[i])==0)
                {
                    inode_next = file_block.directory_block.entries[k].inode_number;
                }
            }
        }


        if (inode_next==-1) //time to create file or folder
        {
            int idx = -1;
            for (int j=0; j<SUPERBLOCK.superblock.s_inodes_count; j++)
            {
                if (INODE_BITMAP.bitmap[j]==0)
                {
                    INODE_BITMAP.bitmap[j] = 1;
                    idx = j;
                    
                    break;
                }
            }

            if (disk_write(2, &INODE_BITMAP)==-1)
            {
                printf("Could not write updated inode bit map back to disk.\n");
                return -1;
            }

            if (idx==-1)
            {
                printf("No free inodes.\n");
                return -1;
            }

            int new_current_inode = idx;
            
            int new_inode_block_number = new_current_inode/64 + 3;
            int new_inode_index = new_current_inode%64;

            union block temp_read;

            if (disk_read(new_inode_block_number, &temp_read)==-1)
            {
                printf("Error reading block.\n");
                return -1;
            }

            if (i!=token_count-1 || (i==token_count-1 && is_directory))
            {
                temp_read.inodes[new_inode_index].i_size = 4096;
                temp_read.inodes[new_inode_index].i_is_directory = 1;
            }
            else
            {
                temp_read.inodes[new_inode_index].i_size = 0;
                temp_read.inodes[new_inode_index].i_is_directory = 0;
            }

            int block_id = -1;
            
            for (int j=0; j<SUPERBLOCK.superblock.s_blocks_count; j++)
            {
                if (BLOCK_BITMAP.bitmap[j]==0)
                {
                    BLOCK_BITMAP.bitmap[j] = 1;
                    block_id = j;
                    break;
                }
            }

            if (disk_write(1, &BLOCK_BITMAP)==-1)
            {
                printf("Could not write new bitmap to disk.\n");
                return -1;
            }

            temp_read.inodes[new_inode_index].i_direct_pointers[0] = block_id;
            for (int b=1; b<INODE_DIRECT_POINTERS; b++)
            {
                temp_read.inodes[new_inode_index].i_direct_pointers[b] = 0;
            }
            temp_read.inodes[new_inode_index].i_single_indirect_pointer = 0;
            temp_read.inodes[new_inode_index].i_double_indirect_pointer = 0;

            if (disk_write(new_inode_block_number, &temp_read)==-1)
            {
                printf("Could not write new inode block back to disk.\n");
                return -1;
            }

            if (i!=token_count-1 || (i==token_count-1 && is_directory))
            {
                union block this_directory;

                for (int i=0; i<DIRECTORY_ENTRIES_PER_BLOCK; i++)
                {
                    memset(&this_directory.directory_block.entries[i], 0, sizeof(struct directory_entry));
                }

                if (disk_write(block_id, &this_directory)==-1)
                {
                    printf("Could not write root's directory to the disk.\n");
                    return -1;
                }
            }

            struct directory_entry direct;

            direct.inode_number = idx;
            
            strncpy(direct.name, tokens[i], DIRECTORY_NAME_SIZE - 1);
            direct.name[DIRECTORY_NAME_SIZE - 1] = '\0';

            if (inode_block.inodes[inode_index].i_direct_pointers[0]!=0)
            {

                int file_block_number = inode_block.inodes[inode_index].i_direct_pointers[0];
                
                union block file_block;

                if (disk_read(file_block_number, &file_block)==-1)
                {
                    printf("Could not read data block to disk.\n");
                    return -1;
                }
                
                for (int k=0; k<DIRECTORY_ENTRIES_PER_BLOCK; k++)
                {
                    if (file_block.directory_block.entries[k].inode_number == 0)
                    {
                        file_block.directory_block.entries[k] = direct;
                        
                        break;
                    }
                }
                if (disk_write(file_block_number, &file_block)==-1)
                {
                    printf("Could not write data block to disk.\n");
                    return -1;
                }
            }
            current_inode = new_current_inode;
        }
        else
        {
            current_inode = inode_next;
            continue;
        }

    }

    for (int i = 0; i < token_count; i++)
    {
        free(tokens[i]);
    }

    return 0;
}

int fs_remove(char *path)
{
    if (MOUNT_FLAG == 0)
    {
        printf("Nothing is mounted.\n");
        return -1;
    }

    char path_copy[strlen(path) + 1];
    strcpy(path_copy, path);

    char *token;
    char *tokens[MAX_TOKENS];
    int token_count = 0;

    token = strtok(path_copy, "/");
    while (token != NULL && token_count < MAX_TOKENS)
    {
        tokens[token_count] = malloc(strlen(token) + 1);
        strcpy(tokens[token_count], token);
        token_count++;
        token = strtok(NULL, "/");
    }

    int current_inode = 0;
    int parent_inode = 0;

    for (int i=0; i<token_count; i++)
    {
        int new_inode_block = current_inode/64 + 3;
        int new_inode = current_inode%64;

        union block inode_block;

        if (disk_read(new_inode_block, &inode_block)==-1)
        {
            printf("Could not read inode block.\n");
            return -1;
        }

        if (inode_block.inodes[new_inode].i_is_directory==1)
        {
            int data_block_number = inode_block.inodes[new_inode].i_direct_pointers[0];

            union block data_block;

            if (disk_read(data_block_number, &data_block)==-1)
            {
                printf("Could not read data block.\n");
                return -1;
            }

            int idx = -1;

            for (int k=0; k<DIRECTORY_ENTRIES_PER_BLOCK; k++)
            {
                if (strcmp(data_block.directory_block.entries[k].name, tokens[i])==0)
                {
                    idx = data_block.directory_block.entries[k].inode_number;
                    break;
                }
            }

            if (idx==-1)
            {
                printf("No such directory.\n");
                return 0;
            }
            parent_inode = current_inode;
            current_inode = idx;
        }
    }
    
    if(helper_func(current_inode)==-1)
    {
        printf("Could not delete directory.\n");
        return -1;
    }


    int inode_block = parent_inode/64 + 3;
    int inode_num = parent_inode%64;

    union block temp_block;

    if (disk_read(inode_block, &temp_block)==-1)
    {
        printf("Could not read inode block.\n");
        return -1;
    }

    int data_block_num = temp_block.inodes[inode_num].i_direct_pointers[0];

    union block data_block;

    if (disk_read(data_block_num, &data_block)==-1)
    {
        printf("Could not read directory block.\n");
        return -1;
    }

    for (int i=0; i<DIRECTORY_ENTRIES_PER_BLOCK; i++)
    {
        if (strcmp(data_block.directory_block.entries[i].name, tokens[token_count-1])==0)
        {
            data_block.directory_block.entries[i].inode_number = 0;
            data_block.directory_block.entries[i].name[0] = '\0';
            break;
        }
    }

    if (disk_write(data_block_num, &data_block)==-1)
    {
        printf("Could not read directory block.\n");
        return -1;
    }

    return 0;
}

int helper_func(int inode)
{
    int inode_block_number = inode/64 +3;
    int inode_num = inode%64;

    union block inode_block;

    if (disk_read(inode_block_number, &inode_block)==-1)
    {
        printf("Could not read inode block.\n");
        return -1;
    }
    if (inode_block.inodes[inode_num].i_is_directory == 1)
    {
        int directory_block_num = inode_block.inodes[inode_num].i_direct_pointers[0];

        union block directory_block;

        if (disk_read(directory_block_num, &directory_block)==-1)
        {
            printf("Could not read directory block.\n");
            return -1;
        }

        for (int i; i<DIRECTORY_ENTRIES_PER_BLOCK; i++)
        {
            if (directory_block.directory_block.entries[i].inode_number!=0)
            {
                int entry_inode_table = directory_block.directory_block.entries[i].inode_number/64 + 3;
                int entry_inode_num = directory_block.directory_block.entries[i].inode_number%64;
                union block temp_inode_block;
                if (disk_read(entry_inode_table, &temp_inode_block)==-1)
                {
                    printf("Could not read inode block.\n");
                    return -1;
                }

                if(helper_func(entry_inode_num)==-1)
                {
                    printf("Could not remove directory.\n");
                    return -1;
                }
            }
        }
    }
    if (inode_block.inodes[inode_num].i_is_directory==1)
    {
        int block_pointed_to = inode_block.inodes[inode_num].i_direct_pointers[0];

        inode_block.inodes[inode_num].i_direct_pointers[0] = 0;

        BLOCK_BITMAP.bitmap[block_pointed_to] = 0;
        INODE_BITMAP.bitmap[inode] = 0;

        union block zero_block = {0};

        if (disk_write(block_pointed_to, &zero_block)==-1)
        {
            printf("Could not write zero block back to disk.\n");
            return -1;
        }

        inode_block.inodes[inode_num] = (struct inode){0};

        if (disk_write(inode_block_number, &inode_block)==-1)
        {
            printf("Could not write block to disk.\n");
            return -1;
        }

        if (disk_write(1, &BLOCK_BITMAP)==-1)
        {
            printf("Could not write Block bitmap to disk.\n");
            return -1;
        }

        if (disk_write(2, &INODE_BITMAP)==-1)
        {
            printf("Could not write Inode.\n");
            return -1;
        }
    }
    else if (inode_block.inodes[inode_num].i_is_directory==0)
    {
        for (int i=0; i<INODE_DIRECT_POINTERS; i++)
        {
            if (inode_block.inodes[inode_num].i_direct_pointers[i] != 0)
            {
                int block_pointed_to = inode_block.inodes[inode_num].i_direct_pointers[i];

                BLOCK_BITMAP.bitmap[block_pointed_to] = 0;

                inode_block.inodes[inode_num].i_direct_pointers[i] = 0;
            }
        }
        if (inode_block.inodes[inode_num].i_single_indirect_pointer != 0)
        {
            int block_pointed_to = inode_block.inodes[inode_num].i_single_indirect_pointer;

            inode_block.inodes[inode_num].i_single_indirect_pointer = 0;

            union block directed_pointers;

            if (disk_read(block_pointed_to, &directed_pointers)==-1)
            {
                printf("Could not read directed table entries.\n");
                return -1;
            }

            for (int j=0; j<INODE_INDIRECT_POINTERS_PER_BLOCK; j++)
            {
                int block_point = directed_pointers.pointers[j];

                BLOCK_BITMAP.bitmap[block_point] = 0;
            }
        }

        inode_block.inodes[inode_num] = (struct inode){0};

        if (disk_write(inode_block_number, &inode_block)==-1)
        {
            printf("Could not write block to disk.\n");
            return -1;
        }

        if (disk_write(1, &BLOCK_BITMAP)==-1)
        {
            printf("Could not write Block bitmap to disk.\n");
            return -1;
        }

        if (disk_write(2, &INODE_BITMAP)==-1)
        {
            printf("Could not write Inode.\n");
            return -1;
        }
    }

    return 0;
}

int fs_read(char *path, void *buf, size_t count, off_t offset)
{
    if (MOUNT_FLAG == 0)
    {
        printf("Nothing is mounted.\n");
        return -1;
    }

    char path_copy[strlen(path) + 1];
    strcpy(path_copy, path);

    char *token;
    char *tokens[MAX_TOKENS];
    int token_count = 0;

    token = strtok(path_copy, "/");
    while (token != NULL && token_count < MAX_TOKENS)
    {
        tokens[token_count] = malloc(strlen(token) + 1);
        strcpy(tokens[token_count], token);
        token_count++;
        token = strtok(NULL, "/");
    }

    int current_inode = 0;

    for (int i=0; i<token_count; i++)
    {
        int new_inode_block = current_inode/64 + 3;
        int new_inode = current_inode%64;

        union block inode_block;

        if (disk_read(new_inode_block, &inode_block)==-1)
        {
            printf("Could not read inode block.\n");
            return -1;
        }

        int data_block_number = inode_block.inodes[new_inode].i_direct_pointers[0];

        union block data_block;

        if (disk_read(data_block_number, &data_block)==-1)
        {
            printf("Could not read data block.\n");
            return -1;
        }

        int idx = -1;

        for (int k=0; k<DIRECTORY_ENTRIES_PER_BLOCK; k++)
        {
            if (strcmp(data_block.directory_block.entries[k].name, tokens[i])==0)
            {
                idx = data_block.directory_block.entries[k].inode_number;
                break;
            }
        }

        if (idx == -1)
        {
            printf("No such directory.\n");
            return -1;
        }

        current_inode = idx;
    }

    int inode_block_number = current_inode/64 + 3;
    int inode_number = current_inode%64;

    union block inode_block;

    if (disk_read(inode_block_number, &inode_block)==-1)
    {
        printf("Could not read data block from disk.\n");
        return -1;
    }

    int start_block = offset/BLOCK_SIZE;
    
    int start_index = offset%BLOCK_SIZE;
    int initial = start_index;

    int data_block = -1;
    int bytes = 0;

    int j = 0;

    while (count>0)
    {
        if (start_block<INODE_DIRECT_POINTERS)
        {
            if (inode_block.inodes[inode_number].i_direct_pointers[start_block]==0)
            {
                printf("No data on this offset.\n");
                return bytes;
            }
            else
            {
                data_block = inode_block.inodes[inode_number].i_direct_pointers[start_block];
            }
        }
        else if (start_block>=INODE_DIRECT_POINTERS)
        {
            if (inode_block.inodes[inode_number].i_single_indirect_pointer==0)
            {
                printf("No data on this offset.\n");
                return bytes;
            }
            else
            {
                int num = inode_block.inodes[inode_number].i_single_indirect_pointer;
                union block read_block;
                if (disk_read(num, &read_block)==-1)
                {
                    printf("Can not read block from disk.\n");
                    return -1;
                }
                if (read_block.pointers[start_block]==0)
                {
                    printf("No data on this offset.\n");
                    return bytes;
                }
                else
                {
                    data_block = read_block.pointers[start_block];
                }
            }
        }

        if (data_block==-1)
        {
            printf("No such file exits.\n");
            return bytes;
        }
        
        union block read_block;
        if (disk_read(data_block, &read_block)==-1)
        {
            printf("Could not read data block from disk.\n");
            return -1;
        }

        int mini = -1;
        if (BLOCK_SIZE<count+start_index)
        {
            mini = BLOCK_SIZE;
        }
        else
        {
            mini = count+start_index;
        }
        for (int i=start_index; i<mini; i++)
        {
            void *temp_buf = buf + j;
            memcpy(temp_buf, read_block.data + i, 1);
            count--;
            start_index++;
            j++;
            bytes++;
        }

        start_block++;
        start_index = 0;
    }

    return bytes;
}

int fs_write(char *path, void *buf, size_t count, off_t offset)
{
    if (MOUNT_FLAG == 0)
    {
        printf("Nothing is mounted.\n");
        return -1;
    }

    char path_copy[strlen(path) + 1];
    strcpy(path_copy, path);

    char *token;
    char *tokens[MAX_TOKENS];
    int token_count = 0;

    token = strtok(path_copy, "/");
    while (token != NULL && token_count < MAX_TOKENS)
    {
        tokens[token_count] = malloc(strlen(token) + 1);
        strcpy(tokens[token_count], token);
        token_count++;
        token = strtok(NULL, "/");
    }

    int current_inode = 0;
    int size = 0;
    int bytes = 0;

    for (int i=0; i<token_count; i++)
    {
        int new_inode_block = current_inode/INODES_PER_BLOCK + 3;
        int new_inode = current_inode%INODES_PER_BLOCK;

        union block inode_block;

        if (disk_read(new_inode_block, &inode_block)==-1)
        {
            printf("Could not read inode block.\n");
            return -1;
        }
        if (inode_block.inodes[new_inode].i_is_directory==1)
        {
            int data_block_number = inode_block.inodes[new_inode].i_direct_pointers[0];

            union block data_block;

            if (disk_read(data_block_number, &data_block)==-1)
            {
                printf("Could not read data block.\n");
                return -1;
            }

            int idx = -1;

            for (int k=0; k<DIRECTORY_ENTRIES_PER_BLOCK; k++)
            {
                if (strcmp(data_block.directory_block.entries[k].name, tokens[i])==0)
                {
                    idx = data_block.directory_block.entries[k].inode_number;
                    break;
                }
            }

            if (idx==-1)
            {
                printf("No such directory, making directory.\n");
                fs_create(path, 0);
                fs_write(path, buf, count, offset);
                return 0;
            }

            current_inode = idx;
        }
    }

    int inode_block_number = current_inode/64 + 3;
    int inode_number = current_inode%64;

    union block inode_block;

    if (disk_read(inode_block_number, &inode_block)==-1)
    {
        printf("Could not read data block from disk.\n");
        return -1;
    }

    int start_block = offset/BLOCK_SIZE;
    int start_index = offset%BLOCK_SIZE;
    int initial_block_size = start_index;

    int data_block = -1;

    int j = 0;
    while (count>0)
    {
        if (start_block<INODE_DIRECT_POINTERS)
        {
            if (inode_block.inodes[inode_number].i_direct_pointers[start_block]==0)
            {
                int block_bit = -1;
                for (int i=0; i<SUPERBLOCK.superblock.s_blocks_count; i++)
                {
                    if (BLOCK_BITMAP.bitmap[i]==0)
                    {
                        BLOCK_BITMAP.bitmap[i] = 1;
                        inode_block.inodes[inode_number].i_direct_pointers[start_block] = i;
                        block_bit = i;
                        break;
                    }
                }
                if (block_bit==-1)
                {
                    printf("No blocks free.\n");
                    return -1;
                }
                data_block = block_bit;

            }
            else
            {
                data_block = inode_block.inodes[inode_number].i_direct_pointers[start_block];
            }
        }
        else if (start_block>=INODE_DIRECT_POINTERS)
        {
            if (inode_block.inodes[inode_number].i_single_indirect_pointer==0)
            {
                int block_bit = -1;
                for (int i=0; i<SUPERBLOCK.superblock.s_blocks_count; i++)
                {
                    if (BLOCK_BITMAP.bitmap[i]==0)
                    {
                        BLOCK_BITMAP.bitmap[i] = 1;
                        inode_block.inodes[inode_number].i_single_indirect_pointer = i;
                        block_bit = i;
                        
                        union block empty;

                        for (int j=0; j<INODE_DIRECT_POINTERS; j++)
                        {
                            empty.pointers[j] = 0;
                        }

                        if (disk_write(block_bit, &empty)==-1)
                        {
                            printf("Could not write.\n");
                            return -1;
                        }

                        break;
                    }
                }
                if (block_bit==-1)
                {
                    printf("No blocks free.\n");
                    return -1;
                }
            }

            int data_block_number = inode_block.inodes[inode_number].i_single_indirect_pointer;

            union block direct_pointers;

            if (disk_read(data_block_number, &direct_pointers)==-1)
            {
                printf("Unsuccessful read.\n");
                return -1;
            }

            if (direct_pointers.pointers[start_block]==0 || direct_pointers.pointers[start_block]>disk_size() || direct_pointers.pointers[start_block]<0)
            {
                int block_bit = -1;
                for (int i=0; i<SUPERBLOCK.superblock.s_blocks_count; i++)
                {
                    if (BLOCK_BITMAP.bitmap[i]==0)
                    {
                        BLOCK_BITMAP.bitmap[i] = 1;
                        direct_pointers.pointers[start_block] = i;
                        block_bit = i;
                        break;
                    }
                }
                if (block_bit==-1)
                {
                    printf("No blocks free.\n");
                    return -1;
                }
                data_block = block_bit;
            }
            else
            {
                data_block = direct_pointers.pointers[start_block];
            }
        }
        if (data_block == -1)
        {
            printf("No block found.\n");
            return -1;
        }

        union block write_block;

        if (disk_read(data_block, &write_block)==-1)
        {
            printf("Could not read block.\n");
            return -1;
        }

        int mini = -1;
        if (BLOCK_SIZE<count+start_index)
        {
            mini = BLOCK_SIZE;
        }
        else
        {
            mini = count+start_index;
        }

        size = 0;
        for (int i=start_index; i<mini; i++)
        {
            memcpy(write_block.data+i, buf+j, 1);
            count--;
            start_index++;
            j++;
            size++;
            bytes++;
        }
        
        if (disk_write(data_block, &write_block)==-1)
        {
            printf("Could not write to the disk.\n");
            return -1;
        }
        start_block++;

        start_index = 0;
    }

    int last_written_to = start_block-1;
    int size_of_file = (last_written_to*BLOCK_SIZE)+(size+initial_block_size);
    // printf ("The new size of the file will be: %d\n", size_of_file);

    int previous_size = inode_block.inodes[inode_number].i_size;

    if (size_of_file>previous_size)
    {
        inode_block.inodes[inode_number].i_size = size_of_file;
    }
    else
    {
        inode_block.inodes[inode_number].i_size = previous_size;
    }

    // printf ("The new size of the file will be: %d\n", inode_block.inodes[inode_number].i_size);
    

    if (disk_write(1, &BLOCK_BITMAP)==-1)
    {
        printf("Could not write back to disk.\n");
        return -1;
    }
    if (disk_write(inode_block_number, &inode_block)==-1)
    {
        printf("Could not write inode block to disk.\n");
        return -1;
    }
    
    return bytes;
}

int fs_list(char *path)
{
    if (MOUNT_FLAG == 0)
    {
        printf("Nothing is mounted.\n");
        return -1;
    }

    char path_copy[strlen(path) + 1];
    strcpy(path_copy, path);

    char *token;
    char *tokens[MAX_TOKENS];
    int token_count = 0;

    token = strtok(path_copy, "/");
    while (token != NULL && token_count < MAX_TOKENS)
    {
        tokens[token_count] = malloc(strlen(token) + 1);
        strcpy(tokens[token_count], token);
        token_count++;
        token = strtok(NULL, "/");
    }

    int current_inode = 0;

    if (token_count==0)
    {
        int directed_block = SUPERBLOCK.superblock.s_data_blocks_start;
        
        union block first;

        if (disk_read(directed_block, &first)==-1)
        {
            printf("Error, could not read block.\n");
            return -1;
        }

        for (int k=0; k<DIRECTORY_ENTRIES_PER_BLOCK; k++)
        {
            if (first.directory_block.entries[k].inode_number!=0)
            {
                int inode_block_num = (first.directory_block.entries[k].inode_number)/64 + 3;

                int inode_num = first.directory_block.entries[k].inode_number%64;

                union block temp_block;
                
                if (disk_read(inode_block_num, &temp_block)==-1)
                {
                    printf("Could not read inode block.\n");
                    return -1;
                }

                printf("<%s> <%d>\n", first.directory_block.entries[k].name, temp_block.inodes[inode_num].i_size);

            }
        }

        return 0;
    }

    current_inode = 0;

    for (int i=0; i<token_count; i++)
    {
        int new_inode_block = current_inode/64 + 3;
        int new_inode = current_inode%64;

        union block inode_block;

        if (disk_read(new_inode_block, &inode_block)==-1)
        {
            printf("Could not read inode block.\n");
            return -1;
        }

        int data_block_number = inode_block.inodes[new_inode].i_direct_pointers[0];

        union block data_block;

        if (disk_read(data_block_number, &data_block)==-1)
        {
            printf("Could not read data block.\n");
            return -1;
        }

        int idx = -1;

        for (int k=0; k<DIRECTORY_ENTRIES_PER_BLOCK; k++)
        {
            if (strcmp(data_block.directory_block.entries[k].name, tokens[i])==0)
            {
                idx = data_block.directory_block.entries[k].inode_number;
                break;
            }
        }

        current_inode = idx;
    }

    int new_inode = current_inode%64;
    int new_inode_block = current_inode/64 + 3;

    union block inode_data;

    if (disk_read(new_inode_block, &inode_data)==-1)
    {
        printf("Could not read data block.\n");
        return -1;
    }

    if (inode_data.inodes[new_inode].i_is_directory == 0)
    {
        printf("Not a directory.\n");
        return -1;
    }

    int data_block_num = inode_data.inodes[new_inode].i_direct_pointers[0];

    union block data_block;
    
    if (disk_read(data_block_num, &data_block)==-1)
    {
        printf("Could not read data block.\n");
        return -1;
    }

    for (int i=0; i<DIRECTORY_ENTRIES_PER_BLOCK; i++)
    {
        if (data_block.directory_block.entries[i].inode_number!=0)
        {
            union block temp;

            int block_num = data_block.directory_block.entries[i].inode_number/64 + 3;
            int inode_num = data_block.directory_block.entries[i].inode_number%64;

            if (disk_read(block_num, &temp)==-1)
            {
                printf("Could not read block.\n");
                return -1;
            }

            printf("<%s> <%d>\n", data_block.directory_block.entries[i].name, temp.inodes[inode_num].i_size);
        }
    }

    return 0;
}

void fs_stat()
{
    if (MOUNT_FLAG == 0)
    {
        printf("Error: Disk is not mounted.\n");
        return;
    }

    printf("Superblock:\n");
    printf("    Blocks: %d\n", SUPERBLOCK.superblock.s_blocks_count);
    printf("    Inodes: %d\n", SUPERBLOCK.superblock.s_inodes_count);
    printf("test block, block bitmap: %d\n", SUPERBLOCK.superblock.s_block_bitmap);
    printf("test block, inode bitmap: %d\n", SUPERBLOCK.superblock.s_inode_bitmap);

    printf("    Inode Table Block Start: %d\n", SUPERBLOCK.superblock.s_inode_table_block_start);
    printf("    Data Blocks Start: %d\n", SUPERBLOCK.superblock.s_data_blocks_start);
}

void fs_unmount()
{
    if (MOUNT_FLAG == 0)
    {
        printf("Error: Disk is not mounted.\n");
        return;
    }
    // Set the mount flag to 0
    MOUNT_FLAG = 0;
}