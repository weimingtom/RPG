#ifndef _RUBYENGINE_H
#define _RUBYENGINE_H

/* Include the mruby header */
#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/irep.h>
#include "binding-util.h"

#include <string>
#include <vector>
using namespace std;


class RubyEngine
{
public:
	mrb_state* initAll();
	mrb_state* initRubyEngine();
	void initBindingMethod();
	mrb_state* getMrbState();
	void checkException();
	void showExcMessageBox(mrb_value exc);
	void runScript(const char* script);

	struct RMXPScript
	{
		int id;
		string name;
		string script;
	};
	vector<RMXPScript> m_RMXPScripts;
	void initRMXPScript(const char* filename);
	void runRMXPScript();
private:
	static void* networkThread(void* data);

	mrb_state* m_mrb;
};

#endif