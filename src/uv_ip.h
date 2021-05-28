/* IP-related utils. */

#ifndef UV_IP_H_
#define UV_IP_H_

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

/* Split @address into @host and @port and populate @addr accordingly. */
int uvIpParse(const char *address, struct sockaddr_in *addr);

#endif /* UV_IP_H */
