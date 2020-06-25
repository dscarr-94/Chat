//
// Written Hugh Smith, Updated: April 2020
// Use at your own risk.  Feel free to copy, just leave my name in it.
// Modified by Dylan Carr April 2020
//
#include "networks.h"
#include "pollLib.h"
#include "packets.h"

/* Client scope MACROS */
#define DEBUG_FLAG 1
#define SETUP_FLAG 1

/* Function prototypes */
uint16_t getFromStdin(char * sendBuf);
void checkArgs(int argc, char * argv[]);
int checkHandle(char *handle, int setupFlag);
void run(int clientSocket, Handle *handle);
void initPacket_F1(uint8_t handle[MAXBUF+1], int clientSocket);
void checkServerResponse(int clientSocket);
void recvFromServer(int clientSocket);
void handleUserInput(int clientSocket, Handle *handle);
int messageClients(uint8_t buf[MAXBUF], uint16_t len, Handle *src_handle, int clientSocket);
void invalidClient(uint8_t buf[MAXBUF]);
void clientExit(int clientSocket);
int messageClients(uint8_t buf[MAXBUF], uint16_t len, Handle *src_handle, int clientSocket);
void sendMessage(int num_handles, Handle handles[MAX_DEST_HANDLES], char *msg, Handle *src_handle, int clientSocket);
void requestHandleList(int clientSocket);
int getHandleList(uint8_t buf[MAXBUF], int num_clients, int clientSocket);
int getNumHandles(uint8_t buf[MAXBUF]);
void invalidClient(uint8_t buf[MAXBUF]);
void receiveMessage(uint8_t buf[MAXBUF]);
void broadcastClients(uint8_t buf[MAXBUF], uint16_t len, Handle *src_handle, int clientSocket);
void sendBroadcast();
void receiveBroadcast(uint8_t buf[MAXBUF]);

/* User Commands:
 * %M num-handles destination-handle [destination-handle] [text]
 * %B [text]
 * %E
 * %L
*/
int main(int argc, char * argv[]) {

	int clientSocket = 0;  //socket descriptor
	setupPollSet();
	checkArgs(argc, argv); // valid handle past here
	Handle handle;
	memcpy(handle.handle, argv[1], (strlen(argv[1])+1) * sizeof(uint8_t));

	/* set up the TCP Client socket  */
 	clientSocket = tcpClientSetup(argv[2], argv[3], 0);

	/* Pass handle and socketNum to run function */
	run(clientSocket, &handle);

	close(clientSocket);
	return 0;
}

void run(int clientSocket, Handle *handle) {

	int socketToProcess = 0;
	initPacket_F1(handle->handle, clientSocket);

	// BLOCK HERE waiting for f=2 or f=3 flag from server
	checkServerResponse(clientSocket);

	addToPollSet(STDIN_FILENO); // add stdin after recv flag 2/3 packet
	addToPollSet(clientSocket);

	printf("$: ");
	fflush(stdout);
	while(1) {
		//blocks here waiting for user input or msg from server
		if((socketToProcess = pollCall(POLL_WAIT_FOREVER)) != -1) {

			if(socketToProcess == clientSocket) {
				/* Recieve message from Server */
				recvFromServer(clientSocket);
			}
			else if(socketToProcess == STDIN_FILENO) {
				/* Process user commands */
				handleUserInput(clientSocket, handle);
			}
			printf("$: "); // print again before polling next socket
			fflush(stdout);
		}
		else {
			// Just printing here to let me know what is going on
			printf("Poll timed out waiting for server or user to enter data\n");
		}
	}
}

/* handles user command line input */
void handleUserInput(int clientSocket, Handle *handle) {

	uint8_t buf[MAXBUF];
	uint16_t len = getFromStdin((char *)buf); // len includes null, buf has user input w/ null

	uint8_t cmd = 0; //must initlaize 
	//assumes first 2 chars are %[letter]
	if(buf[0] == '%')
		cmd = buf[1];

	cmd = toupper(cmd);
	switch(cmd) {
		case 'M' :
			messageClients(buf, len, handle, clientSocket);
			break;
		case 'B' :
			broadcastClients(buf, len, handle, clientSocket);
			break;
		case 'L' :
			requestHandleList(clientSocket);
			break;
		case 'E' :
			clientExit(clientSocket);
			break;
		default :
			printf("Invalid command\n");
	}
}

void requestHandleList(int clientSocket) {

	uint8_t buf[MAXBUF];
	uint16_t pkt_len = 3;
	makeChatHeader(buf, 10, pkt_len);
	sendPacket(clientSocket, buf, pkt_len);

}

void clientExit(int clientSocket) {

	uint8_t buf[MAXBUF];
	uint16_t pkt_len = 3;
	makeChatHeader(buf, 8, pkt_len);
	sendPacket(clientSocket, buf, pkt_len);

}

