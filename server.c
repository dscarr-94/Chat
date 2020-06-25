/* Written Hugh Smith, Updated: April 2020
 * Use at your own risk.  Feel free to copy, just leave my name in it.
 * Modified by Dylan Carr April 2020
 * dscarr94@gmail.com
 */
#include "networks.h"
#include "pollLib.h"
#include "packets.h"

/* Server scope MACROS */
#define DEBUG_FLAG 1

/* Server status */
#define CLOSED 0
#define OPEN 1

#define INIT_CLIENTS 10
#define GOOD_HANDLE 2
#define HANDLE_EXISTS 3

/* Server scope structures */
typedef struct {
   int num_allocations; // max # allocations for server
   int num_handles; // # of clients in server database
   uint8_t *socket_status; // array of socket_status - malloc
   uint8_t *socket_numbers; // array of socket numbers - malloc/realloc
   Handle *clients; // pointer to array of Handle structs
   //socket_handles; // pointer to array of char pointers - malloc
} __attribute__((packed)) Server;

/* Function prototypes */
void processSockets(int mainServerSocket);
void recvFromClient(int clientSocket, Server *s);
void acceptNewClient(int mainServerSocket);
void removeClient(int clientSocket, Server *s);
int checkArgs(int argc, char *argv[]);
void serverSetup(Server *s);
void ackNewClient(uint8_t *buf, Server *s, int clientSocket);
int lookupClient(Server *s, Handle handle);
void addNewClient(Server *s, uint8_t *handle, uint8_t len, int clientSocket);
void removeClientFromServer(int clientSocket, Server *s);
void clientRequestingHandles(int clientSocket, Server *s);
void sendHandles(int clientSocket, Server *s);
void sendNumHandles(int clientSocket, int num_handles);
void clientExiting(int clientSocket, Server *s);
void forwardMessage(uint8_t buf[MAXBUF], Server *s, uint16_t pkt_len, int clientSocket);
void sendInvalidClient(Handle handle, int clientSocket);
void broadcast(uint8_t *buf, Server *s, uint16_t pkt_len, int clientSocket);

int main(int argc, char *argv[]) {

	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;

	setupPollSet();
	portNumber = checkArgs(argc, argv);

	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);

	// Main control process (clients and accept())
	processSockets(mainServerSocket);

	// close the socket - never gets here but nice thought
	close(mainServerSocket);
	return 0;
}

/* Sets up the server allocating space for the servers database */
void serverSetup(Server *s) {
   int i;
   s->socket_status = malloc(sizeof(uint8_t) * INIT_CLIENTS);
   if(s->socket_status == NULL) {
      perror("malloc socket_status failure");
      exit(EXIT_FAILURE);
   }

   s->socket_numbers = malloc(sizeof(uint8_t) * INIT_CLIENTS);
   if(s->socket_numbers == NULL) {
      perror("malloc socket_numbers failure");
      exit(EXIT_FAILURE);
   }

   // just call calloc before instead?
   for(i = 0; i < INIT_CLIENTS; i++) {
      s->socket_status[i] = CLOSED;
      s->socket_numbers[i] = 0;
   }

   s->clients = malloc(sizeof(Handle) * INIT_CLIENTS);
   if(s->clients == NULL) {
      perror("malloc clients failure");
      exit(EXIT_FAILURE);
   }
   // need to zero or null client handles ?
   s->num_handles = 0;
   s->num_allocations = INIT_CLIENTS;
}

/* Main loop processing packets from clients.
 * Polls on accepting a new client and receiving a packet from an existing client
 */
void processSockets(int mainServerSocket) {

	int socketToProcess = 0;
	addToPollSet(mainServerSocket);
	Server server;
	serverSetup(&server);
   /* Note:
    * poll () actually returns socket number that is Ready
    * no need to check which fd in the fd_set is ready for reading
    * select effectively zeros out all fds in FD_SET except for
    * fds ready for reading. This requires re-adding every fd
    * you want to poll on before every select call
    */
	while(1) {
		if ((socketToProcess = pollCall(POLL_WAIT_FOREVER)) != -1) {
			if (socketToProcess == mainServerSocket)
				acceptNewClient(mainServerSocket);
			else
				recvFromClient(socketToProcess, &server);
		}
		else // Just printing here to let me know what is going on
			printf("Poll timed out waiting for client to send data\n");
	}
}

