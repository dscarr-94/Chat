/* Written Hugh Smith, Updated: April 2020
 * Use at your own risk.  Feel free to copy, just leave my name in it.
 * Modified by Dylan Carr April 2020
 * dscarr94@gmail.com
 * Provides an interface to the poll() library.  Allows for
 * adding a file descriptor to the set, removing one and calling poll.
 */

#ifndef __POLLLIB_H__
#define __POLLLIB_H__

#include "packets.h"

#define POLL_SET_SIZE 10
#define POLL_WAIT_FOREVER -1

void setupPollSet();
void addToPollSet(int socketNumber);
void removeFromPollSet(int socketNumber);
int pollCall(int timeInMilliSeconds);
void * srealloc(void *ptr, size_t size);
void * sCalloc(size_t nmemb, size_t size);

#endif
