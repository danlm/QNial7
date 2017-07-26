/**
 * Primitives for synchronisation across multiple processes
 */



#ifdef LINUX
#define AtomicCompareAndSwap(ptr, oldval, newval) \
  __sync_bool_compare_and_swap(ptr, oldval, newval)

#define AtomicIncrement(ptr) \
  __sync_add_and_fetch(ptr,(nialint)1)

#define AtomicDecrement(ptr) \
  __sync_sub_and_fetch(ptr,(nialint)1)

#endif

#ifdef OSX
#include <libkern/OSAtomic.h>

#ifdef INTS32
#define AtomicCompareAndSwap(ptr, oldval, newval) \
  OSAtomicCompareAndSwap32Barrier((int32_t)oldval,(int32_t)newval,(int32_t*)ptr)

#define AtomicIncrement(ptr,val) \
  OSAtomicIncrement32Barrier((int32_t*)ptr)

#define AtomicDecrement(ptr,val) \
  OSAtomicDecrement32Barrier((int32_t*)ptr)

#endif
#ifdef INTS64
#define AtomicCompareAndSwap(ptr,oldval,newval) \
  OSAtomicCompareAndSwap64Barrier((int64_t)oldval,(int64_t)newval,(int64_t*)ptr)

#define AtomicIncrement(ptr,val) \
  OSAtomicIncrement64Barrier((int64_t*)ptr)

#define AtomicDecrement(ptr,val) \
  OSAtomicDecrement64Barrier((int64_t*)ptr)

#endif
#endif


