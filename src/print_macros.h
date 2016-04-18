#ifndef WRAPRUN_SRC_PRINTMACROS_H_
#define WRAPRUN_SRC_PRINTMACROS_H_

#include <stdio.h>
#include <errno.h>

#ifdef DEBUG
#define DEBUG_PRINT(str, args...) do { printf("DEBUG: %s:%d:%s():MPI_COMM_SPLIT=%p(%d) : " str, \
 __FILE__, __LINE__, __func__, MPI_COMM_SPLIT, MPI_COMM_SPLIT, ##args); } while(0)
#else
#define DEBUG_PRINT(str, args...) do {} while (0)
#endif

#define EXIT_PRINT(str, args...) do { fprintf(stderr, "ERROR: %s:%d:%s(): " str, \
  __FILE__, __LINE__, __func__, ##args); \
  exit(EXIT_FAILURE); } while(0)

#endif
