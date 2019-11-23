/* FUNET-NJE developement test tools -- sizeof.c */

#include <stdio.h>
#include "consts.h"
#include "headers.h"

main()
{
/* struct SIGNON
struct ENQUIRE
struct NEGATIVE_ACK
struct SIGN_OFF
struct PERMIT_FILE
struct NMR_MESSAGE */



  printf("sizeof(struct TTB) = %d, claimed size=%d\n",
	 sizeof(struct TTB), TTB_SIZE);
  printf("sizeof(struct TTR) = %d, claimed size=%d\n",
	 sizeof(struct TTR), TTR_SIZE);
  printf("sizeof(struct VMctl) = %d, claimed size=%d\n",
	 sizeof(struct VMctl), VMctl_SIZE);

  printf("sizeof(struct JOB_HEADER) = %d\n",
	 sizeof(struct JOB_HEADER));
  
  printf("sizeof(struct DATASET_HEADER) = %d\n",
	 sizeof(struct DATASET_HEADER));
  
  printf("sizeof(struct JOB_TRAILER) = %d\n",
	 sizeof(struct JOB_TRAILER));
  
  printf("sizeof(struct ROUTE_DATA) = %d\n",
	 sizeof(struct ROUTE_DATA));
  
  return 0;
}
