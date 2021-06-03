#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <iconv.h>
#include <errno.h>
#include <regex.h>

int search_record(char *ip);
int binary_search(int low, int high, int ip);
int readbyte(int size, int offset, int *buff);
int readvalue(unsigned int size, int *buff);
void qqwry_seek(int offset);
void qqwry_forward(unsigned int byte);
void qqwry_back(unsigned int byte);
static char *get_string();
int getDetail(char *ip);
int gbk2utf8(char *utf8_str, char *gbk_str);

typedef struct
{
    FILE *fp;
    unsigned int index_size;
    unsigned int first_item, last_item;
    unsigned int item_number, startip, endip, curr_data_offset;
    char *parent_data, *child_data;
    int isp;
} ip_data;
