#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define INMUTILN 4136
#define INMSIZE 4140
#define INMDSORG 60
#define INMLRECL 66
#define INMBLKSZ 48
#define INMRECFM 73
#define INMDSNAM 2
#define INMDIR 12
#define INMTYPE 32786
static const unsigned char e2a[256] = {
          0,  1,  2,  3,156,  9,134,127,151,141,142, 11, 12, 13, 14, 15,
         16, 17, 18, 19,157,133,  8,135, 24, 25,146,143, 28, 29, 30, 31,
        128,129,130,131,132, 10, 23, 27,136,137,138,139,140,  5,  6,  7,
        144,145, 22,147,148,149,150,  4,152,153,154,155, 20, 21,158, 26,
         32,160,161,162,163,164,165,166,167,168, 91, 46, 60, 40, 43, 33,
         38,169,170,171,172,173,174,175,176,177, 93, 36, 42, 41, 59, 94,
         45, 47,178,179,180,181,182,183,184,185,124, 44, 37, 95, 62, 63,
        186,187,188,189,190,191,192,193,194, 96, 58, 35, 64, 39, 61, 34,
        195, 97, 98, 99,100,101,102,103,104,105,196,197,198,199,200,201,
        202,106,107,108,109,110,111,112,113,114,203,204,205,206,207,208,
        209,126,115,116,117,118,119,120,121,122,210,211,212,213,214,215,
        216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,
        123, 65, 66, 67, 68, 69, 70, 71, 72, 73,232,233,234,235,236,237,
        125, 74, 75, 76, 77, 78, 79, 80, 81, 82,238,239,240,241,242,243,
         92,159, 83, 84, 85, 86, 87, 88, 89, 90,244,245,246,247,248,249,
         48, 49, 50, 51, 52, 53, 54, 55, 56, 57,250,251,252,253,254,255
};

void ebcdicToAscii (unsigned char *s)
{
    while (*s)
    {
        *s = e2a[(int) (*s)];
        s++;
    }
}

unsigned int intcpy (unsigned char *value_bin, unsigned int length)
{
    unsigned int i, value;
    unsigned char value_char[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    i = 0;
    while (i < length) {
        value_char[i] = *(value_bin + length - (i + 1));
        i++;
    }
    memcpy ((unsigned char *) &value, value_char, 4);
    return value;
}

int
main(argc, argv)
int     argc;
char    *argv[];
{
    FILE          *infile;
    unsigned char  line[1000];
    unsigned char  INMR0[6] = {0xE0,0xC9,0xD5,0xD4,0xD9,0xF0};
    unsigned char  INMR01[7] = {0xE0,0xC9,0xD5,0xD4,0xD9,0xF0,0xF1};
    unsigned char  INMR02[7] = {0xE0,0xC9,0xD5,0xD4,0xD9,0xF0,0xF2};
    unsigned char  INMR03[7] = {0xE0,0xC9,0xD5,0xD4,0xD9,0xF0,0xF3};
    unsigned char  name_token[9];
    unsigned int   len, i, j, k, nextkey, keylen, value, len_value_pairs, first_token;
    unsigned short key;
    ++argv;
    infile = fopen(*argv,"r");
    len = fread(line,1,1000,infile);
    fclose(infile);
    len = len - 9;
    if (memcmp(line + 1, INMR01, 7)) goto native;
    i = line[0];
    if (i > len) goto native;
    if (memcmp(line + i + 1, INMR02, 7)) goto native;
    j = i + 12;
    i = i + line[i];
    if (i > len) goto native;
    if (memcmp(line + i + 1, INMR0, 6)) goto native;
    printf("FORMAT=NETDATA\n");
    while (j < i) {
        keylen = 0;
        len_value_pairs = 0;
        key = (short) intcpy (&line[j], 2);
        keylen = intcpy (&line[j+4], 2);
        len_value_pairs = intcpy (&line[j+2], 2);
        k = j + 6;
        nextkey = j + 6 + keylen;
        switch (key) {
            case INMUTILN:
                memcpy(name_token, &line[k], keylen);
                name_token[keylen] = 0;
                ebcdicToAscii(name_token);
                printf("UTILN=%s\n", name_token);
                break;
            case INMSIZE:
                value = intcpy (&line[k], keylen);
                printf("SIZE=%d\n", value);
                break;
            case INMDSORG:
                printf("DSORG=");
                if (!memcmp(&line[k], "\x00\x08", 2)) printf("VS\n");
                    else if (!memcmp(&line[k], "\x02\x00", 2)) printf("PO\n");
                        else if (!memcmp(&line[k], "\x40\x00", 2)) printf("PS\n");
                            else printf("??\n");
                break;
            case INMLRECL:
                value = intcpy (&line[k], keylen);
                printf("LRECL=%d\n", value);
                break;
            case INMBLKSZ:
                value = intcpy (&line[k], keylen);
                printf("BLKSIZE=%d\n", value);
                break;
            case INMDIR:
                value = intcpy (&line[k], keylen);
                printf("DIRBLKS=%d\n", value);
                break;
            case INMRECFM:
                printf("RECFM=");
                if ((line[k] & 0x80) && (line[k] & 0x40)) printf("U");
                    else if (line[k] & 0x80) printf("F");
                        else if (line[k] & 0x40) printf("V");
                            else printf("?");
                if (line[k] & 0x10) printf("B");
                if (line[k] & 0x08) printf("S");
                    else if (line[k] & 0x04) printf("A");
                        else if (line[k] & 0x02) printf("M");
                printf("\n");
                break;
            case INMTYPE:
                if (line[k] == 0) break;
                printf("DSNTYPE=");
                if (line[k] & 0x80) printf("LIBRARY\n");
                    else if (line[k] & 0x40) printf("LIBRARY\n");
                        else if (line[k] & 0x04) printf("EXTREQ\n");
                            else if (line[k] & 0x01) printf("LARGE\n");
                                else printf("UNKNOWN\n");
                break;
            case INMDSNAM:
                printf("DSN=");
                first_token = 1;
                while (len_value_pairs >= 1) {
                    memcpy(name_token, &line[k], keylen);
                    name_token[keylen] = 0;
                    ebcdicToAscii(name_token);
                    if (!first_token) printf(".");
                    first_token = 0;
                    printf("%s", name_token);
                    keylen = 0;
                    keylen = intcpy (&line[nextkey], 2);
                    k = nextkey + 2;
                    nextkey = k + keylen;
                    len_value_pairs--;
                }
                printf("\n");
                break;
            default:
                printf("UNKNOWN=%x\n", key);
        }
        j = nextkey;
    }
    exit(0);
    native: printf("FORMAT=NATIVE\n");
    exit(-1);
}
