/*
 * nd_pkt.c	-- ND packet processing
 * Copyright (C) 2015 jansen@atlas.cz
 */

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <assert.h>

#include "nd_pkt.h"
#include "minor.h"
#include "ndd.h"

/*
 * Open RAW IP socket.
 */
int
init_network()
{
	int sc;
	int one = 1;
	int *val = &one;

	sc = socket(AF_INET, SOCK_RAW, ND_IPPROTO);
	if (sc == -1) {
		log_msg(0, "Unable to open socket: %s.", strerror(errno));
		return (sc);
	}
	if (setsockopt(sc, IPPROTO_IP, IP_HDRINCL, val, sizeof (one)) == -1) {
		log_msg(0, "Unable to set IP_HDRINCL: %s.", strerror(errno));
		close(sc);
		return (-1);
	}
	return (sc);
}

/*
 * Verify incoming packet.
 */
static int
verify_pkt(ndpkt_t *p, int len)
{
	ndmin_t *m;
	long blkno = ntohl(p->np_blkno);
	long bcount = ntohl(p->np_bcount);

	if (ntohl(p->np_bcount) > ND_MAXIO) {
		log_msg(3, "bcount is too big: %lu.",
		    (unsigned long) ntohl(p->np_bcount));
		return (EIO);
	}

	if ((m = get_minor(p->np_min)) == NULL) {
		log_msg(3, "nd%d does not exist.", p->np_min);
		return (EIO);
	}

	if (m->mode == RDONLY && p->np_op & ND_OP_WRITE) {
		log_msg(3, "nd%d is read only.", p->np_min);
		return (EROFS);
	}

	if (blkno != GET_SIZE_REQ && blkno > (m->size / 512)) {
		log_msg(1, "nd%d is too small: has no blkno %lu.",
		    p->np_min, (unsigned long) blkno);
		return (EIO);
	}

	if (blkno == GET_SIZE_REQ && bcount != sizeof (uint32_t))
		return (EINVAL);

	return (0);
}

/*
 * Prepare the IP header for outgoing packet.
 */
static void
prepare_ip_header(ndpkt_t *p)
{
	struct sockaddr_in dst;

	/* IP_HDRINCL will fill the rest. */
	memcpy(&dst.sin_addr.s_addr, &p->np_ip.ip_src, sizeof (in_addr_t));
	memset(&p->np_ip, 0, sizeof (struct ip));
	p->np_ip.ip_hl = 5;
	p->np_ip.ip_v = 4;
	p->np_ip.ip_p = ND_IPPROTO;
	memcpy(&p->np_ip.ip_dst, &dst.sin_addr.s_addr, sizeof (in_addr_t));
}

/*
 * Send the reply.
 */
