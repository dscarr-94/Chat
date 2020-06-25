/* Code written by Hugh Smith	April 2017
 * Feel free to copy, just leave my name in it, use at your own risk.
 * replacement code for gethostbyname for IPv6
 * gives either IPv6 address or IPv4 mapped IPv6 address
 * Use with socket family AF_INET6
 * Modified by Dylan Carr April 2020
 * dscarr94@gmail.com
 */

#ifndef GETHOSTBYNAME6_H
#define GETHOSTBYNAME6_H

#include "packets.h"

uint8_t * gethostbyname6(const char * hostName);
char * getIPAddressString(uint8_t * ipAddress);
uint8_t * getIPAddress6(const char * hostName, struct sockaddr_in6 * aSockaddr);


#endif
