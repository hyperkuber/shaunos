/*
 * select.h
 *
 *  Created on: Aug 14, 2013
 *      Author: root
 */

#ifndef SHAUN_SELECT_H_
#define SHAUN_SELECT_H_

#include <kernel/types.h>

/*
 * Used to maintain information about processes that wish to be
 * notified when I/O becomes possible.
 */
struct selinfo {
	pid_t	si_pid;		/* process to be notified */
	short	si_flags;	/* see below */
};

/* Socket level message types.  This must match the definitions in
   <linux/socket.h>.  */
enum
  {
    SCM_RIGHTS = 0x01		/* Transfer file descriptors.  */
#define SCM_RIGHTS SCM_RIGHTS
#ifdef __USE_GNU
    , SCM_CREDENTIALS = 0x02	/* Credentials passing.  */
# define SCM_CREDENTIALS SCM_CREDENTIALS
#endif
  };

#endif /* SHAUN_SELECT_H_ */
