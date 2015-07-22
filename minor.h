/*
 * minor.h	- definitions for soft states
 * Copyright (C) 2015 jansen@atlas.cz
 */

/* pending packet (but we don't use this now) */
typedef struct ndack {
	uint32_t	seq;
	uint32_t	blkno;
	uint32_t	caddr;
	time_t		last_pkt;
	struct ndack	*next;
} ndack_t;

/* softstate */
typedef struct ndmin {
	int		fd;	/* file descriptor */
	int		size;	/* disk size in bytes */
	int		mode;	/* RDONLY or WR */
	ndack_t		*ack;	/* XXX list of pending ACKs for this disk */
} ndmin_t;

ndmin_t *get_minor(int mn);
int add_ack(ndmin_t *m, ndpkt_t *p);
uint32_t get_minor_size(int mn);

#define	INI_FILE	"ndd.ini"

#define	RDONLY_STR	"RO"
#define	WR_STR		"WR"

#define	RDONLY		1
#define	WR		2
