#include <mruby.h>
#include <mruby/string.h>
#include <mruby/compile.h>
#include <mruby/array.h>
#include <string.h>
#include "binding-util.h"

#include <cocos2d.h>
using namespace cocos2d;

#include "marshal.h"
#include "../SceneMain.h"
#include <pthread.h>
#include "../ThreadHandlerManager.h"

int checkUtf8Len(const char* str)
{
	int len = 0;

	//�����ж�
	char utf8check_tmp = str[0];
	int i=7;
	while (1)
	{
		char checkbyte = 1;
		if ((utf8check_tmp & (checkbyte<<i))!=0)
			len++;
		else
			break;
		i--;
	}
	return len;
}

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
	string log = RSTRING_PTR(buffer);
	CCLOG("%s",log.c_str());
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

	string filepath = filename;

	unsigned long size;
	unsigned char* data = CCFileUtils::sharedFileUtils()->getFileData(filepath.c_str(),"rb",&size);
	//CCLOG("loaddata %s",filepath.c_str());
	mrb_value ret = marshalLoadInt(mrb,(char*)data,size);
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

	string writepath = SceneMain::writeablePath;
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
	mrb_value block, match_expr, replace_expr = mrb_nil_value();
	int const argc = mrb_get_args(mrb, "&o|S", &block, &match_expr, &replace_expr);

	if(mrb_string_p(match_expr)) {
		mrb_value argv[] = { match_expr, replace_expr };
		return mrb_funcall_with_block(mrb, self, mrb_intern_lit(mrb, "string_gsub"), argc, argv, block);
	}

	if(!mrb_nil_p(block) && !mrb_nil_p(replace_expr)) {
		mrb_raise(mrb, E_ARGUMENT_ERROR, "both block and replace expression must not be passed");
	}
	char* self_str = mrb_string_value_ptr(mrb,self);
	mrb_value ret_str = mrb_str_new_cstr(mrb,self_str);
	char* findret = RSTRING_PTR(ret_str);
	while (1)
	{
		if(*findret==0)
			break;

		mrb_value matchdata = mrb_funcall(mrb, match_expr, "match", 1, mrb_str_new_cstr(mrb,findret));
		if (mrb_nil_p(matchdata))
			break;
		mrb_value regchar = mrb_funcall(mrb,matchdata,"to_s",0);
		if (RSTRING_LEN(regchar)==0)
			break;

		char findchar[1024]={0};
		strncpy(findchar,RSTRING_PTR(regchar),RSTRING_LEN(regchar));

		mrb_value blockret = mrb_yield(mrb,block,regchar);
		char* blockret_str = RSTRING_PTR(blockret);
		
		findret = strstr(findret,findchar);
		memcpy(findret,blockret_str,RSTRING_LEN(regchar));
		findret+=RSTRING_LEN(regchar);
	}

	if (findret == RSTRING_PTR(ret_str))
	{
		return mrb_nil_value();
	}

	return ret_str;
}

MRB_METHOD(string_gsub2)
{
	mrb_value block, match_expr, replace_expr = mrb_nil_value();
	int const argc = mrb_get_args(mrb, "&o|S", &block, &match_expr, &replace_expr);

	if(mrb_string_p(match_expr)) {
		mrb_value argv[] = { match_expr, replace_expr };
		return mrb_funcall_with_block(mrb, self, mrb_intern_lit(mrb, "string_gsub"), argc, argv, block);
	}

	if(!mrb_nil_p(block) && !mrb_nil_p(replace_expr)) {
		mrb_raise(mrb, E_ARGUMENT_ERROR, "both block and replace expression must not be passed");
	}
	char* self_str = mrb_string_value_ptr(mrb,self);
	string findret = self_str;
	int findoff=0;
	while (1)
	{
		if(findret[0]==0)
			break;

		mrb_value matchdata = mrb_funcall(mrb, match_expr, "match", 1, mrb_str_new_cstr(mrb,findret.c_str()));
		if (mrb_nil_p(matchdata))
			break;
		mrb_value regchar = mrb_funcall(mrb,matchdata,"to_s",0);
		if (RSTRING_LEN(regchar)==0)
			break;

		char findchar[1024]={0};
		strncpy(findchar,RSTRING_PTR(regchar),RSTRING_LEN(regchar));

		mrb_value blockret = mrb_yield(mrb,block,regchar);
		if (mrb_nil_p(blockret))
			break;
		
		char* ret_str = RSTRING_PTR(blockret);

		int findpos = findret.find(findchar,findoff);
		findret.replace(findpos,RSTRING_LEN(regchar),ret_str);
		findoff+=RSTRING_LEN(blockret);
	}

	if (findret == self_str)
	{
		return mrb_nil_value();
	}


	if(RSTRING_LEN(self)!=findret.size())
	{
		mrb_str_resize(mrb,self,findret.size());
	}
	strcpy(RSTRING_PTR(self),findret.c_str());

	return self;
}

