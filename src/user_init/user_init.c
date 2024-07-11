#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include "helpers.h"
#include "rc.h"

#define USERINIT RC_LIBEXECDIR "/sh/user-init.sh"

int main(int argc, char **argv) {
	struct passwd *user;
	char *cmd;
	int nullfd = -1;
	if (argc < 3)
		return 1;

	user = getpwnam(argv[1]);
	if (!user || initgroups(user->pw_name, user->pw_gid) == -1
			|| setgid(user->pw_gid) == -1
			|| setuid(user->pw_uid) == -1)
		return 1;

	setenv("HOME", user->pw_dir, true);
	setenv("SHELL", user->pw_shell, true);

	nullfd = open("/dev/null", O_RDWR);
	dup2(nullfd, STDIN_FILENO);
	close(nullfd);

	xasprintf(&cmd, "%s %s", USERINIT, argv[2]);
	execl(user->pw_shell, "-", "-c", cmd, NULL);
}
