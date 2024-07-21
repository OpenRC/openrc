#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <stdlib.h>

#include "helpers.h"
#include "rc.h"

#define USERINIT RC_LIBEXECDIR "/sh/user-init.sh"

int main(int argc, char **argv) {
	struct passwd *user;
	char *cmd;
	if (argc < 3)
		return 1;

	user = getpwnam(argv[1]);
	if (!user || initgroups(user->pw_name, user->pw_gid) == -1
			|| setgid(user->pw_gid) == -1
			|| setuid(user->pw_uid) == -1)
		return 1;

	setenv("HOME", user->pw_dir, true);
	xasprintf(&cmd, "%s %s", USERINIT, argv[2]);
	execl(user->pw_shell, user->pw_shell, "-c", cmd, NULL);
}