void broadcastClients(uint8_t buf[MAXBUF], uint16_t len, Handle *src_handle, int clientSocket) {

	// buf+3 = message start
	int offset = 3; // message start
	char message[MAX_MESSAGE];
	//len includes null (getFromStdin() appended)
	int text_len = len - offset;
	if(text_len <= 1) { //checks if user entered "%B" or "%B "
		//send blank newline msg
		message[0] = '\n';
		message[1] = '\0';
		sendBroadcast((char *)message, src_handle, clientSocket);
	}
	else {
		// len - offset = length of text including \0
		// so check if msg entered was 199+null bytes or less
		// if so, send single packet
		if(text_len <= MAX_MESSAGE) {
			// this copies < 200 byte input correctly (null is included at end)
			memcpy(message, buf+offset, text_len); // +1 for null
			sendBroadcast((char *)message, src_handle, clientSocket);
		}
		else { // here means msg (w/ null) > 200
			//offset = start of msg
			while(text_len > MAX_MESSAGE) {
				memcpy(message, buf+offset, MAX_MESSAGE-1); // copy 199 bytes
				message[MAX_MESSAGE]= '\0';
				sendBroadcast((char *)message, src_handle, clientSocket);
				offset += MAX_MESSAGE-1; // move offset to next part of msg
				text_len -= MAX_MESSAGE-1;
			}
			// handle remaining msg part
			memcpy(message, buf+offset, text_len); // shld include null
			sendBroadcast((char *)message, src_handle, clientSocket);
		}
	}
}

void sendBroadcast(char *msg, Handle *src_handle, int clientSocket) {
	uint8_t buf[MAXBUF];
	uint16_t pkt_len = 0; // = sizeof(chatHdr); // 3 to start
	uint8_t flag = BROADCAST_FLAG;
	uint8_t src_handle_len = strlen((char *)src_handle->handle); // length without null
	memcpy(buf+3, &src_handle_len, 1);
	memcpy(buf+4, src_handle->handle, src_handle_len * sizeof(uint8_t)); // dont want null term in pkt

	int offset = 4+src_handle_len;
	int msg_len = strlen(msg) + 1; // strlen - no null term  so add 1
	//offset now ready for message
	memcpy(buf+offset, msg, msg_len * sizeof(uint8_t));
	offset += msg_len; // should be pkt_len now
	pkt_len = (uint16_t)offset;
	makeChatHeader(buf, flag, pkt_len);
	sendPacket(clientSocket, buf, pkt_len);
}

// buf points to beginning of user input (%) and IS NULL TERMINATED
// returns -1 on user input error
// buf will be filled with MAXBUF-1 characters of user input (ignore the rest)
int messageClients(uint8_t buf[MAXBUF], uint16_t len, Handle *src_handle, int clientSocket) {

	int num_handles = 0;
	int i, offset;
	offset = 5; //start of first dest_handle "%m # "
	Handle handles[MAX_DEST_HANDLES];

	char delim[] = " ";
	char *tok = strtok((char *)buf, delim); // gets %M
	tok = strtok(NULL, delim); // gets num_handles
	num_handles = atoi(tok);

	// checks number of handles entered
	if(num_handles < 1 || num_handles > 9) {
		printf("num handles must be [1-9]\n");
		return -1;
	}

	// adds each dest_handle to handles to send array if valid
	for(i = 0; i < num_handles; i++) {
		if((tok = strtok(NULL, delim)) != NULL) {
			if(checkHandle(tok, 0) < 0)
				return -1;
			// here valid handale to add
			offset += strlen(tok) + 1; // add 1 for space
			memcpy(handles[i].handle, tok, ((strlen(tok)+1) * sizeof(uint8_t)));
		}
		else {
			printf("null token\n");
			return -1;
		}
	}

	/* print handles
	printf("handles in array:\n");
	for(i = 0; i < num_handles; i++) {
		printf(" %s\n", handles[i].handle);
 } */

	/* print buf
	for(i = 0; i < len; i++) {
		printf("buf[%d]: %c %u\n", i, buf[i], buf[i]);
	} */

	char message[MAX_MESSAGE];
	//text len includes null (getFromStdin() appended)

	int text_len = len - offset; //shld be 0 if user hit enter after last dest_handle
	// checks if null is last char in buff at last dest_handle - empty/blank msg
	if(offset >= len) { //blank message, send \n as message
		message[0] = '\n';
		message[1] = '\0';
		sendMessage(num_handles, handles, (char *)message, src_handle, clientSocket);
	}
	// len - offset = length of text including \0
	// so check if msg entered was 199+null bytes or less
	// if so, send single packet
	else if(text_len <= MAX_MESSAGE) {
		// this copies < 200 byte input correctly (null is included at end)
		memcpy(message, buf+offset, text_len); // +1 for null
		sendMessage(num_handles, handles, (char *)message, src_handle, clientSocket);
	}
	else { // here means msg (w/ null) > 200
		//offset = start of msg
		while(text_len > MAX_MESSAGE) {
			memcpy(message, buf+offset, MAX_MESSAGE-1); // copy 199 bytes
			message[MAX_MESSAGE]= '\0';
			sendMessage(num_handles, handles, (char *)message, src_handle, clientSocket);
			offset += MAX_MESSAGE-1; // move offset to next part of msg
			text_len -= MAX_MESSAGE-1;
		}
		// handle remaining msg part
		memcpy(message, buf+offset, text_len); // shld include null
		sendMessage(num_handles, handles, (char *)message, src_handle, clientSocket);
	}
	return 1; // success
}

