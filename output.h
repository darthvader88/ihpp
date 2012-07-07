
#ifndef __OUTPUT_HEADER__
#define __OUTPUT_HEADER__

#include <iomanip>
#include "util.h"

VOID Fini(INT32 code, VOID *v);
void funcTraceDebugDump(ihppContextClass *globalCtx, FunctionObj *fc, 
						ihppThreadContextClass *ctx, ADDRINT reg_sp, 
						ihppNode *treeTop, ihppNode *treeBottom);

#endif