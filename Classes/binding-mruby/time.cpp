#include "binding-util.h"
#include <mruby/string.h>
#include <mruby/array.h>
#include <mruby/class.h>

#ifdef WIN32
#include <CCStdC.h>
#else
#include <sys/time.h>
#endif


struct TimeImpl
{
	struct tm _tm;
	struct timeval _tv;
};

extern const mrb_data_type TimeType =
{
    "Time",
    freeInstance<TimeImpl>
};

extern mrb_value
timeFromSecondsInt(mrb_state *mrb, time_t seconds)
{
	TimeImpl *p = new TimeImpl;

	const time_t tt = seconds;
	p->_tv.tv_sec = seconds;
	p->_tm = *localtime(&tt);

	mrb_value obj = wrapObject(mrb, p, TimeType);

	return obj;
}


MRB_FUNCTION(timeAt)
{
	mrb_int seconds;
	mrb_get_args(mrb, "i", &seconds);

	return timeFromSecondsInt(mrb, seconds);
}

MRB_FUNCTION(timeNow)
{
	TimeImpl *p = new TimeImpl;

	const time_t tt = p->_tv.tv_sec;
	gettimeofday(&p->_tv, 0);
	p->_tm = *localtime(&tt);

	mrb_value obj = wrapObject(mrb, p, TimeType);

	return obj;
}

static mrb_value
secondsAdded(mrb_state *mrb, TimeImpl *p, mrb_int seconds)
{
	TimeImpl *newP = new TimeImpl;
	*newP = *p;
	
	newP->_tv.tv_sec += seconds;
	const time_t tt = p->_tv.tv_sec;
	p->_tm = *localtime(&tt);

	return wrapObject(mrb, newP, TimeType);
}

MRB_METHOD(timePlus)
{
	mrb_int seconds;
	mrb_get_args(mrb, "i", &seconds);

	TimeImpl *p = getPrivateData<TimeImpl>(mrb, self);

	TimeImpl *newP = new TimeImpl;
	*newP = *p;

	newP->_tv.tv_sec += seconds;
	const time_t tt = p->_tv.tv_sec;
	p->_tm = *localtime(&tt);

	return wrapObject(mrb, newP, TimeType);
}

MRB_METHOD(timeMinus)
{
	mrb_value minuent;
	mrb_get_args(mrb, "o", &minuent);

	TimeImpl *p = getPrivateData<TimeImpl>(mrb, self);

	if (mrb_fixnum_p(minuent))
	{
		mrb_int seconds = mrb_fixnum(minuent);

		return secondsAdded(mrb, p, -seconds);
	}
	else if (mrb_type(minuent) == MRB_TT_OBJECT)
	{
		TimeImpl *o = getPrivateDataCheck<TimeImpl>(mrb, minuent, TimeType);

		return mrb_float_value(mrb, p->_tv.tv_sec - o->_tv.tv_sec);
	}
	else
	{
		mrb_raisef(mrb, E_TYPE_ERROR, "Expected Fixnum or Time, got %S",
		           mrb_str_new_cstr(mrb, mrb_class_name(mrb, mrb_class(mrb, minuent))));
	}

	return mrb_nil_value();
}

MRB_METHOD(timeCompare)
{
	mrb_value cmpTo;
	mrb_get_args(mrb, "o", &cmpTo);

	mrb_int cmpToS = 0;

	if (mrb_fixnum_p(cmpTo))
	{
		cmpToS = mrb_fixnum(cmpTo);
	}
	else if (mrb_type(cmpTo) == MRB_TT_OBJECT)
	{
		TimeImpl *cmpToP = getPrivateDataCheck<TimeImpl>(mrb, cmpTo, TimeType);
		cmpToS = cmpToP->_tv.tv_sec;
	}
	else
	{
		mrb_raisef(mrb, E_TYPE_ERROR, "Expected Fixnum or Time, got %S",
		           mrb_str_new_cstr(mrb, mrb_class_name(mrb, mrb_class(mrb, cmpTo))));
	}

	TimeImpl *p = getPrivateData<TimeImpl>(mrb, self);
	mrb_int selfS = p->_tv.tv_sec;

	if (selfS < cmpToS)
		return mrb_fixnum_value(-1);
	else if (selfS == cmpToS)
		return mrb_fixnum_value(0);
	else
		return mrb_fixnum_value(1);
}

MRB_METHOD(timeStrftime)
{
	const char *format;
	mrb_get_args(mrb, "z", &format);

	TimeImpl *p = getPrivateData<TimeImpl>(mrb, self);

	static char buffer[512];
	strftime(buffer, sizeof(buffer), format, &p->_tm);

	return mrb_str_new_cstr(mrb, buffer);
}

#define TIME_ATTR(attr) \
	MRB_METHOD(timeGet_##attr) \
	{ \
		TimeImpl *p = getPrivateData<TimeImpl>(mrb, self); \
		return mrb_fixnum_value(p->_tm.tm_##attr); \
	}

TIME_ATTR(sec)
TIME_ATTR(min)
TIME_ATTR(hour)
TIME_ATTR(mday)
TIME_ATTR(mon)
TIME_ATTR(year)
TIME_ATTR(wday)

#define TIME_BIND_ATTR(attr) \
	{ mrb_define_method(mrb, klass, #attr, timeGet_##attr, MRB_ARGS_NONE()); }

void
timeBindingInit(mrb_state *mrb)
{
	RClass *klass = mrb_define_class(mrb, "Time", mrb_class_get(mrb, "Object"));
	mrb_include_module(mrb, klass, mrb_class_get(mrb, "Object"));

	mrb_define_class_method(mrb, klass, "now", timeNow, MRB_ARGS_NONE());
	mrb_define_class_method(mrb, klass, "at", timeAt, MRB_ARGS_REQ(1));

	mrb_define_method(mrb, klass, "+", timePlus, MRB_ARGS_REQ(1));
	mrb_define_method(mrb, klass, "-", timeMinus, MRB_ARGS_REQ(1));
	mrb_define_method(mrb, klass, "<=>", timeCompare, MRB_ARGS_REQ(1));
	mrb_define_method(mrb, klass, "strftime", timeStrftime, MRB_ARGS_REQ(1));

	TIME_BIND_ATTR(sec);
	TIME_BIND_ATTR(min);
	TIME_BIND_ATTR(hour);
	TIME_BIND_ATTR(mday);
	TIME_BIND_ATTR(mon);
	TIME_BIND_ATTR(year);
	TIME_BIND_ATTR(wday);
}