/* sends <msg> packet to server with provided handle list - max 200 byte msg*/
// msg assumed null terminated here so 199 + \0
void sendMessage(int num_handles, Handle handles[MAX_DEST_HANDLES], char *msg, Handle *src_handle, int clientSocket) {

	uint8_t buf[MAXBUF];
	uint16_t pkt_len = 0; // = sizeof(chatHdr); // 3 to start
	uint8_t flag = MESSAGE_FLAG;
	uint8_t src_handle_len = strlen((char *)src_handle->handle); // length without null
	memcpy(buf+3, &src_handle_len, 1);
	memcpy(buf+4, src_handle->handle, src_handle_len * sizeof(uint8_t)); // dont want null term in pkt
	memcpy(buf+4+src_handle_len, &num_handles, 1);
	int i, handle_len = 0;
	int offset = 5+src_handle_len;

	for(i = 0; i < num_handles; i++) {
		handle_len = strlen((char *)handles[i].handle); //get handle length without null
		memcpy(buf+offset, &handle_len, 1);
		offset++;
		memcpy(buf+offset, handles[i].handle, handle_len * sizeof(uint8_t));
		offset += handle_len;
	}

	int msg_len = strlen(msg) + 1; // strlen - no null term  so add 1
	//offset now ready for message
	memcpy(buf+offset, msg, msg_len * sizeof(uint8_t));
	offset += msg_len; // should be pkt_len now
	pkt_len = (uint16_t)offset;
	makeChatHeader(buf, flag, pkt_len);
	sendPacket(clientSocket, buf, pkt_len);
}

/* handles messages from the server */
void recvFromServer(int clientSocket) {

	uint8_t buf[MAXBUF];
	uint8_t flag = 0;
	int messageLen = 0;
	int error = 0;

	// checks if 0 bytes read from server
	if((messageLen = sRecv(buf, clientSocket) < 0)) {
		printf("Server Terminted\n");
		exit(EXIT_FAILURE);
	}
	else {
		memcpy(&flag, buf, 1);

		int num_clients = 0;
		switch(flag) {
			case 4:
				receiveBroadcast(buf);
				break;
         case 5:
				receiveMessage(buf);
            break;

         case 7:
				invalidClient(buf);
            break;

         case 9: // client received exit ACK
				exit(EXIT_SUCCESS);
            break;

         case 11:
				num_clients = getNumHandles(buf);
				printf("Number of clients: %d\n", num_clients);
				error = getHandleList(buf, num_clients, clientSocket);
				if(error < 0)
					perror("getHandleList failed\n");
            break;

         default:
            fprintf(stderr, "server sent bad packet (wrong flag): %u\n", flag);
      } // end switch
	} // end else
}

// buf points to flag of first f =12 packet
int getHandleList(uint8_t buf[MAXBUF], int num_clients, int clientSocket) {

	int i;
	uint8_t handle_len;
	Handle handle;
	for(i = 0; i < num_clients; i++) {
		//buf points to flag
		if((sRecv(buf, clientSocket) < 0)) {
			perror("sRecv in getHandleList f=12\n");
			exit(EXIT_FAILURE);
		}
		handle_len = buf[1];
		memcpy(handle.handle, buf+2, handle_len * sizeof(uint8_t));
		handle.handle[handle_len] = '\0';

		printf("  %s\n", handle.handle);

	}
	// finished processing handle List - now f = 13
	// recv 1 last time for f = 13
	if((sRecv(buf, clientSocket) < 0)) {
		perror("sRecv in getHandleList f=13\n");
		exit(EXIT_FAILURE);
	}
	if(buf[0] == 13)
		return 1; // success
	else
		return -1;
}

int getNumHandles(uint8_t buf[MAXBUF]) {

	int num_handles = 0;
	memcpy(&num_handles, buf+1, sizeof(uint32_t));
	num_handles = ntohl(num_handles);
	return num_handles;

}

