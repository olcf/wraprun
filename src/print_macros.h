#ifndef SPH_SRC_DEBUG_H_
#define SPH_SRC_DEBUG_H_

#include <stdio.h>
#include <errno.h>

#ifdef DEBUG
#define DEBUG_PRINT(str, args...) do { printf("DEBUG: %s:%d:%s(): " str, \
 __FILE__, __LINE__, __func__, ##args); } while(0)
#else
#define DEBUG_PRINT(str, args...) do {} while (0)
#endif

#define EXIT_PRINT(str, args...) do { fprintf(stderr, "ERROR: %s:%d:%s(): " str, \
  __FILE__, __LINE__, __func__, ##args); \
  exit(EXIT_FAILURE); } while(0)

#endif
