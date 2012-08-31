

#include "config.h"
#include "debug.h"
#include "ihpp.h"
#include "benchmark.h"
#include "output.h"
#include "tracingFuncs.h"

using namespace std;


#define INTRAMODE_LOAD_TOP_BOTTOM()					treeTop = intraCtx->shadowStack.top().treeTop; treeBottom = intraCtx->shadowStack.top().treeBottom; 

#if ENABLE_KEEP_STACK_PTR
#define INTRAMODE_STORE_TOP_BOTTOM()				intraCtx->shadowStack.push(ShadowStackItemType(treeTop,treeBottom,reg_sp));
#define INTRAMODE_REPLACE_TOP_BOTTOM()				intraCtx->shadowStack.pop(); intraCtx->shadowStack.push(ShadowStackItemType(treeTop,treeBottom,oldStackPtr));
#else
#define INTRAMODE_STORE_TOP_BOTTOM()				intraCtx->shadowStack.push(ShadowStackItemType(treeTop,treeBottom));
#define INTRAMODE_REPLACE_TOP_BOTTOM()				intraCtx->shadowStack.pop(); intraCtx->shadowStack.push(ShadowStackItemType(treeTop,treeBottom));
#endif
#define TOP_STACKPTR()								(intraCtx->shadowStack.size()?intraCtx->shadowStack.top().stackPtr:(ADDRINT)-1)

#define INTRAMODE_SET_TOP_BOTTOM_TO_ROOT()			intraCtx->counter=1; treeTop=intraCtx->kSlabForest.getTreeRef(intraCtx->rootKey); treeBottom=0;
#define INTRAMODE_TOP_BOTTOM_ARE_POINTING_TO_ROOT()	(treeTop==intraCtx->kSlabForest.getTreeRef(intraCtx->rootKey) && !treeBottom)


void intraModeBlockTrace(BasicBlock *bb

#if ENABLE_KEEP_STACK_PTR
	, ADDRINT reg_sp
#endif
) { 

	GlobalContext *globalCtx = globalSharedContext;
	ThreadContext *ctx;
	IntraModeContext *intraCtx;
	ihppNode *treeTop;
	ihppNode *treeBottom;

	bb->incSimpleCounter();

	ctx = globalCtx->getThreadCtx(PIN_ThreadUid());


	if (!ctx->haveToTrace)
		return;

	if ( FUNC_IS_TEXT(bb->functionAddr()) )
		return;

#ifdef _WIN32

	if (IS_WIN32_NLG_NOTIFY(bb->functionAddr())) {

		dbg_intratr_nlog_skip();
		return;
	}

#endif

	dbg_intratr_begin();

#if ENABLE_RELY_ON_SP_CHECK

	ctx->setCurrentFunction(bb->functionAddr());

#else

	if ( ctx->getCurrentFunction() && ctx->getCurrentFunction() != bb->functionAddr() ) {

		dbg_intratr_longjmp();

		intraMode_ret();
		ctx->setCurrentFunction(bb->functionAddr());
	}

#endif	

	intraCtx = ctx->getCurrentFunctionCtx();

	dbg_intratr_begin_sp();

	if (!bb->isFirstBlock()) {

		//If this function wasn't traced before (due to startFunc), don't it trace now..
		if (!intraCtx->shadowStack.size())
			return;

		dbg_intratr_normal_trace();

		INTRAMODE_LOAD_TOP_BOTTOM();

#if ENABLE_KEEP_STACK_PTR
		ADDRINT oldStackPtr = TOP_STACKPTR();
#endif

		if (!globalCtx->options.rollLoops) {

			traceObject(bb, intraCtx, treeTop, treeBottom);

		} else {

			bool found=false;
			ihppNode *parent = treeTop->getParentRef();

			while (parent) {


				if (parent->getKey() == bb->getKey()) {

					treeTop=parent;
					treeBottom=0;
					
					//in rollLoops mode, k=inf is automatic
					//intraCtx->counter=1;

					found=true;
					treeTop->incCounter();
					break;

				}

				parent = parent->getParentRef();
			}

			if (!found) {

				traceObject(bb, intraCtx, treeTop, treeBottom);
			} 
		}

		INTRAMODE_REPLACE_TOP_BOTTOM();

		dbg_intratr_end_sp();

		return;
	}

	/*
	First block of a function is met: it has the SAME address as the function.
	This happens where there is a RECURSION or a NORMAL CALL of the function (maybe also the first time)
	*/
	dbg_intratr_first_block();



	if (!intraCtx->rootKey) {

		/*
		Rootkey is null, so this is the first the function is called (in this thread): everything is very simple.
		*/

		dbg_intratr_first_call();

		treeTop=0; treeBottom=0;

		//here accumulate was false
		traceObject(bb, intraCtx, treeTop, treeBottom);

		assert(intraCtx->rootKey);
		assert(treeTop);

		INTRAMODE_STORE_TOP_BOTTOM();
		return;
	}


	//This was NOT the first time something called this function.

	INTRAMODE_LOAD_TOP_BOTTOM();

	if (!INTRAMODE_TOP_BOTTOM_ARE_POINTING_TO_ROOT()) 
	{
		//Reset top,bottom pointers to root of the function's tree
		INTRAMODE_SET_TOP_BOTTOM_TO_ROOT();

		//Push them on the shadow stack and increment the root counter
		INTRAMODE_STORE_TOP_BOTTOM();
	}

	treeTop->incCounter();

	dbg_intratr_end_sp();
}



void intraMode_ret() 
{
	ThreadContext *ctx;
	IntraModeContext *intraCtx;

	GlobalContext *globalCtx = globalSharedContext;
	ctx = globalCtx->getThreadCtx(PIN_ThreadUid());


	intraCtx = ctx->getCurrentFunctionCtx();


#ifdef _WIN32
	if (IS_WIN32_NLG_NOTIFY(ctx->getCurrentFunction()))
		return;
#endif

	dbg_intraret_begin();

	if (!intraCtx->shadowStack.size()) {

		dbg_intraret_cantpop();
		return;
	}

	dbg_intraret_stackpop();

	intraCtx->shadowStack.pop();

	if (!intraCtx->shadowStack.size()) {

		ihppNode *treeTop;
		ihppNode *treeBottom;

#if ENABLE_KEEP_STACK_PTR
		ADDRINT reg_sp = (ADDRINT)-1;
#endif

		dbg_intraret_lastret();

		/*
		Since now shadow stack is empty, on the next activation of the function
		top and bottom pointers MUST point to the calling tree's root.
		*/

		//Reset top,bottom pointers to root of the function's tree
		INTRAMODE_SET_TOP_BOTTOM_TO_ROOT();

		//Push them on the shadow stack a increment the root counter
		INTRAMODE_STORE_TOP_BOTTOM();		
	}
}
