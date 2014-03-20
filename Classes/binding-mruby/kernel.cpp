#include <mruby.h>
#include <mruby/string.h>
#include <mruby/compile.h>
#include <string.h>
#include "binding-util.h"

#include <cocos2d.h>
using namespace cocos2d;

#include "marshal.h"
#include "../SceneMain.h"
#include <pthread.h>
#include "../ThreadHandlerManager.h"

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

#ifdef WIN32
	system("Pause");
#endif
	CCDirector::sharedDirector()->end();

	

	return mrb_nil_value();
}

MRB_FUNCTION(kernelLoadData)
{
	const char *filename;
	mrb_get_args(mrb, "z", &filename);

	string writepath = CCFileUtils::sharedFileUtils()->getWritablePath();
	string filepath = filename;

	unsigned long size;
	unsigned char* data = CCFileUtils::sharedFileUtils()->getFileData(filepath.c_str(),"rb",&size);
	mrb_value ret = mrb_marshal_load(mrb,(char*)data,size);
	delete [] data;

	return ret;
}

MRB_FUNCTION(kernelSaveData)
{
	mrb_value obj;
	const char *filename;
	mrb_get_args(mrb, "oz", &obj, &filename);

	RClass* const mod = mrb_module_get(mrb, "Marshal");
	mrb_value result = mrb_funcall(mrb, mrb_obj_value(mod), "dump", 1, obj);

	string writepath = CCFileUtils::sharedFileUtils()->getWritablePath();
	string filepath = writepath + filename;
	char* data = RSTRING_PTR(result);
	long len = RSTRING_LEN(result);
	FILE* fp = fopen(filepath.c_str(),"wb");
	if (NULL!=fp)
	{
		fwrite(data,sizeof(char),len,fp);
		fclose(fp);
	}

	return mrb_nil_value();
}

MRB_FUNCTION(kernelInteger)
{
	mrb_value obj;
	mrb_get_args(mrb, "o", &obj);

	return mrb_to_int(mrb, obj);
}

MRB_METHOD(string_gsub)
{
	mrb_value blk, match_expr, replace_expr = mrb_nil_value();
	int const argc = mrb_get_args(mrb, "&o|S", &blk, &match_expr, &replace_expr);

	if(mrb_string_p(match_expr)) {
		mrb_value argv[] = { match_expr, replace_expr };
		return mrb_funcall_with_block(mrb, self, mrb_intern_lit(mrb, "string_gsub"), argc, argv, blk);
	}

	if(!mrb_nil_p(blk) && !mrb_nil_p(replace_expr)) {
		mrb_raise(mrb, E_ARGUMENT_ERROR, "both block and replace expression must not be passed");
	}

	RClass* klass = mrb_class(mrb,match_expr);
	const char* classname = mrb_class_name(mrb,klass);
	mrb_value par = mrb_str_new_cstr(mrb,"/b/");

	mrb_value ret = mrb_funcall(mrb, match_expr, "match", 1, par);
	klass = mrb_class(mrb,ret);
	classname = mrb_class_name(mrb,klass);

	//mrb_str_cat(mrb, result, RSTRING_PTR(self) + last_end_pos, RSTRING_LEN(self) - last_end_pos);
	return mrb_nil_value();
}

MRB_METHOD(string_sub)
{
	return mrb_nil_value();
}

MRB_METHOD(string_split)
{
	return mrb_nil_value();
}

MRB_METHOD(string_scan)
{
	return mrb_nil_value();
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

	mrb_define_method(mrb, mrb->string_class, "gsub", string_gsub, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(1) | MRB_ARGS_BLOCK());
	mrb_define_method(mrb, mrb->string_class, "onig_regexp_sub", string_sub, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(1) | MRB_ARGS_BLOCK());
	mrb_define_method(mrb, mrb->string_class, "onig_regexp_split", string_split, MRB_ARGS_REQ(1));
	mrb_define_method(mrb, mrb->string_class, "onig_regexp_scan", string_scan, MRB_ARGS_REQ(1) | MRB_ARGS_BLOCK());
}
