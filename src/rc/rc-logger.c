/*
  rc-logger.c
  Spawns a logging daemon to capture stdout and stderr so we can log
  them to a buffer and/or files.
*/

/*
 * Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <ctype.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#if defined(__linux__) || defined(__GLIBC__)
#  include <pty.h>
#elif defined(__NetBSD__) || defined(__OpenBSD__)
#  include <util.h>
#else
#  include <libutil.h>
#endif

#include "einfo.h"
#include "rc-logger.h"
#include "rc.h"
#include "rc-misc.h"

#define TMPLOG RC_SVCDIR "/rc.log"
#define DEFAULTLOG "/var/log/rc.log"

static int signal_pipe[2] = { -1, -1 };
static int fd_stdout = -1;
static int fd_stderr = -1;
static const char *runlevel = NULL;
static bool in_escape = false;
static bool in_term = false;

static char *logbuf = NULL;
static size_t logbuf_size = 0;
static size_t logbuf_len = 0;

pid_t rc_logger_pid = -1;
int rc_logger_tty = -1;
bool rc_in_logger = false;

static void
write_log(int logfd, const char *buffer, size_t bytes)
{
	const char *p = buffer;

	while ((size_t)(p - buffer) < bytes) {
		switch (*p) {
		case '\r':
			goto cont;
		case '\033':
			in_escape = true;
			in_term = false;
			goto cont;
		case '\n':
			in_escape = in_term = false;
			break;
		case '[':
			if (in_escape)
				in_term = true;
			break;
		}

		if (!in_escape) {
			if (write(logfd, p++, 1) == -1)
				eerror("write: %s", strerror(errno));
			continue;
		}

		if (! in_term || isalpha((unsigned char)*p))
			in_escape = in_term = false;
cont:
		p++;
	}
}

static void
write_time(FILE *f, const char *s)
{
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);

	fprintf(f, "\nrc %s logging %s at %s\n", runlevel, s, asctime(tm));
	fflush(f);
}

void
rc_logger_close(void)
{
	int sig = SIGTERM;

	if (signal_pipe[1] > -1) {
		if (write(signal_pipe[1], &sig, sizeof(sig)) == -1)
			eerror("write: %s", strerror(errno));
		close(signal_pipe[1]);
		signal_pipe[1] = -1;
	}

	if (rc_logger_pid > 0)
		waitpid(rc_logger_pid, 0, 0);

	if (fd_stdout > -1)
		dup2(fd_stdout, STDOUT_FILENO);
	if (fd_stderr > -1)
		dup2(fd_stderr, STDERR_FILENO);
}

void
rc_logger_open(const char *level)
{
	int slave_tty;
	struct termios tt;
	struct winsize ws;
	char buffer[BUFSIZ];
	struct pollfd fd[2];
	int s = 0;
	size_t bytes;
	int i;
	FILE *log = NULL;
	FILE *plog = NULL;
	const char *logfile;
	int log_error = 0;

	if (!rc_conf_yesno("rc_logger"))
		return;

	if (pipe(signal_pipe) == -1)
		eerrorx("pipe: %s", strerror(errno));
	for (i = 0; i < 2; i++)
		if ((s = fcntl (signal_pipe[i], F_GETFD, 0) == -1 ||
			fcntl (signal_pipe[i], F_SETFD, s | FD_CLOEXEC) == -1))
			eerrorx("fcntl: %s", strerror (errno));

	if (isatty(STDOUT_FILENO)) {
		tcgetattr(STDOUT_FILENO, &tt);
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
		if (openpty(&rc_logger_tty, &slave_tty, NULL, &tt, &ws))
			return;
	} else
		if (openpty(&rc_logger_tty, &slave_tty, NULL, NULL, NULL))
			return;

	if ((s = fcntl(rc_logger_tty, F_GETFD, 0)) == 0)
		fcntl(rc_logger_tty, F_SETFD, s | FD_CLOEXEC);

	if ((s = fcntl(slave_tty, F_GETFD, 0)) == 0)
		fcntl(slave_tty, F_SETFD, s | FD_CLOEXEC);

	rc_logger_pid = fork();
	switch (rc_logger_pid) {
	case -1:
		eerror("fork: %s", strerror(errno));
		break;
	case 0:
		rc_in_logger = true;
		close(signal_pipe[1]);
		signal_pipe[1] = -1;

		runlevel = level;
		if ((log = fopen(TMPLOG, "ae")))
			write_time(log, "started");
		else {
			free(logbuf);
			logbuf_size = BUFSIZ * 10;
			logbuf = xmalloc(sizeof (char) * logbuf_size);
			logbuf_len = 0;
		}

		fd[0].fd = signal_pipe[0];
		fd[0].events = fd[1].events = POLLIN;
		fd[0].revents = fd[1].revents = 0;
		if (rc_logger_tty >= 0)
			fd[1].fd = rc_logger_tty;
		for (;;) {
			if ((s = poll(fd,
				    rc_logger_tty >= 0 ? 2 : 1, -1)) == -1)
			{
				eerror("poll: %s", strerror(errno));
				break;
			} else if (s == 0)
				continue;

			if (fd[1].revents & (POLLIN | POLLHUP)) {
				memset(buffer, 0, BUFSIZ);
				bytes = read(rc_logger_tty, buffer, BUFSIZ);
				if (write(STDOUT_FILENO, buffer, bytes) == -1)
					eerror("write: %s", strerror(errno));

				if (log)
					write_log(fileno (log), buffer, bytes);
				else {
					if (logbuf_size - logbuf_len < bytes) {
						logbuf_size += BUFSIZ * 10;
						logbuf = xrealloc(logbuf,
						    sizeof(char ) *
						    logbuf_size);
					}

					memcpy(logbuf + logbuf_len,
					    buffer, bytes);
					logbuf_len += bytes;
				}
			}

			/* Only SIGTERMS signals come down this pipe */
			if (fd[0].revents & (POLLIN | POLLHUP))
				break;
		}
		if (logbuf) {
			if ((log = fopen(TMPLOG, "ae"))) {
				write_time(log, "started");
				write_log(fileno(log), logbuf, logbuf_len);
			}
			free(logbuf);
		}
		if (log) {
			write_time(log, "stopped");
			fclose(log);
		}

		/* Append the temporary log to the real log */
		logfile = rc_conf_value("rc_log_path");
		if (logfile == NULL)
			logfile = DEFAULTLOG;

		if ((plog = fopen(logfile, "ae"))) {
			if ((log = fopen(TMPLOG, "re"))) {
				while ((bytes = fread(buffer, sizeof(*buffer), BUFSIZ, log)) > 0) {
					if (fwrite(buffer, sizeof(*buffer), bytes, plog) < bytes) {
						log_error = 1;
						eerror("Error: write(%s) failed: %s", logfile, strerror(errno));
						break;
					}
				}
			} else {
				log_error = 1;
				eerror("Error: fopen(%s) failed: %s", TMPLOG, strerror(errno));
			}

			fclose(log);
			fclose(plog);
		} else {
			/*
			 * logfile or its basedir may be read-only during sysinit and
			 * shutdown so skip the error in this case
			 */
			if (errno != EROFS && ((strcmp(level, RC_LEVEL_SHUTDOWN) != 0) && (strcmp(level, RC_LEVEL_SYSINIT) != 0))) {
				log_error = 1;
				eerror("Error: fopen(%s) failed: %s", logfile, strerror(errno));
			}
		}

		/* Try to keep the temporary log in case of errors */
		if (!log_error) {
			if (errno != EROFS && ((strcmp(level, RC_LEVEL_SHUTDOWN) != 0) && (strcmp(level, RC_LEVEL_SYSINIT) != 0)))
				if (unlink(TMPLOG) == -1)
					eerror("Error: unlink(%s) failed: %s", TMPLOG, strerror(errno));
		} else if (exists(TMPLOG))
			eerrorx("Warning: temporary logfile left behind: %s", TMPLOG);

		exit(0);
		/* NOTREACHED */

	default:
		setpgid(rc_logger_pid, 0);
		fd_stdout = dup(STDOUT_FILENO);
		fd_stderr = dup(STDERR_FILENO);
		if ((s = fcntl(fd_stdout, F_GETFD, 0)) == 0)
			fcntl(fd_stdout, F_SETFD, s | FD_CLOEXEC);

		if ((s = fcntl(fd_stderr, F_GETFD, 0)) == 0)
			fcntl(fd_stderr, F_SETFD, s | FD_CLOEXEC);
		dup2(slave_tty, STDOUT_FILENO);
		dup2(slave_tty, STDERR_FILENO);
		if (slave_tty != STDIN_FILENO &&
		    slave_tty != STDOUT_FILENO &&
		    slave_tty != STDERR_FILENO)
			close(slave_tty);
		close(signal_pipe[0]);
		signal_pipe[0] = -1;
		break;
	}
}
