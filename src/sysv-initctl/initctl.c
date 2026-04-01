#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <syslog.h>

#include <rc.h>

#include "initreq.h"

static bool init_halt = false;

static void sysvinit_runlevel(int runlevel)
{
	const char *cmd;
	int fifo;

	switch (runlevel) {
	case '0':
		cmd = init_halt ? "halt" : "poweroff";
		break;
	case '1':
	case 'S':
	case 's':
		cmd = "single";
		break;
	case '2':
	case '3':
	case '4':
	case '5':
		/* TODO: switch to default runlevel? */
		return;
	case '6':
		/* RB_KEXEC: Gentoo inittab calls /sbin/reboot -k */
		cmd = "kexec";
		break;
	case 'Q':
	case 'q':
		return;
	case 'U':
	case 'u':
		cmd = "reexec";
		break;
	}

	if ((fifo = open(RC_INIT_FIFO, O_WRONLY | O_NONBLOCK)) == -1) {
		syslog(LOG_ERR, "open: %s", strerror(errno));
		return;
	}

	if (write(fifo, cmd, strlen(cmd) + 1) == -1)
		syslog(LOG_ERR, "write: %s", strerror(errno));
	close(fifo);
}

static void sysvinit_setenv(char *data, size_t size)
{
	data[size - 1] = '\0';
	for (char *end = data + size; data < end && *data; data += strlen(data) + 1) {
		/* We only care about the INIT_HALT variable */
		if (!strcmp(data, "INIT_HALT=HALT"))
			init_halt = true;
		else if (!strcmp(data, "INIT_HALT") || !strncmp(data, "INIT_HALT=", 10))
			init_halt = false;
	}
}

int main(void) {
	int fifo;

	openlog("sysv-initctl", 0, LOG_DAEMON);
	if (mkfifo("/run/initctl", 0600) == -1 && errno != EEXIST) {
		syslog(LOG_ERR, "mkfifo: %s", strerror(errno));
		return 1;
	}
	symlink("/run/initctl", "/dev/initctl");
	if ((fifo = open("/run/initctl", O_RDONLY | O_NONBLOCK)) == -1) {
		syslog(LOG_ERR, "open: %s", strerror(errno));
		return 1;
	}

	for (;;) {
		struct init_request req;
		ssize_t count = read(fifo, &req, sizeof(req));

		if (count == -1) {
			syslog(LOG_ERR, "read: %s", strerror(errno));
			continue;
		}

		if (count != sizeof(req)) {
			syslog(LOG_ERR, "read: short count");
			continue;
		}

		if (req.magic != INIT_MAGIC) {
			syslog(LOG_ERR, "invalid magic bytes");
			continue;
		}

		switch (req.cmd) {
		case INIT_CMD_RUNLVL:
			sysvinit_runlevel(req.runlevel);
			break;
		case INIT_CMD_SETENV:
		case INIT_CMD_UNSETENV:
			sysvinit_setenv(req.i.data, sizeof(req.i.data));
			break;
		}
	}
}
