/*
 * uipc_syscall.h
 *
 *  Created on: Oct 23, 2013
 *      Author: root
 */

#ifndef SHAUN_UIPC_SYSCALL_H_
#define SHAUN_UIPC_SYSCALL_H_

#include <kernel/sys/socket.h>
#include <kernel/types.h>

extern int
ksocket(int dom, int type, int proto);

extern int
kbind(int fd, struct sockaddr *addr, socklen_t len);
extern int
klisten(int fd, int backlog);
extern int
kaccept(int fd, struct sockaddr *addr, socklen_t *len);
extern int
ksendit(int fd, struct msghdr *msg, int flags);
extern int
krecvit(int fd, struct msghdr *msg, int *fromlen);
extern int
kshutdown(int fd, int how);
extern int
kconnect(int fd, struct sockaddr *addr, socklen_t len);
extern int
ksocketpair(int domain, int type, int protocol, int *retval);
extern int
ksetsockopt(int sockfd, int level, int optname, void *optval, socklen_t optlen);
extern int
kgetsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
#endif /* SHAUN_UIPC_SYSCALL_H_ */
