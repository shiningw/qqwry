#include <unistd.h>
#include <stdint.h>
#include "qqwry.h"

ip_data ip_defaults = {.parent_data = NULL, .child_data = NULL, .index_size = 7, .isp = 1};

int qqwry_init(char *file)
{
    ip_defaults.fp = fopen(file, "r");
    if (ip_defaults.fp == NULL)
    {
        fprintf(stderr, "failed to open %s\n", file);
        return -1;
    }
    int count, buff;
    readvalue(4, &buff); //first 4 bytes represents the offset of first index
    ip_defaults.first_item = buff;
    readvalue(4, &buff);
    ip_defaults.last_item = buff;
    ip_defaults.item_number = (ip_defaults.last_item - ip_defaults.first_item) / ip_defaults.index_size;
}

int qqwry_match(char *pattern, char *subject)
{
    regex_t regex;
    int reti, ret;
    char msgbuf[100];

    /* Compile regular expression */
    reti = regcomp(&regex, pattern, 0);
    if (reti)
    {
        fprintf(stderr, "Could not compile regex\n");
        return 0;
    }

    /* Execute regular expression */
    reti = regexec(&regex, subject, 0, NULL, 0);
    if (!reti)
    {
        ret = 1;
    }
    else if (reti == REG_NOMATCH)
    {
        ret = 0;
    }
    else
    {
        regerror(reti, &regex, msgbuf, sizeof(msgbuf));
        fprintf(stderr, "Regex match failed: %s\n", msgbuf);
        ret = 0;
    }

    /* Free memory allocated to the pattern buffer by regcomp() */
    regfree(&regex);
    return ret;
}

iconv_t initialize_iconv(const char *target, const char *src)
{
    iconv_t iconvDesc = iconv_open(target, src);

    if (iconvDesc == (iconv_t)-1)
    {
        fprintf(stderr, "conversion from '%s' to '%s' not available", src, target);
        return NULL;
    }
}

int gbk2utf8(char *utf8_str, char *gbk_str)
{
    iconv_t iconvDesc = initialize_iconv("UTF-8//TRANSLIT//IGNORE", "GBK");
    size_t iconv_value, len, utf8len;
    //int len_start;

    len = strlen(gbk_str) + 1;
    if (!len)
    {
        fprintf(stderr, "iconvISO2UTF8: input String is empty.");
        return -1;
    }

    /* Assign enough space to put the UTF-8. */
    utf8len = 3 * len;
    if (!utf8_str)
    {
        fprintf(stderr, "iconvISO2UTF8: Calloc failed.");
        return -1;
    }

    iconv_value = iconv(iconvDesc, &gbk_str, &len, &utf8_str, &utf8len);
    /* Handle failures. */
    if (iconv_value == (size_t)-1)
    {
        switch (errno)
        {
            /* See "man 3 iconv" for an explanation. */
        case EILSEQ:
            fprintf(stderr, "iconv failed: Invalid multibyte sequence, in string '%s', length %d, out string '%s', length %d\n", gbk_str, (int)len, utf8_str, (int)utf8len);
            break;
        case EINVAL:
            fprintf(stderr, "iconv failed: Incomplete multibyte sequence, in string '%s', length %d, out string '%s', length %d\n", gbk_str, (int)len, utf8_str, (int)utf8len);
            break;
        case E2BIG:
            fprintf(stderr, "iconv failed: No more room, in string '%s', length %d, out string '%s', length %d\n", gbk_str, (int)len, utf8_str, (int)utf8len);
            break;
        default:
            fprintf(stderr, "iconv failed, in string '%s', length %d, out string '%s', length %d\n", gbk_str, (int)len, utf8_str, (int)utf8len);
        }
        return -1;
    }

    if (iconv_close(iconvDesc) != 0)
    {
        fprintf(stderr, "libicon close failed: %s", strerror(errno));
        return -1;
    }

    return utf8len;
}

int readbyte(int size, int offset, int *buff)
{
    int count;
    int nbytes = 1;
    *buff = 0;
    if (ip_defaults.fp != NULL)
    {
        //if offset is negative,keep the current offset unchanged
        if (offset >= 0)
        {
            qqwry_seek(offset);
        }
        else
        {
            int curr_pos = ftell(ip_defaults.fp);
            fseek(ip_defaults.fp, curr_pos, SEEK_SET);
        }

        if ((count = fread(buff, nbytes, size, ip_defaults.fp)) != size)
        {
            return -1;
        }
        return count;
    }
    return -1;
}

int readvalue(unsigned int size, int *buff)
{
    return readbyte(size, -1, buff);
}

void set_ip_range(unsigned int offset)
{
    readbyte(4, offset, &ip_defaults.startip);
    //skip 3 bytes to read the next ip
    qqwry_forward(3);
    readvalue(4, &ip_defaults.endip);
}

void qqwry_seek(int offset)
{
    fseek(ip_defaults.fp, offset, SEEK_SET);
}

void qqwry_forward(unsigned int byte)
{
    fseek(ip_defaults.fp, byte, SEEK_CUR);
}

void qqwry_back(unsigned int byte)
{
    int currPos = ftell(ip_defaults.fp);
    qqwry_seek(currPos - byte);
}

