#include <mruby.h>
#include <mruby/string.h>
#include <mruby/compile.h>
#include <string.h>
#include "binding-util.h"

#include <cocos2d.h>
using namespace cocos2d;

#include "marshal.h"
#include "../RMSceneCache.h"

MRB_FUNCTION(kernelEval)
{
	const char *exp;
	mrb_int expLen;
	mrb_get_args(mrb, "s", &exp, &expLen);

	return mrb_load_nstring(mrb, exp, expLen);
}

static void printP(mrb_state *mrb,
                   const char *conv, const char *sep)
{
	mrb_int argc;
	mrb_value *argv;
	mrb_get_args(mrb, "*", &argv, &argc);

	mrb_value buffer = mrb_str_buf_new(mrb, 128);
	mrb_value arg;

	for (int i = 0; i < argc; ++i)
	{
		arg = argv[i];
		arg = mrb_funcall(mrb, arg, conv, 0);

		mrb_str_buf_append(mrb, buffer, arg);

		if (i < argc)
			mrb_str_buf_cat(mrb, buffer, sep, strlen(sep));
	}

	CCLOG(RSTRING_PTR(buffer));
}

MRB_FUNCTION(kernelP)
{
	printP(mrb, "inspect", "\n");

	return mrb_nil_value();
}

MRB_FUNCTION(kernelPrint)
{
	printP(mrb, "to_s", "");

	return mrb_nil_value();
}

// FIXME store this in kernel module
static int currentSeed = 1;
bool srandCalled = false;

static void
srandCurrentTime(int *currentSeed)
{
	timeval tv;
	gettimeofday(&tv, 0);

	// FIXME could check how MRI does this
	*currentSeed = tv.tv_sec * tv.tv_usec;

	srand(*currentSeed);
}

MRB_FUNCTION(kernelRand)
{
	if (!srandCalled)
	{
		srandCurrentTime(&currentSeed);
		srandCalled = true;
	}

	mrb_value arg;

	mrb_get_args(mrb, "o", &arg);

	if (mrb_fixnum_p(arg) && mrb_fixnum(arg) != 0)
	{
		return mrb_fixnum_value(rand() % mrb_fixnum(arg));
	}
	else if ((mrb_fixnum_p(arg) && mrb_fixnum(arg) == 0) || mrb_nil_p(arg))
	{
		return mrb_float_value(mrb, (float) rand() / RAND_MAX);
	}
	else if (mrb_float_p(arg))
	{
		return mrb_float_value(mrb, (float) rand() / RAND_MAX);
	}
	else
	{
		mrb_raisef(mrb, E_TYPE_ERROR, "Expected Fixnum or Float, got %S",
		           mrb_str_new_cstr(mrb, mrb_class_name(mrb, mrb_class(mrb, arg))));
		return mrb_nil_value();
	}
}

MRB_FUNCTION(kernelSrand)
{
	srandCalled = true;

	if (mrb->c->ci->argc == 1)
	{
		mrb_int seed;
		mrb_get_args(mrb, "i", &seed);
		srand(seed);

		int oldSeed = currentSeed;
		currentSeed = seed;

		return mrb_fixnum_value(oldSeed);
	}
	else
	{
		int oldSeed = currentSeed;
		srandCurrentTime(&currentSeed);

		return mrb_fixnum_value(oldSeed);
	}
}

MRB_FUNCTION(kernelExit)
{
	MRB_FUN_UNUSED_PARAM;

	CCDirector::sharedDirector()->end();

	return mrb_nil_value();
}

MRB_FUNCTION(kernelLoadData)
{
	const char *filename;
	mrb_get_args(mrb, "z", &filename);

	unsigned long size;
	unsigned char* data = CCFileUtils::sharedFileUtils()->getFileData(filename,"rb",&size);
	mrb_value ret = mrb_marshal_load(mrb,(char*)data,size);
	delete [] data;

	return ret;
}

MRB_FUNCTION(kernelSaveData)
{


	return mrb_nil_value();
}

MRB_FUNCTION(kernelInteger)
{
	mrb_value obj;
	mrb_get_args(mrb, "o", &obj);

	return mrb_to_int(mrb, obj);
}

MRB_FUNCTION(test_draw)
{
	pthread_mutex_lock(&s_draw_cache_mutex);
	
	RMSceneCache* scene = RMSceneCache::getInstance();
	//scene->begin();
	CCSprite* sp = CCSprite::create("CloseNormal.png");
	sp->setPosition(ccp(100,100));
	//sp->visit();
	//scene->end();
	return mrb_nil_value();

	pthread_mutex_unlock(&s_draw_cache_mutex);
}

void kernelBindingInit(mrb_state *mrb)
{
	RClass *module = mrb->kernel_module;

	mrb_define_module_function(mrb, module, "eval", kernelEval, MRB_ARGS_REQ(1));
	mrb_define_module_function(mrb, module, "p", kernelP, MRB_ARGS_REQ(1) | MRB_ARGS_REST());
	mrb_define_module_function(mrb, module, "print", kernelPrint, MRB_ARGS_REQ(1) | MRB_ARGS_REST());
	mrb_define_module_function(mrb, module, "rand", kernelRand, MRB_ARGS_REQ(1));
	mrb_define_module_function(mrb, module, "srand", kernelSrand, MRB_ARGS_OPT(1));
	mrb_define_module_function(mrb, module, "exit", kernelExit, MRB_ARGS_NONE());
	mrb_define_module_function(mrb, module, "load_data", kernelLoadData, MRB_ARGS_REQ(1));
	mrb_define_module_function(mrb, module, "save_data", kernelSaveData, MRB_ARGS_REQ(2));
	mrb_define_module_function(mrb, module, "Integer", kernelInteger, MRB_ARGS_REQ(1));

	mrb_define_module_function(mrb, module, "test_draw", test_draw, MRB_ARGS_NONE());
}
