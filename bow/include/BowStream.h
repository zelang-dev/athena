/*
 * BowStream.h
 */
#ifndef __BOWSTREAM_H__
#define __BOWSTREAM_H__ 1

typedef struct BowStreamP    *BowStream;

/*
 * io.c
 */
typedef void (*BowStreamCallback)(BowStream, ssize_t, void *);

BowStream StreamCreateINet(BowResources, char *, int);
BowStream StreamCreateINet2(BowResources);
BowStream StreamCreateFD(BowResources, int);
void StreamDestroy(BowStream);
void StreamWrite(BowStream, byte *, size_t, BowStreamCallback, void *);
void StreamRead(BowStream, byte *, size_t, BowStreamCallback, void *);
void StreamRead(BowStream, byte *, size_t, BowStreamCallback, void *);
int StreamGetINetPort(BowStream);
unsigned long StreamGetINetAddr(BowStream);

#endif
