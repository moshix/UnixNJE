/* BCB_CRC.C   V1.2
 | Copyright (c) 1988,1989 by
 | The Hebrew University of Jerusalem, Computation Center.
 |
 |   This software is distributed under a license from the Hebrew University
 | of Jerusalem. It may be copied only under the terms listed in the license
 | agreement. This copyright message should never be changed or removed.
 |   This software is gievn without any warranty, and the Hebrew University
 | of Jerusalem assumes no responsibility for any damage that might be caused
 | by use of misuse of this software.
 |
 | Add the BCB and FCS to sending messages. Also compute CRC for incoming
 | and outgoing messages.
 | 1. The CRC computing algorithm should be enhanced.
 |
 | The generating polynomial is X^16+X^15+X^2+1 (CRC-16). When computing the
 | CRC, DLE's are not computed (except from a second DLE in a sequence of
 | 2 DLE's). Furthermore, the first DLE+ETB which starts a text block is
 | not computed also.
 |   When a BCB is sent, first it is checked whether we have to send a reset
 | BCB. If so, we send a reset BCB (instead of normal one), and this flag is
 | set, so the next BCB will be a normal one. This flag is set to zero when
 | a line is (re)started, so the first BCB will be a reset one.
 |   When adding BCB and CRC to the buffer, DLE's are replicated. Upon receive,
 | the routine that computes the CRC removes the second DLE and rfeturns the
 | new size to the caller.
 |
 | V1.1 - Add the functions REMOVE_DLES (same as CHECK-CRC but doesn't do the
 |        CRC checking) and the function ADD_BCB (line ADD_BCB_CRC but does
 |        not add the CRC).
 | V1.2 - Modify ADD_BCB function. When we pass a frame to DMB, we must leave
 |        enough space (for CRCs) in the length of buffer we pass to DMB.
 |        However, enough should be = exactly, and we should not reserve place
 |        for the last PAD. Hence, the size of buffer was decreased by one in
 |        this change (to not give place for the PAD character).
 */
#include "consts.h"
#include "headers.h"
#include "prototypes.h"
#include "ebcdic.h"

EXTERNAL struct	LINE IoLines[ABSMAX_LINES];

/* A naive bit-shifts algoruthm (not used):
#define ACUCRC(crc, c) {\
	register unsigned t0, t1;\
	for(t0 = 0; t0 < 8; t0++) {\
		if(((crc & 0x1) ^ ((c >> t0) & 0x1)) == 0) \
			crc = ((crc >> 1) & 0x7fff);\
		else\
			crc = ((((crc & 0xbffd) | ((crc & 0x4002) ^ 0x4002)) >> \
				1) & 0x7fff) | 0x8000;\
	};\
}
*/


unsigned char cones[256] =
{
    0x0,  0xFF, 0xFE, 0x1,  0xFC, 0x3,  0x2,  0xFD,
    0xF8, 0x7,  0x6,  0xF9, 0x4,  0xFB, 0xFA, 0x5,
    0xF0, 0x0F, 0x0E, 0xF1, 0xC,  0xF3, 0xF2, 0xD,
    0x8,  0xF7, 0xF6, 0x9,  0xF4, 0xB,  0xA,  0xF5,
    0xE0, 0x1F, 0x1E, 0xE1, 0x1C, 0xE3, 0xE2, 0x1D,
    0x18, 0xE7, 0xE6, 0x19, 0xE4, 0x1B, 0x1A, 0xE5,
    0x10, 0xEF, 0xEE, 0x11, 0xEC, 0x13, 0x12, 0xED,
    0xE8, 0x17, 0x16, 0xE9, 0x14, 0xEB, 0xEA, 0x15,
    0xC0, 0x3F, 0x3E, 0xC1, 0x3C, 0xC3, 0xC2, 0x3D,
    0x38, 0xC7, 0xC6, 0x39, 0xC4, 0x3B, 0x3A, 0xC5,
    0x30, 0xCF, 0xCE, 0x31, 0xCC, 0x33, 0x32, 0xCD,
    0xC8, 0x37, 0x36, 0xC9, 0x34, 0xCB, 0xCA, 0x35,
    0x20, 0xDF, 0xDE, 0x21, 0xDC, 0x23, 0x22, 0xDD,
    0xD8, 0x27, 0x26, 0xD9, 0x24, 0xDB, 0xDA, 0x25,
    0xD0, 0x2F, 0x2E, 0xD1, 0x2C, 0xD3, 0xD2, 0x2D,
    0x28, 0xD7, 0xD6, 0x29, 0xD4, 0x2B, 0x2A, 0xD5,
    0x80, 0x7F, 0x7E, 0x81, 0x7C, 0x83, 0x82, 0x7D,
    0x78, 0x87, 0x86, 0x79, 0x84, 0x7B, 0x7A, 0x85,
    0x70, 0x8F, 0x8E, 0x71, 0x8C, 0x73, 0x72, 0x8D,
    0x88, 0x77, 0x76, 0x89, 0x74, 0x8B, 0x8A, 0x75,
    0x60, 0x9F, 0x9E, 0x61, 0x9C, 0x63, 0x62, 0x9D,
    0x98, 0x67, 0x66, 0x99, 0x64, 0x9B, 0x9A, 0x65,
    0x90, 0x6F, 0x6E, 0x91, 0x6C, 0x93, 0x92, 0x6D,
    0x68, 0x97, 0x96, 0x69, 0x94, 0x6B, 0x6A, 0x95,
    0x40, 0xBF, 0xBE, 0x41, 0xBC, 0x43, 0x42, 0xBD,
    0xB8, 0x47, 0x46, 0xB9, 0x44, 0xBB, 0xBA, 0x45,
    0xB0, 0x4F, 0x4E, 0xB1, 0x4C, 0xB3, 0xB2, 0x4D,
    0x48, 0xB7, 0xB6, 0x49, 0xB4, 0x4B, 0x4A, 0xB5,
    0xA0, 0x5F, 0x5E, 0xA1, 0x5C, 0xA3, 0xA2, 0x5D,
    0x58, 0xA7, 0xA6, 0x59, 0xA4, 0x5B, 0x5A, 0xA5,
    0x50, 0xAF, 0xAE, 0x51, 0xAC, 0x53, 0x52, 0xAD,
    0xA8, 0x57, 0x56, 0xA9, 0x54, 0xAB, 0xAA, 0x55
};

