/*
   rc-logger.h
   Copyright 2007 Gentoo Foundation
   */

pid_t rc_logger_pid;
int rc_logger_tty;
extern bool rc_in_logger;

void rc_logger_open (const char *runlevel);
void rc_logger_close ();
