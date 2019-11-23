/*  unix_msgs.h  -- FUNET-NJE NMR exit constants & structs */

enum msg_exit_actions {
	msg_discard,
	msg_inetudp
};

struct msg_exit_req {
	short		size;		/* Size of this request		*/
	char		regflag;	/* 0: deregister, 1: register	*/
	char		userid[9];	/* Userid (8 chars)		*/
	enum msg_exit_actions	action;	/* method			*/
	union {
	  char	none;
	  struct {
	    unsigned long key;
	    pid_t remotepid;
	    struct sockaddr_in addr;
	  } udp;
	} u;
};

struct msg_package {
	unsigned long key;
	short	size;
	char	fromnode[9],
		fromuser[9],
		tonode[9],
		touser[9];
	char	type;		/* 'A', 'C', or 'M', or 'T' -- Time-stamped 'M' */
	char	message[1]; /* N chars.. */
};