/*
 | Macro to calculate CRC-16
 */
#define ACUCRC(crc, c) {\
	register unsigned t0, t1;\
	t0 = cones[c] ^ cones[crc & 0xff];\
	crc >>= 8;\
	t1 = (crc >> 6) & 03;\
	crc &= 0x3f;\
	t1 |= t0 << 2;\
	t1 ^= t0;\
	crc |= t1 << 6;\
	crc |= (t0 & 0xc0) << 8;\
	crc ^= (crc >> 15) & 1;}



/*
 | Check the CRC. First check that there is an ETB 2 characters before the
 | end of the transmission.  If so, calculate the CRC and compare it to the
 | received one. return 1 if ok, 0 if not.
 | If there are two DLE's in a sequence, remove one (the first is for
 | transparency) and reduce the buffer size by one.
 */
int
check_crc(buffer, size)
void	*buffer;
int	*size;
{
	unsigned char	c;
	unsigned char *p, *q;
	unsigned short crc, rcrc;
	register int	DleFound, AccumulatedSize;

	crc = 0;
	q = p = &((unsigned char *)buffer)[2];	/* Skip first DLE+STX */
	DleFound = AccumulatedSize = 0;
	for (;;) {
	  c = *p++;	/* Use direct mode instead of indirect, and
			   also save us incrementing problems later. */
	  if ((c != DLE) || (DleFound != 0)) {
	    /* Accumulate only the second DLE */
	    ACUCRC(crc, c);
	  }
	  if ((c == DLE) && (DleFound != 0)) {
	    /* Remove the second DLE - Reduce size and
	       don't increment *q */
	    *size -= 1;
	    AccumulatedSize--;
	  } else			/* Retain it */
	    *q++ = c;
	  if ((c == ETB) && (DleFound))
	    break;		/* End of the block */
	  /* Mark that we have a DLE only if it is first in a sequence (and
	     not Second in a serias of 4 DLE's which should be made to 2) */
	  if ((c == DLE) && (DleFound == 0))
	    DleFound++;
	  else
	    DleFound = 0;
	  if (++AccumulatedSize > *size) { /* Unterminated block */
	    logger(2, "BCB_CRC, Unterminated block received\n");
	    trace(buffer, AccumulatedSize, 2);
	    return 0;
	  }
	}

	/* Compute now the received CRC and compare */

	rcrc = (*p++ & 0xff);
	rcrc += ((*p & 0xff) << 8);
	
	if (crc == rcrc)		/* Match */
	  return 1;

	logger(2,
	       "BCB_CRC: Received CRC error, received=x^%x, computed=x^%x\n",
	       rcrc, crc);
	return 0;
}

/*
 | In case of DMB working in BISYNC mode, the DMB does the CRC checking.
 | However, it doesn't strip out double DLE's, so we have to do it.
 | If there are two DLE's in a sequence, remove one (the first is for
 | transparency) and reduce the buffer size by one.
 */
