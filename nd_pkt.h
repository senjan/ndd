/*
 * nd_pkt.h	-- ND packet definitions
 * Copyright (C) 2015 jansen@atlas.cz
 */

/*
 * Following definitions are based on ND(4P) man pages found on Internet.
 */
typedef struct ndpkt {
	struct ip np_ip;	/* IP header */
	u_char	np_op;		/* operation code, see below */
	u_char	np_min;		/* minor device */
	int8_t	np_error;	/* b_error */
	int8_t	np_ver;		/* version number */
	int32_t	np_seq;		/* sequence number */
	int32_t	np_blkno;	/* b_blkno, disk block number */
	int32_t	np_bcount;	/* b_bcount, byte count */
	int32_t	np_resid;	/* b_resid, residual byte count */
	int32_t	np_caddr;	/* current byte offset of this packet */
	int32_t	np_ccount;	/* current byte count of this packet */
	void	*np_data;	/* data */
} ndpkt_t;

/* np_op operation codes */
#define	ND_OP_READ	1	/* read */
#define	ND_OP_WRITE	2	/* write */
#define	ND_OP_ERROR	3	/* error flag, see error in np_error */
#define	ND_OP_CODE	7	/* op code mask */
#define	ND_OP_WAIT	010	/* waiting for DONE or next request (flag) */
#define	ND_OP_DONE	020	/* operation done (flag) */

/* misc protocol defines */
#define	ND_MAXDATA	1024	/* max data per packet */
#define	ND_MAXIO	63*1024	/* max np_bcount */
#define	ND_IPPROTO	77	/* IP protocol number */
#define	ND_HDRSZ	(sizeof (ndpkt_t) - sizeof (void *))
				/* header size */
#define	ND_MAXPKT	(ND_HDRSZ + ND_MAXDATA)
				/* max packet size */
#define	MAX_MINOR	4	/* max number of disks */
/*
 * ND(4P) does not provide support for any ioctl() operation directly, but
 * client can use a reserved value of np_blkno to obtain some information.
 * As far as I know, this is not documented anywhere so the following is based
 * only on my observation.
 */

/*
 * SunOS 3.5 sends ND_OP_READ for the following blkno for np_min 1 during
 * the boot. I suppose it's trying to determine the size of swap/dump device.
 * Server seems to be expected to reply with the device size (in 512 blocks)
 * stored in np_data (in network order).
 * If client dislikes the returned size, it panics with: "panic: vstodb".
 */
#define	GET_SIZE_REQ	0x10000000