char *long2ip(int ip)
{
    int size = 16 * sizeof(char);
    char *ip_str = malloc(size);
    snprintf(ip_str, size, "%d.%d.%d.%d", ip >> 24, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
    return ip_str;
}

unsigned int ip2long(char *ip)
{
    int nip = 0, tmp = 0, step = 24;
    char *copy = strdup(ip);
    char *token = strtok(copy, ".");

    while (token)
    {
        tmp = (unsigned int) atoi(token);
        tmp <<= step;
        nip += tmp;
        step -= 8;
        token = strtok(NULL, ".");
    }
    free(copy);
    return nip;
}

int search_record(char *ip)
{
    int numeric_ip = ip2long(ip);
    int low = 0;
    int high = ip_defaults.item_number;
    return binary_search(low, high, numeric_ip);
}

int binary_search(int low, int high, int ip)
{
    unsigned int mid, offset, range, startip, endip;

    if (low <= high)
    {
        mid = low + (high - low) / 2;
        offset = round(ip_defaults.first_item + mid * ip_defaults.index_size);
        set_ip_range(offset);
        startip = ip_defaults.startip;
        endip = ip_defaults.endip;
        if (ip >= startip && ip <= endip)
        {
            return offset;
        }
        //if ip is below the lower limit, decrease the upper limit
        if (ip < startip)
        {
            return binary_search(low, mid - 1, ip);
        }
        //if ip is above the lower limit, increase the lower limit
        return binary_search(mid + 1, high, ip);
    }
    return ip_defaults.last_item;
}

static char *get_string()
{
    unsigned int buff = 0;
    char *str = realloc(NULL, sizeof(char));
    char *tmp;
    int i =0 ,c = 0;

    if((c = readvalue(1, &buff)) != 1){
        return NULL;
    }

    for (i = 0; buff !=0; i++)
    {
        str[i] = buff;
        tmp = realloc(str, (sizeof(char)) * (i + 2));
        str = tmp;
        readvalue(1, &buff);
    }
    str[i] = '\0';
    return str;
}

static char *get_child_data()
{
    unsigned int flag, offset;
    readvalue(1, &flag);
    if (flag == 0)
    { //no child data
        return 0;
    }
    else if (flag == 1 || flag == 2)
    { // redirection for child data
        readvalue(3, &offset);
        qqwry_seek(offset);
        return get_string();
    }
    // no redirection for child data
    qqwry_back(1);
    return get_string();
}

int convert_data(char *parent_data, char *child_data)
{
    ip_defaults.parent_data = malloc(strlen(parent_data) * 3); //in utf8,one chinese character could consume up to 3 bytes
    gbk2utf8(ip_defaults.parent_data, parent_data);
    ip_defaults.child_data = malloc(strlen(child_data) * 3);
    gbk2utf8(ip_defaults.child_data, child_data);

    if (qqwry_match("移动", ip_defaults.child_data))
    {
        ip_defaults.isp = 0x03;
    }
    else if (qqwry_match("联通", ip_defaults.child_data))
    {
        ip_defaults.isp = 0x02;
    }
    else
    {
        ip_defaults.isp = 0x01;
    }
    free(parent_data);
    free(child_data);
}

int qqwry_redirect(int bytes)
{
    int redirect_offset;
    readvalue(3, &redirect_offset);
    qqwry_seek(redirect_offset);
    return redirect_offset;
}

int get_data(int offset)
{ //get record data
    int flag, tmp_offset, redirect_offset;
    char *parent_data, *child_data;
    readbyte(1, offset + 4, &flag); //get the flag value to see if the data is stored elsewhere

    if (flag == 1)
    {                                        //this means we should look elsewhere for both
        redirect_offset = qqwry_redirect(3); //read 3 bytes to get a new offset and redirect there
        readvalue(1, &flag);
        if (flag == 2)
        {
            // child data is elsewhere
            qqwry_redirect(3);
            parent_data = get_string();
            qqwry_seek(redirect_offset + 4);
            child_data = get_child_data();
        }
        else
        { // no redirection for parent data
            qqwry_back(1);
            parent_data = get_string();
            child_data = get_child_data();
        }
    }
    else if (flag == 2)
    { //redirection for only parent
        qqwry_redirect(3);
        parent_data = get_string();
        qqwry_seek(offset + 8);
        child_data = get_child_data();
    }
    else
    { // no redirection for both parent and child
        qqwry_back(1);
        parent_data = get_string();
        child_data = get_string();
    }

    convert_data(parent_data, child_data);

    return 0;
}

int get_location(char *ip)
{
    //offset is the address where the ip is found. first 4 bytes is the start ip address of the ip range and the following 3 bytes is the offset pointing to the actual record data;
    unsigned int offset = search_record(ip);
    unsigned int tmp_offset;
    qqwry_seek(offset + 4);    // skip 4 byte to get the offset value pointing to record data
    readvalue(3, &tmp_offset); // the offset pointing to the data
    get_data(tmp_offset);
}

int main(int argc, char **argv)
{
    char ip[16];
    qqwry_init("qqwry.dat");
    if (argv[1])
    {
        strncpy(ip, argv[1], 16);
    }
    else
    {
        fprintf(stderr, "missing ip parameter\n");
        return -1;
    }

    get_location(ip);
    printf("%s-%s %d\n", ip_defaults.parent_data, ip_defaults.child_data, ip_defaults.isp);
    free(ip_defaults.parent_data);
    free(ip_defaults.child_data);
    fclose(ip_defaults.fp);
    return 0;
}
