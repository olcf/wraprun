#ifndef WRAPRUN_SRC_PRINTMACROS_H_
#define WRAPRUN_SRC_PRINTMACROS_H_

#include <stdio.h>
#include <errno.h>

#ifdef DEBUG
#define DEBUG_PRINT(str, args...) do { \
int world_rank; \
int split_rank = -1; \
PMPI_Comm_rank(MPI_COMM_WORLD, &world_rank); \
if(MPI_COMM_SPLIT != MPI_COMM_NULL) PMPI_Comm_rank(MPI_COMM_SPLIT, &split_rank); \
printf("DEBUG: %s:%d:%s():MPI_COMM_SPLIT=%04x:world_rank=%d:split_rank=%d : " str, \
 __FILE__, __LINE__, __func__, MPI_COMM_SPLIT, world_rank, split_rank, ##args); } while(0)
#else
#define DEBUG_PRINT(str, args...) do {} while (0)
#endif

#define EXIT_PRINT(str, args...) do { fprintf(stderr, "ERROR: %s:%d:%s(): " str, \
  __FILE__, __LINE__, __func__, ##args); \
  exit(EXIT_FAILURE); } while(0)

#endif
