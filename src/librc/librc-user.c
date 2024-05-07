#include <stdlib.h>

#include "librc.h"
#include "einfo.h"

RC_STRINGLIST *
rc_users_list(void)
{
	if (rc_is_user())
		return rc_stringlist_new();
	return ls_dir(RC_SVCDIR "/users", 0);
}

char *
rc_user_value_get(const char *user, const char *option)
{
	char *file, *buffer = NULL;
	size_t len;

	if (rc_is_user())
		return NULL;

	xasprintf(&file, "%s/users/%s/%s", rc_service_dir(), user, option);
	rc_getfile(file, &buffer, &len);
	free(file);

	return buffer;
}

bool
rc_user_value_set(const char *user, const char *option, const char *value)
{
	char *dir, *file;
	FILE *fp;

	if (rc_is_user())
		return false;

	xasprintf(&dir, "%s/users/%s", rc_service_dir(), user);
	if (mkdir(file, 0755) != 0 && errno != EEXIST) {
		free(dir);
		return false;
	}

	xasprintf(&file, "%s/%s", dir, option);
	free(dir);

	if (!value) {
		unlink(file);
	} else if ((fp = fopen(file, "w"))) {
		fprintf(fp, "%s", value);
		fclose(fp);
	} else {
		free(file);
		return false;
	}

	free(file);
	return true;
}