// received flag = 7 invalid client packet
// buf points to flag
void invalidClient(uint8_t buf[MAXBUF]) {

	uint8_t handle_len = buf[1];
	Handle handle;
	memcpy(handle.handle, buf+2, handle_len * sizeof(uint8_t));
	handle.handle[handle_len] = '\0';
	printf("Client with handle <%s> does not exist\n", handle.handle);
}

//receieves a broadcast message
// NOT COMPELTE
void receiveBroadcast(uint8_t buf[MAXBUF]) {

	uint8_t offset = 1;
	uint8_t source_handle_len = buf[offset++];
	Handle handle;

	memcpy(handle.handle, buf+offset, source_handle_len * sizeof(uint8_t));
	offset += source_handle_len;
	handle.handle[source_handle_len] = '\0';
	printf("\n%s: %s\n", handle.handle, buf+offset);
}

void receiveMessage(uint8_t buf[MAXBUF]) {

	uint8_t offset = 1;
	uint8_t source_handle_len = buf[offset++];
	Handle handle;

	memcpy(handle.handle, buf+offset, source_handle_len * sizeof(uint8_t));
	offset += source_handle_len;
	handle.handle[source_handle_len] = '\0';

	uint8_t num_dest_handles = buf[offset++];
	int i, handle_len;
	for(i = 0; i < num_dest_handles; i++) {
		handle_len = buf[offset++];
		offset+= handle_len;
	}
	// points to msg at end
	printf("\n%s: %s\n", handle.handle, buf+offset);
}

/* Blocks waiting for flag = 2 or 3 from server */
void checkServerResponse(int clientSocket) {

	uint8_t buf[MAXBUF];
	uint8_t flag = 0;
	if((sRecv(buf, clientSocket) < 0)) {
		printf("Server died\n");
		exit(EXIT_FAILURE);
	}
	memcpy(&flag, buf, 1);
	if(flag == 3) {
		printf("client handle already exists\n");
		exit(EXIT_FAILURE);
	}
}

// sends initial packet to server to validate clients handlename
// blocks until receieves ACK from server
void initPacket_F1(uint8_t handle[MAX_HANDLE+1], int clientSocket) {
	uint8_t buf[MAXBUF];
	uint8_t handleLen = (uint8_t)strlen((char *)handle); //doesnt calc /0 (max 99)
	uint8_t flag = 1;
	//sizoef(handle) includes null terminator (i.e. strlen(handle) + 1)
	// 3 + 1 + length without /0
	uint16_t pkt_len = sizeof(struct chatHeader) + sizeof(handleLen) + handleLen;

	makeChatHeader(buf, flag, pkt_len);
	memcpy(buf+PKT_LEN+FLAG_LEN, &handleLen, 1);
	memcpy(buf+PKT_LEN+FLAG_LEN+1, handle, handleLen); // shouldnt include /0
	sendPacket(clientSocket, buf, pkt_len);
}

// Gets input up to MAXBUF-1 (and then appends \0)
// Returns length of string including null
uint16_t getFromStdin(char * sendBuf) {
	char aChar = 0;
	int inputLen = 0;
	// Important you don't input more characters than you have space
	//printf("%s ", prompt);
	//fflush(stdin);
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			sendBuf[inputLen] = aChar;
			inputLen++;
		}
	}

	sendBuf[inputLen] = '\0';
	inputLen++;  //we are going to send the null

	return inputLen;
}

/* Checks the correct number of arguments and
 * If the handle name is valid
 */
void checkArgs(int argc, char * argv[]) {
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle host-name port-number \n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* check handle name */
	if((checkHandle(argv[1], SETUP_FLAG)) < 0)
		exit(EXIT_FAILURE);
}

/* Checks if handle is longer than 100 chars
 * Return -1 if cmd handle too long or 1st letter not a-z (A-Z)
 * Return 1 on valid handle
 */
int checkHandle(char *handle, int setupFlag) {

	// check handle length of 100 (not including null term)
	if(strlen(handle) > MAX_HANDLE) {
		if(setupFlag)
			printf("Invalid handle, handle longer than 100 characters: <%s>\n", handle);
		else
			printf("Invalid handle, handle longer than 100 characters: <%s> ignoring cmd...\n", handle);

		return -1;
	}

	// check if first letter between A-Z (a-z)
	if(toupper(handle[0]) < 'A' || toupper(handle[0]) > 'Z') {

		if(setupFlag)
			printf("Invalid handle, handle starts with a number\n");
		else
			printf("Invalid handle, handle starts with a number ignoring cmd...\n");

		return -1;
	}

	//valid handle
	return 1;
}
