#ifndef P9_TRACE_H_
#define P9_TRACE_H_

#ifdef TRACE
  #define TRACE_PRINT( format, ... ) printf( "%15.15s::%16.16s(%d) \t" format "\n", __FILE__, __FUNCTION__,  __LINE__, __VA_ARGS__ )
#else
  #define TRACE_PRINT( format, ... )
#endif


#endif