int
remove_dles(buffer, size)
void	*buffer;
int	*size;
{
	unsigned char	c, *p, *q;
	register int	DleFound, AccumulatedSize;

	q = p = &((unsigned char *)buffer)[2];	/* Skip first DLE+STX */
	DleFound = AccumulatedSize = 0;
	for (;;) {
	  c = *p++;	/* Use direct mode instead of indirect, and
			   also save us incrementing problems later. */
	  if ((c == DLE) && (DleFound != 0)) {
	    /* Remove the second DLE - Reduce size and don't increment *q */
	    *size -= 1;
	    AccumulatedSize--;
	  } else	/* Retain it */
	    *q++ = c;
	  if (c == ETB && DleFound)
	    break;	/* End of the block */
	  /* Mark that we have a DLE only if it is first in a sequence (and
	     not Second in a serias of 4 DLE's which should be made to 2) */
	  if (c == DLE && !DleFound)
	    DleFound++;
	  else
	    DleFound = 0;
	  if(++AccumulatedSize > *size) {	/* Unterminated block */
	    logger(2, "BCB_CRC, Unterminated block received\n");
	    trace(buffer, AccumulatedSize, 2);
	    return 0;
	  }
	}
	return 1;
}


/*
 | Add DLE+STX+BCB+FCS in front of the transmission block, compute the CRC,
 | and append it to the end of the trasmission block. The complete output is
 | placed in NewLine (buffer is not changed). The size of the resultant buffer
 | is returned as the function value.
 | This function does not check whether there is enough place in the output
 | buffer.
 */
int
add_bcb_crc(Index, buffer, size, NewLine, BCBcount)
const void	*buffer;
      void	*NewLine;
const int	Index, size, BCBcount;
{
	unsigned char	*p, c;
	const unsigned char *q;
	int	i, NewSize;
	unsigned short	crc;

	NewSize = 0;  p = NewLine; crc = 0;

	/* Put the DLE, STX, BCB and FCS.
	   If BCB is zero, send a "reset" BCB. */

	*p++ = DLE; *p++ = STX;

	if ((BCBcount == 0) &&
	    ((IoLines[Index].flags & F_RESET_BCB) == 0)) {
	  IoLines[Index].flags |= F_RESET_BCB;
	  *p++ = 0xa0;	/* Reset BCB count to zero */
	} else
	  *p++ = 0x80 + (BCBcount & 0xf);	/* Normal block */

	*p++ = 0x8f; *p++ = 0xcf;	/* FCS - all streams are enabled */
	NewSize = 5;			/* DLE+STX+BCB+FCS1+FCS2 */
	p = &((unsigned char*)NewLine)[2];  /* Compute the CRC of the above */
	for (i = 2; i < NewSize; i++) {	/* Skip the DLE STX */
	  c = *p++;	/* Use direct mode instead of indirect, and
			   also save us incrementing problems later. */
	  ACUCRC(crc, c);
	}

	/* Copy the data, compute the CRC during the copy,
	   and replicate DLE if needed */

	q = buffer;
	for (i = 0; i < size; i++) {
	  c = *q++;
	  ACUCRC(crc, c);
	  *p++ = c;
	  if (c == DLE) {
	    *p++ = DLE;
	    NewSize++;
	  }
	}
	NewSize += size;

	/* Add DLE+ETB and the CRC, and finaly the PAD character */

	*p++ = DLE; *p++ = ETB;
	ACUCRC(crc, ETB);
	*p++ = (crc & 0xff);
	*p++ = ((crc & 0xff00) >> 8);
	*p++ = PAD;
	NewSize += 5;

	return NewSize;
}


/*
 | Add DLE+STX+BCB+FCS in front of the transmission block, and leave space for
 | CRCs at the end of the block. The complete output is placed in NewLine
 | (buffer is not changed). The size of the resultant buffer is returned as
 | the function value.
 | This function does not check whether there is enough place in the output
 | buffer.
 */
int
add_bcb(Index, buffer, size, NewLine, BCBcount)
const void	*buffer;
      void	*NewLine;
const int	Index, size, BCBcount;
{
	unsigned char	*p, c;
	const unsigned char *q;
	int	i, NewSize;

	NewSize = 0;  p = NewLine;

	/* Put the DLE, STX, BCB and FCS.
	   If BCB is zero, send a "reset" BCB.  */

	*p++ = DLE; *p++ = STX;
	if ((BCBcount == 0) &&
	    ((IoLines[Index].flags & F_RESET_BCB) == 0)) {
	  IoLines[Index].flags |= F_RESET_BCB;
	  *p++ = 0xa0;	/* Reset BCB count to zero */
	} else
	  *p++ = 0x80 + (BCBcount & 0xf);	/* Normal block */

	*p++ = 0x8f; *p++ = 0xcf;	/* FCS - all streams are enabled */
	NewSize = 5;	/* DLE+STX+BCB+FCS1+FCS2 */

	/* Copy the data, replicate DLE if needed */

	q = buffer;
	for (i = 0; i < size; i++) {
	  c = *q++;
	  *p++ = c;
	  if(c == DLE) {
	    *p++ = DLE;
	    NewSize++;
	  }
	}
	NewSize += size;

	/* Add DLE+ETB and leave space for CRC's but not for PAD
	   (if we leave place for the PAD also, the DMB gets confused
	   and adds garbage after the frame) */

	*p++ = DLE; *p++ = ETB;
	NewSize += 4;	/* DEL+ETB, 2 for CRCs */

	return NewSize;
}
