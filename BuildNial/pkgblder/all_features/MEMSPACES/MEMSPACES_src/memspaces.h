/**
 * Alternative macros for
 *
 */

#ifdef OSX
/* MAP_ANONYMOUS is undefined for OSX */
#ifndef MAP_ANONYNOUS
#define MAP_ANONYMOUS   0
#endif
#endif


/**
 * Types of data
 */
#define MSP_INTTYPE       1
#define MSP_BOOLTYPE      2
#define MSP_CHARTYPE      3
#define MSP_REALTYPE      4


/**
 * Different types of memory spaces
 */
#define MSP_LOCAL           0
#define MSP_SHARED          1



/**
 * Memory space definition
 */
typedef struct {
  int            mtype;          /* type of memory space */
  void          *handle;         /* OS specific handle */
  nialint       *mbase;          /* base address for memory */
  nialint        msize;          /* No of bytes of memory */
} MemSpace, *MemSpacePtr;