MRB_METHOD(string_sub)
{
	mrb_value block, match_expr, replace_expr = mrb_nil_value();
	int const argc = mrb_get_args(mrb, "&o|S", &block, &match_expr, &replace_expr);

	if(mrb_string_p(match_expr)) {
		mrb_value argv[] = { match_expr, replace_expr };
		return mrb_funcall_with_block(mrb, self, mrb_intern_lit(mrb, "string_gsub"), argc, argv, block);
	}

	if(!mrb_nil_p(block) && !mrb_nil_p(replace_expr)) {
		mrb_raise(mrb, E_ARGUMENT_ERROR, "both block and replace expression must not be passed");
	}
	char* self_str = mrb_string_value_ptr(mrb,self);
	mrb_value ret_str = mrb_str_new_cstr(mrb,self_str);

	mrb_value matchdata = mrb_funcall(mrb, match_expr, "match", 1, ret_str);
	if (mrb_nil_p(matchdata))
		return mrb_nil_value();

	mrb_value regchar = mrb_funcall(mrb,matchdata,"to_s",0);

	char findchar[1024]={0};
	strncpy(findchar,RSTRING_PTR(regchar),RSTRING_LEN(regchar));

	mrb_value blockret = mrb_yield(mrb,block,regchar);
	char* blockret_str = RSTRING_PTR(blockret);

	char* findret = strstr(RSTRING_PTR(ret_str),findchar);
	memcpy(findret,blockret_str,RSTRING_LEN(regchar));

	return ret_str;
}

MRB_METHOD(string_sub2)
{
	mrb_value block, match_expr, replace_expr = mrb_nil_value();
	int const argc = mrb_get_args(mrb, "&o|S", &block, &match_expr, &replace_expr);

	if(mrb_string_p(match_expr)) {
		mrb_value argv[] = { match_expr, replace_expr };
		return mrb_funcall_with_block(mrb, self, mrb_intern_lit(mrb, "string_gsub"), argc, argv, block);
	}

	if(!mrb_nil_p(block) && !mrb_nil_p(replace_expr)) {
		mrb_raise(mrb, E_ARGUMENT_ERROR, "both block and replace expression must not be passed");
	}
	char* self_str = mrb_string_value_ptr(mrb,self);

	mrb_value matchdata = mrb_funcall(mrb, match_expr, "match", 1, mrb_str_new_cstr(mrb,self_str));
	if(mrb_nil_p(matchdata))
		return mrb_nil_value();

	mrb_value regchar = mrb_funcall(mrb,matchdata,"to_s",0);

	char findchar[1024]={0};
	strncpy(findchar,RSTRING_PTR(regchar),RSTRING_LEN(regchar));

	char* ret_str = NULL;
	if (!mrb_nil_p(block))
	{
		mrb_value blockret = mrb_yield(mrb,block,regchar);
		ret_str = RSTRING_PTR(blockret);
	}
	else
	{
		ret_str = RSTRING_PTR(replace_expr);
	}
	
	string selfstr = self_str;
	int pos = selfstr.find(findchar);
	if (pos!=1)
	{
		selfstr.replace(pos,RSTRING_LEN(regchar),ret_str);
	}

	if(RSTRING_LEN(self)!=selfstr.size())
	{
		mrb_str_resize(mrb,self,selfstr.size());
	}
	
	strcpy(RSTRING_PTR(self),selfstr.c_str());

	return self;
}

