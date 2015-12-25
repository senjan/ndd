/*
 * minor.c	- soft states
 * Copyright (C) 2015 jansen@atlas.cz
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "iniparser.h"

#include "nd_pkt.h"
#include "minor.h"
#include "ndd.h"

static ndmin_t *nd_minors[MAX_MINOR];

/*
 * Return 'sofstate' for given minor.
 */
ndmin_t *
get_minor(int mn)
{
	if (mn < 0 || mn > MAX_MINOR)
		return (NULL);
	return (nd_minors[mn]);
}

/*
 * Return the disk size in blocks (512 bytes).
 * Size of non-existing disk is 0.
 */
uint32_t
get_minor_size(int mn)
{
	ndmin_t *m;

	m = get_minor(mn);
	return (m != NULL ? m->size / 512 : 0);
}

/*
 * Initialize given 'softstate'.
 */
static int
open_minor(int n, char *name, int m)
{
	struct stat st;
	mode_t mode = O_RDONLY;
	ndmin_t *minor;
	int fd;

	if (m == WR)
		mode = O_RDWR;

	if ((fd = open(name, mode)) < 0) {
		log_msg(0, "Unable to open file %s: %s",
		    name, strerror(errno));
		return (1);
	}
	if (fstat(fd, &st) == -1) {
		(void) close(fd);
		log_msg(0, "Unable to get size of %s: %s",
		    name, strerror(errno));
		return (1);
	}

	if ((minor = malloc(sizeof (ndmin_t))) == NULL) {
		(void) close(fd);
		log_msg(0, "Unable to allocate memory for minor");
		return (1);
	}
	minor->fd = fd;
	minor->size = st.st_size;
	minor->mode = m;
	minor->ack = NULL;
	nd_minors[n] = minor;
	log_msg(2, "nd%d is %s, %llu blocks %s", n, name,
	    (unsigned long long ) st.st_size / 512, (m != WR) ? "(RO)" : "");

	return (0);
}

/*
 * Load configuration.
 * Note that logging is not available yet.
 */
int
load_config(const char *cf, ndd_t *nds)
{
	dictionary *ini;
	int m;

	ini = iniparser_load(cf ? cf : INI_FILE);
	if (ini == NULL) {
		(void) fprintf(stderr, "Unable to open ini file.\n");
		return (ENOENT);
	}

	nds->log_level = (short) iniparser_getint(ini, "general:log_level", 0);
	nds->simple_ack = (short) iniparser_getboolean(ini,
	    "general:simple_ack", -1);

	for (m = 0; m < MAX_MINOR; m++) {
		char key[100];
		char *fn, *mods;
		int mode;

		nd_minors[m] = NULL;
		(void) sprintf(key, "nd%d:path", m);
		if ((fn = iniparser_getstring(ini, key, NULL)) == NULL)
			continue;
		(void) sprintf(key, "nd%d:mode", m);
		if ((mods =  iniparser_getstring(ini, key, WR_STR)) == NULL)
			continue;
		if (strcmp(mods, RDONLY_STR) == 0) {
			mode = RDONLY;
		} else if (strcmp(mods, WR_STR) == 0) {
			mode = WR;
		} else {
			log_msg(0, "nd%d has unknown mode: '%s'! Skipping.",
			    m, mods);
			continue;
		}
		if (open_minor(m, fn, mode))
			log_msg(0, "Skipping nd%d.", m);
	}
	iniparser_freedict(ini);

	return (0);
}

void
close_minors()
{
	int m;
	for (m = 0; m < MAX_MINOR; m++) {
		ndmin_t *mn = get_minor(m);
		ndack_t *next;

		if (mn == NULL)
			continue;
		(void) close(mn->fd);
		while (mn->ack) {
			next = mn->ack->next;
			free(mn->ack);
			mn->ack = next;
		}
		log_msg(5, "nd%d removed", m);
	}
}
