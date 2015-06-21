#ifndef SPH_SRC_DEBUG_H_
#define SPH_SRC_DEBUG_H_

#include <errno.h>

#ifdef DEBUG
#define DEBUG_PRINT(str, args...) printf("DEBUG: %s:%d:%s(): " str, \
 __FILE__, __LINE__, __func__, ##args)
#else
#define DEBUG_PRINT(str, args...) do {} while (0)
#endif

#define EXIT_PRINT(str, args...) fprintf(stderr, "ERROR: %s:%d:%s(): " str, \
 __FILE__, __LINE__, __func__, ##args); exit(EXIT_FAILURE)

#endif
