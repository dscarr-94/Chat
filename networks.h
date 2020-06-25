
//
// Written Hugh Smith, Updated: April 2020
// Use at your own risk.  Feel free to copy, just leave my name in it.
//


#ifndef __NETWORKS_H__
#define __NETWORKS_H__

/* Client and Server scope macros */
#define BACKLOG 10
#define MAXBUF 1400
#define MAX_HANDLE 100
#define PORT 55555


#define TIME_IS_NULL 1
#define TIME_IS_NOT_NULL 2

// for the server side
int tcpServerSetup(int portNumber);
int tcpAccept(int server_socket, int debugFlag);

// for the client side
int tcpClientSetup(char * serverName, char * port, int debugFlag);

int selectCall(int socketNumber, int seconds, int microseconds, int timeIsNotNull);



#endif
