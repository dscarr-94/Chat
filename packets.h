#ifndef PACKETS_H
#define PACKETS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "networks.h"

#define PKT_LEN 2 // 2 bytes packet length field
#define FLAG_LEN 1
#define MESSAGE_FLAG 5
#define BROADCAST_FLAG 4
#define MAX_DEST_HANDLES 9
#define MAX_MESSAGE 200

/* Fixed size handle */
typedef struct {
   uint8_t handle[MAX_HANDLE+1]; // null term
} __attribute__((packed)) Handle;

struct chatHeader {
	uint16_t pkt_len;
	uint8_t flag;
} __attribute__((packed)) ChatHeader;

void getChatHeader(struct chatHeader *chatHdr, uint8_t buf[MAXBUF]);
int sRecv(uint8_t buf[MAXBUF], int socketNum);
uint16_t getPktLen(int socketNum);
void sendPacket(int socketNum, uint8_t buf[MAXBUF], uint16_t len);
void makeChatHeader(uint8_t buf[MAXBUF], uint8_t flag, uint16_t pkt_len);
int safeSocket();

#endif