// all flag packets sent from client processed here
void recvFromClient(int clientSocket, Server *s) {

	uint8_t buf[MAXBUF];
   uint8_t flag = 0;
	int pkt_len = 0;
   // doesn't put pkt_len in buf
	if((pkt_len = sRecv(buf, clientSocket)) < 0) {
		printf("client died\n");
		removeClient(clientSocket, s);
	}

   else { // ready to parse client message!
      memcpy(&flag, buf, 1); // or just flag = buf[0] ?
      // set data to point to first byte after flag
      // no longer need chat header after this point?
      uint8_t *data = buf + 1;
   	// now can switch based on flag
      switch(flag) {
         case 1: // initial packet, f = 2,3 response
            ackNewClient(data, s, clientSocket);
            break;

         case 4:
            broadcast(buf, s, pkt_len, clientSocket);
            break;

         case 5:
            forwardMessage(buf, s, pkt_len, clientSocket); // need whole packet to forward
            break;

         case 8:
            clientExiting(clientSocket, s);
            break;

         case 10:
            clientRequestingHandles(clientSocket, s);
            break;

         default:
            fprintf(stderr, "client sent bad packet (wrong flag): %u\n", flag);
      } // end switch
   } // end else
}

void clientRequestingHandles(int clientSocket, Server *s) {

   sendNumHandles(clientSocket, s->num_handles);
   sendHandles(clientSocket, s);

}

void sendHandles(int clientSocket, Server *s) {

   uint8_t buf[MAXBUF];
   uint16_t pkt_len;
   uint8_t handle_len = 0;
   int i;

   // need num_allocations in case clients were removed
   for(i = 0; i < s->num_allocations; i++) {
      pkt_len = 4; // 4 minimum + handle len
      if(s->socket_status[i] == OPEN) { // valid client
         handle_len = strlen((char *)s->clients[i].handle); // strlen doesnt count \0
         pkt_len += handle_len;
         makeChatHeader(buf, 12, pkt_len);
         memcpy(buf+3, &handle_len, 1);
         memcpy(buf+4, s->clients[i].handle, handle_len); // doesnt copy \0
         sendPacket(clientSocket, buf, pkt_len);
      }
   }
   // finished sending handles - send f = 13
   pkt_len = 3;
   makeChatHeader(buf, 13, pkt_len);
   sendPacket(clientSocket, buf, pkt_len);
}

void sendNumHandles(int clientSocket, int num_handles) {

   uint8_t buf[MAXBUF];
   uint16_t pkt_len = 7; // 3 + 4 byte int
   makeChatHeader(buf, 11, pkt_len);
   num_handles = htonl(num_handles);
   memcpy(buf+3, &num_handles, sizeof(uint32_t)); // 64 vs 32 bit int?
   sendPacket(clientSocket, buf, pkt_len);
}

// send flag = 9 ACK and remove client from server database
void clientExiting(int clientSocket, Server *s) {

   uint8_t buf[MAXBUF];
   uint16_t pkt_len = 3;
   makeChatHeader(buf, 9, pkt_len);
   sendPacket(clientSocket, buf, pkt_len);
   removeClient(clientSocket, s);

}

// fowards messages to the appropriate clients
// buf points to flag (buf offest by 2)
void forwardMessage(uint8_t buf[MAXBUF], Server *s, uint16_t pkt_len, int clientSocket) {

   uint8_t sendbuf[MAXBUF];
   uint16_t pkt_len_NetW;
   int i, offset = 1;
   uint8_t src_handle_len, num_dest_handles;
   Handle handle;

   memcpy(&src_handle_len, buf+offset, 1);
   offset += src_handle_len + 1;
   memcpy(&num_dest_handles, buf+offset, 1);
   offset++; // now points to first dest handle

   int handle_len = 0;
   int socketToSend;

   // for each pair of <handle length, handle>
   for(i = 0; i < num_dest_handles; i++) {
      memcpy(&handle_len, buf+offset, 1);
      offset++;
      memcpy(handle.handle, buf+offset, handle_len * sizeof(uint8_t));
      // append null term to handle  for lookup
      handle.handle[handle_len] = '\0';
      // set offset to next dest_handle, or msg if last dest
      offset += handle_len;

      if((socketToSend = lookupClient(s, handle)) < 0) {
         // handle doesnt exist in server (bad handle)
         // dont foward, send flag = 7 packet
         // printf("client doesn't exist!\n");
         sendInvalidClient(handle, clientSocket);
      }
      else { //valid handle - socketToSend = index of socket
         socketToSend = s->socket_numbers[socketToSend];
         memcpy(sendbuf+2, buf, pkt_len-2);
         pkt_len_NetW = htons(pkt_len);
         memcpy(sendbuf, &pkt_len_NetW, PKT_LEN);
         // now foward packet
         sendPacket(socketToSend, sendbuf, pkt_len);
      }
   }
}

// flag = 7 invalid client
void sendInvalidClient(Handle handle, int clientSocket) {

   uint8_t buf[MAXBUF];
   uint16_t pkt_len = 0;
   uint8_t handle_len;
   handle_len = strlen((char *)handle.handle); // strlen doesnt include \0
   pkt_len = 4 + handle_len;
   makeChatHeader(buf, 7, pkt_len);
   memcpy(buf+3, &handle_len, 1);
   memcpy(buf+4, handle.handle, handle_len * sizeof(uint8_t));
   sendPacket(clientSocket, buf, pkt_len);
}

