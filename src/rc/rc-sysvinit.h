/*
 * rc-sysvinit.h	- Interface to communicate with sysvinit via /run/initctl.
 */
#ifndef _RC_SYSVINIT_H
#define _RC_SYSVINIT_H

#define INIT_MAGIC 0x03091969
#define INIT_CMD_START		0
#define INIT_CMD_RUNLVL		1
#define INIT_CMD_POWERFAIL	2
#define INIT_CMD_POWERFAILNOW	3
#define INIT_CMD_POWEROK	4
#define INIT_CMD_BSD		5
#define INIT_CMD_SETENV		6
#define INIT_CMD_UNSETENV	7

/*
 *	This is what BSD 4.4 uses when talking to init.
 *	Linux doesn't use this right now.
 */
struct init_request_bsd {
	char	gen_id[8];		/* Beats me.. telnetd uses "fe" */
	char	tty_id[16];		/* Tty name minus /dev/tty      */
	char	host[64];	/* Hostname                     */
	char	term_type[16];		/* Terminal type                */
	int	signal;			/* Signal to send               */
	int	pid;			/* Process to send to           */
	char	exec_name[128];	        /* Program to execute           */
	char	reserved[128];		/* For future expansion.        */
};

/*
 *	Because of legacy interfaces, "runlevel" and "sleeptime"
 *	aren't in a seperate struct in the union.
 *
 *	The weird sizes are because init expects the whole
 *	struct to be 384 bytes.
 */
struct init_request {
	int	magic;			/* Magic number                 */
	int	cmd;			/* What kind of request         */
	int	runlevel;		/* Runlevel to change to        */
	int	sleeptime;		/* Time between TERM and KILL   */
	union {
		struct init_request_bsd	bsd;
		char			data[368];
	} i;
};

void sysvinit_runlevel(char rl);

#endif
