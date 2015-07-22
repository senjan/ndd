/*
 * ndd.h
 * Copyright (C) 2015 jansen@atlas.cz
 */

#define	VERSION	"ndd v1.0"

typedef struct ndd {
	int sc_fd;
	int lf_fd;
	short log_level;
	short exiting;
	short is_daemon;
	short simple_ack;
	char *lf;
} ndd_t;

int load_config(const char *, ndd_t *);
int init_network();
int serve(ndd_t *);
void close_minors();
void log_msg(int level, const char *fmt, ...);

#define	MAX_LOG_LEN	100

#ifdef __sun
#define	DEFAULT_LOCK_FILE	"/var/spool/locks/ndd.lock"
#else
#define	DEFAULT_LOCK_FILE	"/var/lock/ndd.lock"
#endif
