#include <stdio.h>
#include <stdlib.h>

#include <file_helper.h>

long int get_file_size(char *file_name)
{
    FILE * p_fw_file = NULL;
    long int lsize = 0;
    p_fw_file = fopen(file_name, "rb");
    if(p_fw_file == NULL)
    {
        printf("Could not open fw file!\n");
        exit(-1);
    }    

    fseek(p_fw_file, 0, SEEK_END);
    lsize = ftell(p_fw_file);

    fclose(p_fw_file);

    return lsize;
}

long int write_mem_from_file(char *file_name, uint8_t *memory, long int mem_size)
{
    FILE * p_fw_file = NULL;
    long int lsize = 0;
    long int result = 0;

    lsize = get_file_size(file_name);

    if(lsize > mem_size)
    {
        printf("Not able to load fw file of size %lu, mem space is %lx\n", lsize, mem_size);
        exit(-1);
    }

    p_fw_file = fopen(file_name, "rb");
    if(p_fw_file == NULL)
    {
        printf("Could not open fw file!\n");
        exit(-2);
    }

    result = fread(memory, sizeof(uint8_t), lsize, p_fw_file);
    if(result != lsize)
    {
        printf("Error while reading file!\n");
        exit(-3);
    }

    fclose(p_fw_file);

    return lsize;
}