MRB_METHOD(string_split)
{

	return self;
}

MRB_METHOD(string_scan)
{
	mrb_value block, match_expr, replace_expr = mrb_nil_value();
	int const argc = mrb_get_args(mrb, "&o|S", &block, &match_expr, &replace_expr);

	if(mrb_string_p(match_expr)) {
		mrb_value argv[] = { match_expr, replace_expr };
		return mrb_funcall_with_block(mrb, self, mrb_intern_lit(mrb, "string_gsub"), argc, argv, block);
	}

	if(!mrb_nil_p(block) && !mrb_nil_p(replace_expr)) {
		mrb_raise(mrb, E_ARGUMENT_ERROR, "both block and replace expression must not be passed");
	}
	char* self_str = mrb_string_value_ptr(mrb,self);
	char* findret = self_str;
	mrb_value ret_array = mrb_ary_new(mrb);
	int i=0;
	while (1)
	{
		if(*findret==0)
			break;

		mrb_value matchdata = mrb_funcall(mrb, match_expr, "match", 1, mrb_str_new_cstr(mrb,findret));
		if (mrb_nil_p(matchdata))
			break;
		mrb_value regchar = mrb_funcall(mrb,matchdata,"to_s",0);
		if (RSTRING_LEN(regchar)==0)
			break;

		if(mrb_nil_p(block))
		{
			mrb_ary_push(mrb,ret_array,regchar);
		}
		else
		{
			mrb_value blockret = mrb_yield(mrb,block,regchar);
			mrb_ary_push(mrb,ret_array,blockret);
		}

		char findchar[1024]={0};
		strncpy(findchar,RSTRING_PTR(regchar),RSTRING_LEN(regchar));
		
		findret = strstr(findret,findchar);
		findret+=RSTRING_LEN(regchar);

	}

	return ret_array;
}

MRB_METHOD(string_slice2)
{
	char* src_str=RSTRING_PTR(self);
	char ret_str[100]={0};

	if (mrb->c->ci->argc == 1)
	{
		mrb_value match_expr;
		mrb_get_args(mrb, "o", &match_expr);
		
		mrb_value matchdata = mrb_funcall(mrb, match_expr, "match", 1, mrb_str_new_cstr(mrb,src_str));

		if (mrb_nil_p(matchdata))
			return mrb_nil_value();
		
		mrb_value regchar = mrb_funcall(mrb,matchdata,"to_s",0);
		char* regchar_c = RSTRING_PTR(regchar);
		int del_len = checkUtf8Len(regchar_c);

		if (del_len==0)
			del_len = RSTRING_LEN(regchar);

		strncpy(ret_str,RSTRING_PTR(regchar),del_len);
		char* delp = strstr(src_str,RSTRING_PTR(regchar));
		strcpy(delp,delp+del_len);
	
	}
	else
	{
		mrb_int del_start,del_len;
		mrb_get_args(mrb, "ii", &del_start, &del_len);

		char* delp =src_str+del_start-1;
		strncpy(ret_str,delp,del_len);
		strcpy(delp,delp+del_len);
	}

	return mrb_str_new_cstr(mrb,ret_str);
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
	mrb_define_method(mrb, mrb->string_class, "gsub!", string_gsub2, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(1) | MRB_ARGS_BLOCK());
	mrb_define_method(mrb, mrb->string_class, "sub", string_sub, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(1) | MRB_ARGS_BLOCK());
	mrb_define_method(mrb, mrb->string_class, "sub!", string_sub2, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(1) | MRB_ARGS_BLOCK());
	mrb_define_method(mrb, mrb->string_class, "split", string_split, MRB_ARGS_REQ(1));
	mrb_define_method(mrb, mrb->string_class, "scan", string_scan, MRB_ARGS_REQ(1) | MRB_ARGS_BLOCK());
	mrb_define_method(mrb, mrb->string_class, "slice!",string_slice2,MRB_ARGS_REQ(2)|MRB_ARGS_OPT(1));
}
