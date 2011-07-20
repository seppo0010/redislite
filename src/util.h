#ifndef _UTIL_H
#define _UTIL_H

// we are only going to support 32 bits right now

int redislitePutVarint32(unsigned char *, int);
int redisliteGetVarint32(const unsigned char *, int *);

#define getVarint32(A,B)  (int)((*(A)<(int)0x80) ? ((B) = (int)*(A)),1 : redisliteGetVarint32((A), (int *)&(B)))
#define putVarint32(A,B)  (int)(((int)(B)<(int)0x80) ? (*(A) = (unsigned char)(B)),1 : redislitePutVarint32((A), (B)))


void redislite_put_4bytes(unsigned char *p, int v);
int redislite_get_4bytes(const unsigned char *p);
void redislite_put_2bytes(unsigned char *p, int v);
int redislite_get_2bytes(const unsigned char *p);

#define MIN(A,B) ((A) > (B) ? (B) : (A))
#define MAX(A,B) ((A) < (B) ? (B) : (A))

int intlen(int integer);
int str_to_long_long(char *str, int len, long long *value);

int stringmatchlen(const char *pattern, int patternLen, const char *string, int stringLen, int nocase);
int stringmatch(const char *pattern, const char *string, int nocase);

#endif
