#ifndef _PUB_ERROR_H_
#define _PUB_ERROR_H_

/* kernel error codes -- keep in sync with list in lib/printfmt.c */
#define E_UNSPECIFIED        1    // unspecified or unknown problem
#define E_BAD_PROC           2    // process doesn't exist or otherwise
#define E_INVAL              3    // invalid parameter
#define E_NO_MEM             4    // request failed due to memory shortage
#define E_NO_FREE_PROC       5    // attempt to create a new process beyond
#define E_FAULT              6    // memory fault

/* the maximum error allowed */
#define MAXERROR             6

#endif
