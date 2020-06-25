//
// Written by Dylan Carr April 2020
//

#include "networks.h"
#include "gethostbyname6.h"
#include "packets.h"

/* returns the new socket number on success */
int safeSocket() {
   int socket_num;
   if((socket_num = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
      perror("socket call");
      exit(EXIT_FAILURE);
   }
   return socket_num;
}

/* Fills the chat Header structure with the fields from buf */
void getChatHeader(struct chatHeader *chatHdr, uint8_t buf[MAXBUF]) {
   //fprintf(stderr, "getChatHeader() pkt_len (N): %u\n", (uint16_t *)buf);
   uint16_t pkt_len;
   memcpy(&pkt_len, buf, PKT_LEN);
   //printf("pkt_len (N): %u\npkt_len (H): %u\n", pkt_len, ntohs(pkt_len));
   memcpy(&(chatHdr->pkt_len), &pkt_len, PKT_LEN);
   //chatHdr->pkt_len = ntohs(chatHdr->pkt_len);
   // why does memcpy flip byte orders?!?!
   memcpy(&(chatHdr->flag), buf+PKT_LEN, FLAG_LEN);
}

/* puts the packet starting at the flag into the buf
 * returns 0 on 0 bytes read (client or server sent 0 byte pkt)
 * returns pkt_len (Host Order) on success
 * Safe recv
 */
int sRecv(uint8_t buf[MAXBUF], int socketNum) {

   /* entire packet length including chat header + null term (if exists) */
   /* should be in host order */
   uint16_t pkt_len = getPktLen(socketNum);
   int messageLen = 0;

   // means 0 byte packet sent
   if(pkt_len == 0)
      return -1;

   /* reads the rest of the packet into the buf (offset by 2 from pkt_len) */
   if ((messageLen = recv(socketNum, buf, pkt_len-PKT_LEN, 0)) < 0) {
      perror("recv call in sRecv()");
      exit(EXIT_FAILURE);
   }
   return pkt_len;
}

/* Gets the packet length from the user level chat header */
/* so you can recv the exact amount of bytes */
/* helper function for sRecv() */
uint16_t getPktLen(int socketNum) {
   uint16_t pkt_len = 0;
   int messageLen = 0; // should be 2 unless client/server ctrl+C (0 bytes)

   // read first 2 bytes - packet length
   if ((messageLen = recv(socketNum, &pkt_len, PKT_LEN, 0)) < 0) {
      perror("recv call in getPktLen()");
      exit(EXIT_FAILURE);
   }
   return ntohs(pkt_len);
}

/* Sends the packet pointed to at buf as is. len = # bytes */
void sendPacket(int socketNum, uint8_t buf[MAXBUF], uint16_t len) {
	int sent = 0;
	if((sent = send(socketNum, buf, len, 0) < 0)) {
		perror("sending packet call\n");
		exit(EXIT_FAILURE);
	}
}

/* puts into buf the flag and pkt_len (in bytes) in network order. */
void makeChatHeader(uint8_t buf[MAXBUF], uint8_t flag, uint16_t pkt_len) {
   pkt_len = htons(pkt_len);
   memcpy(buf, &pkt_len, PKT_LEN);
   memcpy(buf+PKT_LEN, &flag, FLAG_LEN);
}