static int
send_packet(ndd_t *nds, ndpkt_t *p, int size, int err)
{
	struct sockaddr_in sin;

	if (err) {
		/*
		 * It appears that client ignores read replies if their
		 * size differs from what is expected. So we need
		 * to send enough data even when we hit error.
		 */
		if (p->np_op & ND_OP_READ)
			(void) memset(&p->np_data, 0, ntohl(p->np_ccount));
		p->np_op |= ND_OP_ERROR;
		p->np_error = err;
	}

	memset(&sin, 0, sizeof (struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = p->np_ip.ip_dst.s_addr;
	p->np_ip.ip_len = htons(size);

	if (sendto(nds->sc_fd, p, size, 0, (struct sockaddr *) &sin,
	    sizeof (struct sockaddr)) < 0) {
		log_msg(0, "Unable to send packet: %s", strerror(errno));
		return (1);
	}
	return (0);
}

/*
 * Read request processing.
 */
static int
serve_read(ndd_t *nds, ndpkt_t *p, int err)
{
	long caddr = 0;
	long bcount = ntohl(p->np_bcount);
	long blkno = ntohl(p->np_blkno);

	prepare_ip_header(p);

	if (blkno == GET_SIZE_REQ) {
		/* The client wants to know the size of this disk */
		if (err == 0) {
			uint32_t disk_size = htonl(get_minor_size(p->np_min));

			assert(bcount == sizeof (uint32_t));
			memcpy(&p->np_data, &disk_size, bcount);
			p->np_op |= ND_OP_DONE;
			p->np_caddr = 0;
			p->np_ccount = htonl(bcount);
			log_msg(8, "nd%d: get size request - has %d blks",
			    p->np_min, get_minor_size(p->np_min));
		} else {
			log_msg(4, "nd%d: get size request failed", p->np_min);
		}
		return (send_packet(nds, p, ND_HDRSZ + bcount, err));
	}

	/* This is normal read request. */
	assert(blkno < GET_SIZE_REQ);
	log_msg(9, "nd%d: read - blkno: %ld, count: %ld",
	    p->np_min, blkno, bcount);
	while (bcount > caddr) {
		long size, ccount, resid = bcount - caddr;
		off_t offset = blkno * 512 + caddr;

		ccount = resid > ND_MAXDATA ? ND_MAXDATA : resid;
		size = ND_HDRSZ + ccount;
		p->np_op |= ND_OP_DONE;
		p->np_caddr = htonl(caddr);
		p->np_ccount = htonl(ccount);

		if (err)
			return (send_packet(nds, p, size, err));

		ssize_t rc = pread(get_minor(p->np_min)->fd, &p->np_data,
		    (size_t) ccount, offset);
		if (rc != ccount) {
			log_msg(1, "nd%d: unable to read %ld bytes at "
			    "%ld. Got %ld, error: %d", p->np_min,
			    ccount, offset,
			    rc, errno);
			err = errno;
		}
		caddr += ccount;
		/* send reply */
		if (send_packet(nds, p, size, err))
			return (1);
	}
	return (0);
}

/*
 * Write request processing.
 */
static int
serve_write(ndd_t *nds, ndpkt_t *p, int err)
{
	long caddr = ntohl(p->np_caddr);
	long ccount = ntohl(p->np_ccount);
	long blkno = ntohl(p->np_blkno);
	off_t offset = blkno * 512 + caddr;

	log_msg(9, "nd%d: write - blkno: %ld, count: %ld",
	    p->np_min, blkno, ccount);
	if (err == 0) {
		ndmin_t *m = get_minor(p->np_min);

		if (pwrite(m->fd, &p->np_data, ccount, offset) != ccount) {
			log_msg(1, "nd%d: unable to write %ld bytes,"
			    " offset %lu: %d", p->np_min, ccount, offset,
			    errno);
			err = errno;
		}
	}

	/*
	 * Send nothing when the client does not wait for the response or
	 * the packet was invalid (and hence we pretend that we got none.
	 */
	if (!(p->np_op & ND_OP_WAIT) || err == EINVAL)
		return (0);

	/* Prepare and send the reply. */
	prepare_ip_header(p);
	p->np_op = ND_OP_WRITE | ND_OP_DONE | ND_OP_WAIT;
	p->np_caddr = htonl(caddr + ccount);

	return (send_packet(nds, p, sizeof (ndpkt_t) - sizeof (void*), err));
}

/*
 * This the main loop receiving, processing, and sending packets.
 */
int
serve(ndd_t *nds)
{
	ndpkt_t *pkt;
	int rc;

	/*
	 * Since we process only packet at time, we need just one
	 * preallocated buffer.
	 */
	if ((pkt = (ndpkt_t *)malloc(ND_MAXPKT)) == NULL) {
		log_msg(0, "Unable to allocate buffer: %s.",
		    strerror(errno));
		return (1);
	}

	do {
		ssize_t pkt_len;
		int op, err;

		/* Get packet from the network */
		pkt_len = recv(nds->sc_fd, (void *)pkt, ND_MAXPKT, 0);
		if (nds->exiting)
			break;

		if (pkt_len == -1) {
			log_msg(0, "Unable to receive packet: %s.",
			    strerror(errno));
			break;
		}

		/* Verify just received packet */
		err = verify_pkt(pkt, pkt_len);

		/* ND protocol supports only 2 operations: read and write */
		op = pkt->np_op & ND_OP_CODE;
		if (op == ND_OP_READ) {
			rc = serve_read(nds, pkt, err);
		} else if (op == ND_OP_WRITE) {
			rc = serve_write(nds, pkt, err);
		} else {
			log_msg(1, "Unknown operation %d.", op);
			rc = 1;
		}
	} while (rc == 0 && nds->exiting == 0);

	free(pkt);
	return (rc);
}
