/*
 * ndd.c	- main source file
 * Copyright (C) 2015 jansen@atlas.cz
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <signal.h>

#include "ndd.h"

ndd_t nds;

/*
 * Print the usage message.
 */
void
usage()
{
	(void) fprintf(stderr, "ndd [-dvh] [-c config] [-l lock_file]\n");
	exit(1);
}

/*
 * Log message using the syslogd(8). If we are not a daemon, print it also
 * to stderr.
 */
void
log_msg(int level, const char *fmt, ...)
{
	va_list arg;
	char log_str[MAX_LOG_LEN];

	if (level > nds.log_level)
		return;

	va_start(arg, fmt);
	(void) vsnprintf(log_str, MAX_LOG_LEN, fmt, arg);
	va_end(arg);

	syslog(LOG_INFO, "%s", log_str);
	if (!nds.is_daemon) {
		(void) fprintf(stderr, "%s\n", log_str);
		(void) fflush(stderr);
	}
}

/*
 * Handle signals. We currently handle only SIGTERM and SIGINT; we simply
 * exit in both cases.
 */
void
signal_handler(int sig)
{
	assert(sig == SIGTERM || sig == SIGINT);
	log_msg(0, "Shuting down...");
	nds.exiting = 1;
	exit(0);
}

/*
 * Daemonise ndd so it can run at background.
 */
int
daemonise()
{
	pid_t pid, sid;

	pid = fork();
	if (pid < 0) {
		log_msg(0, "Fork failed: %d", errno);
		return (1);
	}
	if (pid > 0) {
		/* We are the parent process. */
		exit(0);
	}

	umask(0);

	if ((sid = setsid()) < 0) {
		log_msg(0, "Unable to create a new SID: %d", errno);
		return (1);
	}
	if ((chdir("/")) < 0) {
		log_msg(0, "Unable to change to /: %d", errno);
		return (1);
	}
	/* Close all standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* stderr is no more available */
	nds.is_daemon = 1;

	signal(SIGCHLD, SIG_IGN);
	return (0);
}

/*
 * We use a lock file to make sure we are the only instance of this
 * program running on the system. The lock file contains the pid so
 * the children has to call this again after it daemonize to write
 * down its pid.
 * Note that we use O_EXCL so this may not work properly for lock
 * file on NFS.
 */
int
create_lock(ndd_t *ndd)
{
	char pid_str[10];
	int pid_len;

	if (ndd->lf_fd < 0) {
		/* We don't have the lock file yet -- create it. */
		if ((ndd->lf_fd = open(ndd->lf, O_RDWR | O_CREAT | O_EXCL,
		    0640)) < 0) {
			log_msg(0, "Unable to create lock file %s: %d",
			    ndd->lf, errno);
			return (1);
		}
		log_msg(0, "Lock file %s created, fd: %d", ndd->lf, ndd->lf_fd);
	} else {
		/* The lock file already exists, just update the pid. */
		(void) lseek(ndd->lf_fd, 0, SEEK_SET);
		log_msg(0, "Updating pid only: %d", ndd->lf_fd);
	}
	(void) snprintf(pid_str, sizeof (pid_str), "%ld\n", getpid());
	pid_len = strlen(pid_str);

	if (write(ndd->lf_fd, pid_str, pid_len) != pid_len) {
		log_msg(0, "Unable to write the lock file %s: %d",
		    ndd->lf, errno);
		return (1);
	}

	return (0);
}

/*
 * Close and remove the lock file.
 */
void
remove_lock()
{
	if (nds.lf_fd < 0)
		return;
	(void) close(nds.lf_fd);
	(void) unlink(nds.lf);
	log_msg(5, "Lock file %s removed.", nds.lf);
	nds.lf_fd = -1;
}

/*
 * We call this from atexit(). The main purpose of this function is
 * remove the lock file.
 */
void
cleanup()
{
	if (!nds.exiting)
		log_msg(1, "Warning: exiting for uknown reason.");

	/* Close the network socket. */
	if (nds.sc_fd >= 0)
		(void) close(nds.sc_fd);

	/* Close all minors. */
	close_minors();
	remove_lock();
}

int
main(int argc, char **argv)
{
	char *conf_file = NULL;
	int do_daemon = 1;
	int c;

	/* Initialize the context */
	nds.is_daemon = 0;
	nds.exiting = 0;
	nds.lf_fd = -1;
	nds.sc_fd = -1;
	nds.lf = DEFAULT_LOCK_FILE;

	while ((c = getopt(argc, argv, "dc:l:vh")) != -1) {
		switch (c) {
		case 'd':	/* debug */
			do_daemon = 0;
			break;
		case 'c':	/* config file */
			conf_file = optarg;
			break;
		case 'l':	/* lock file */
			nds.lf = optarg;
			break;
		case 'v':
			(void) printf("Version: %s\n", VERSION);
			exit(0);
			break;
		case 'h':
			usage();
			break;
		case '?':
			if (optopt == 'c') {
				(void) fprintf(stderr, "Missing configuration"
				    " file name.\n");
			} else if (optopt == 'l') {
				(void) fprintf(stderr, "Missing lock file "
				    "name.\n");
			}
			usage();
			break;
		default:
			abort();
		}
	}

	openlog(NULL, LOG_PID, LOG_USER);

	/* Load the config file. */
	if (load_config(conf_file, &nds) != 0) {
		(void) fprintf(stderr, "Unable to load configuration.\n");
		exit(1);
	}

	/* Try to create the lock file. */
	if (create_lock(&nds)) {
		log_msg(0, "Unable to get the lock. Exiting.");
		close_minors();
		exit(1);
	}

	/* Initialize the network. */
	if ((nds.sc_fd = init_network()) == -1) {
		log_msg(0, "Unable to initialize network. Exiting.");
		close_minors();
		remove_lock();
		exit(1);
	}

	if (do_daemon) {
		if (daemonise()) {
			log_msg(0, "Unable to deamonize. Exiting.");
			close_minors();
			remove_lock();
			exit(1);
		}
		/* Update the lock file with the new PID the child got. */
		if (create_lock(&nds)) {
			log_msg(0, "Unable to lock. Exiting.");
			close_minors();
			remove_lock();
			exit(1);
		}
	}

	/* We handle SIGTERM and SIGINT only */
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	/* atexit function will remove the lock file. */
	atexit(cleanup);

	serve(&nds);

	exit(0);
}