//buf points to flag
// pkt_len host order
// forwards the message (packet unaltered) to each OPEN client
void broadcast(uint8_t *buf, Server *s, uint16_t pkt_len, int clientSocket) {

   //re-create packet to send to each valid client except clientSocket
   uint16_t pkt_len_NetW = htons(pkt_len);
   uint8_t sendbuf[MAXBUF];
   memcpy(sendbuf+PKT_LEN, buf, pkt_len-2);
   memcpy(sendbuf, &pkt_len_NetW, PKT_LEN);
   //sendbuf ready

   int socketToSend = 0;
   int i;
   for(i = 0; i < s->num_allocations; i++) {
      if(s->socket_status[i] == OPEN) {
         //send this client the message
         socketToSend = s->socket_numbers[i];
         if(socketToSend != clientSocket) // dont send back to sender
            sendPacket(socketToSend, sendbuf, pkt_len);
      }
   }
}

void acceptNewClient(int mainServerSocket) {

	int clientSocket = tcpAccept(mainServerSocket, 0);
	addToPollSet(clientSocket);

}

// Called if flag = 1 packet sent from client
// check if handle exists in server table
// respond with flag 2,3 on success/failure
// buf points to source_handle_len of packet
void ackNewClient(uint8_t *buf, Server *s, int clientSocket) {

   Handle handle;
   uint8_t handle_len = buf[0];
   buf[handle_len+1] = '\0'; // append null terminator to handle
   // handle_len is now 1 less than total size
   handle_len++;
   memcpy(handle.handle, buf+1, handle_len * sizeof(uint8_t));

   uint16_t pkt_len = sizeof(ChatHeader);
   // buf + 1 points to actual handle
   if(lookupClient(s, handle) < 0) {
      //handle not found
      //add client to server
      addNewClient(s, buf+1, handle_len, clientSocket);
      //send flag 2 (success)
      // re-use buf now, dont need it anymore
      makeChatHeader(buf, GOOD_HANDLE, pkt_len);
      sendPacket(clientSocket, buf, pkt_len);
   }
   else {
      //send flag 3 (failure: handle exists)
      makeChatHeader(buf, HANDLE_EXISTS, pkt_len);
      sendPacket(clientSocket, buf, pkt_len);
   }
}

//adds a new client to the server at the next available position
//realloc the server if not enough room for new client
void addNewClient(Server *s, uint8_t *handle, uint8_t len, int clientSocket) {

   int i;
   // check if need to increase server size
   if(s->num_handles == s->num_allocations) {
      // double allocation space ( * 2)
      s->clients = srealloc(s->clients, sizeof(Handle) * s->num_allocations * 2);
      s->num_allocations *= 2;
      s->socket_numbers = srealloc(s->socket_numbers, sizeof(char) * s->num_allocations);
      s->socket_status = srealloc(s->socket_status, sizeof(char) * s->num_allocations);
      // need to memset the new arrays to 0?
   }

   // Ready to actualy add client to server
   // loop through socket status to find first available socket CLOSED (0)
   for(i = 0; i < s->num_allocations; i++) {
      if(s->socket_status[i] == CLOSED) { //available to add
         memcpy(s->clients[i].handle, handle, sizeof(char)*len);
         //printf("added %s to server client list", s->clients[i].handle);
         s->socket_numbers[i] = clientSocket;
         s->socket_status[i] = OPEN;
         s->num_handles++;
         //printf(" with socket %d NumClients = %d\n", clientSocket, s->num_handles);
         break;
      }
   }
}

// looks up if the given handle name is in the server database
// if so, return index in server table [0,num_handles-1]
// else return -1 if not found
// assume handle null terminated here
int lookupClient(Server *s, Handle handle) {

   int i;
   //loop allocations incase client was removed
   for(i = 0; i < s->num_allocations; i++) {
      if(strcmp((char *)handle.handle, (char *)s->clients[i].handle) == 0) { // equal - handle exists
         if(s->socket_status[i] == OPEN) {
            //make sure open handle (closed handles aren't nulled out)
            //printf("Found matching client at index: %d!\n", i);
            return i;
         }
      }
   }
   // handle doesnt exist
   return -1;
}

void removeClient(int clientSocket, Server *s) {
	//printf("Client on socket %d terminted\n", clientSocket);
	removeFromPollSet(clientSocket);
   removeClientFromServer(clientSocket, s);
	close(clientSocket);
}

/* helper function for removeClient - updates server */
void removeClientFromServer(int clientSocket, Server *s) {
   int i;
   for(i = 0; i < s->num_allocations; i++) {
      if(s->socket_numbers[i] == clientSocket) {
         s->socket_status[i] = CLOSED;
         s->num_handles--;
      }
   }
}

// Checks args and returns port number
int checkArgs(int argc, char *argv[]) {
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}

	return portNumber;
}
