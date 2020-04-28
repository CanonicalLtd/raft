/* IP-related utils. */

#ifndef UV_IP_H_
#define UV_IP_H_

#if defined(__linux__)
#include <netinet/in.h>
#elif defined(_WIN32)
#include <winsock2.h>
#endif

/* Split @address into @host and @port and populate @addr accordingly. */
int uvIpParse(const char *address, struct sockaddr_in *addr);

#endif /* UV_IP_H */
