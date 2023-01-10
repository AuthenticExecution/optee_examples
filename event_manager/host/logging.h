#ifndef __LOGGING_H__
#define __LOGGING_H__

#include "defines.h"

#define INFO(...) do{ printf("INFO: "); printf(__VA_ARGS__ ); printf("\n"); } while(0)
#define WARNING(...) do{ printf("WARNING: "); printf(__VA_ARGS__ ); printf("\n"); } while(0)
#define ERROR(...) do{ printf("ERROR: "); printf(__VA_ARGS__ ); printf("\n"); } while(0)

#ifdef DBG
#define DEBUG(...) do{ printf("DEBUG: "); printf(__VA_ARGS__ ); printf("\n"); } while(0)
#else
# define DEBUG(...) do {} while (0)
#endif

#endif