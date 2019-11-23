/* HEADERS.C    V1.6-mea
 | Copyright (c) 1988,1989,1990,1991,1992,1993 by
 | The Hebrew University of Jerusalem, Computation Center.
 |
 |   This software is distributed under a license from the Hebrew University
 | of Jerusalem. It may be copied only under the terms listed in the license
 | agreement. This copyright message should never be changed or removed.
 |   This software is gievn without any warranty, and the Hebrew University
 | of Jerusalem assumes no responsibility for any damage that might be caused
 | by use or misuse of this software.
 |
 | All the headers and fixed transmission blocks used by NJE.
 | The structure fields are defined in HEADERS.H
 | Currently, all records, lines, etc counts are intialized to the default.
 | Currently, the program supports only NJH, DSH & NJT control records.
 | WARNING:  The headers contain uninitialized fields. These fields are
 |           marked with M in front of the lines. The various routines change
 |           these fields inside the skeleton structure (to reduce run time).
 |           Hence, each routine MUST initialize these values.
 */
#include "consts.h"
#include "headers.h"
#include "prototypes.h"

/* The Enquiry block */
INTERNAL struct	ENQUIRE		Enquire = { SOH, ENQ, PAD };

/* The negative ACK block */
INTERNAL struct	NEGATIVE_ACK	NegativeAck = { NAK, PAD };

/* The positive ACK */
INTERNAL struct	POSITIVE_ACK	PositiveAck = { DLE, ACK0, PAD };

/* The signoff record */
INTERNAL struct	SIGN_OFF	SignOff = { 0xf0, E_B, PAD };

/* The end of block record */
/* struct	EOF_BLOCK	EOFblock = { SYSOUT_0, CC_NO_SRCB, 0, 0 }; */
INTERNAL struct	EOF_BLOCK	EOFblock = { SYSOUT_0, CC_NO_SRCB, 0, 0 };

/* File xmit permission/rejection: */
INTERNAL struct	PERMIT_FILE	PermitFile = { PERMIT_RCB, SYSOUT_0, 0, 0};
INTERNAL struct	REJECT_FILE	RejectFile = { CANCEL_RCB, SYSOUT_0, 0, 0};
INTERNAL struct	COMPLETE_FILE	CompleteFile = { COMPLETE_RCB, SYSOUT_0, 0, 0};

/* The job header */
INTERNAL struct	JOB_HEADER	NetworkJobHeader = {
			0,
			0, 0,
			0,
			0, 0,
			0,		/* Job ID */
			E_A,		/* Job class */
			E_A,		/* Message class */
			0,		/* Flag */
			7,		/* Priority */
			1,		/* Origin system qualifier */
			1,		/* Job copies */
			0,		/* Lines per page (default) */
			0,0,0,		/* Reserved */
			"", "", "", "", "",
			{0,0},		/* Time */
			"", "", "", "", "",
			"", "", "", "",
			0, 0, 0, 0,	/* lines, records, etc */
			"", "", "", "",
			0		/* Records count */
		};

/* The dataset header */
INTERNAL struct	DATASET_HEADER	NetworkDatasetHeader = {
			0,
			0, 0,
		{
			/* General section */
			0,
			0, 0,
			"", "", "", "", "",
			1, 0,
			E_A,			/* Class (A =files, M = Mail */
			0, 0,
			0x80,			/* Record format */		
			0,
			1,			/* NDHGDSCT */
			0, 0,
			0,			/* Reserved */
			"", "", "", "",
			"",			/* Reserved */
			0x40, 0, "",
			""
		}, {
			/* RSCS section */
			0,
			0x87, 0,
/* M */			0, E_A,		/* Job class */
/* M */			0x82,		/* CP device type */
			0,
/* M */			"", "", "",
			0,
			RSCS_VERSION, RSCS_RELEASE,
			""
		}
	};

/* The job trailer */
INTERNAL struct	JOB_TRAILER	NetworkJobTrailer = {
			0,
			0, 0,
			0,
			0, 0,
			0, E_A, { 0,0 },
			{ 0,0 }, { 0,0 }, 0,
			0,		/* Number of lines */
			0,		/* Number of crads */
			0,
			0, 0, 0, 0
		} ;

/* The initial signon record */
INTERNAL struct	SIGNON	InitialSignon = {
		0xf0,		/* RCB */
		E_I,		/* SRCB */
		0x25,
		"",		/* Node name */
		1,
		0, 0,
		0,		/* Buffer size */
		"", "",		/* Passwrods */
		0, 0		/* Features are none... */
	};


/* The response signon record */
INTERNAL struct	SIGNON	ResponseSignon = {
		0xf0,		/* RCB */
		E_J,		/* SRCB */
		0x25,
		"",		/* Node name */
		0x1,
		0xffffffff, 0,
		0,		/* Buffer size */
		"", "",		/* Passwrods */
		0
	};

void
init_headers()
{
  NetworkJobHeader.LENGTH   = htons(sizeof(struct JOB_HEADER));
  NetworkJobHeader.LENGTH_4 = htons(sizeof(struct JOB_HEADER) - 4);
  NetworkJobTrailer.LENGTH = htons(sizeof(struct JOB_TRAILER));
  NetworkJobTrailer.LENGTH_4 = htons(sizeof(struct JOB_TRAILER) - 4);
}
