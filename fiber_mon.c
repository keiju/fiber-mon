/**********************************************************************

  fiber-mon.c -
   	Copyright (C) 2010-2011 Keiju ISHITSUKA
				(Penta Advanced Labrabries, Co.,Ltd)

**********************************************************************/

#include "ruby.h"

#include "xthread.h"
#include "fiber_mon.h"

VALUE rb_cFiberMon;
VALUE rb_cFiberMonMonitor;
VALUE rb_cFiberMonConditionVariable;

typedef struct rb_fibermon_struct
{
  long started;
  VALUE current_fib;
  VALUE entries;

  VALUE obsolate_mon;
} fibermon_t;

#define GetFiberMonPtr(obj, tobj) \
  TypedData_Get_Struct((obj), fibermon_t, &fibermon_data_type, (tobj))

static void
fibermon_mark(void *ptr)
{
  fibermon_t *mon = (fibermon_t*)ptr;
  
  rb_gc_mark(mon->current_fib);
  rb_gc_mark(mon->entries);
  rb_gc_mark(mon->obsolate_mon);
}

static void
fibermon_free(void *ptr)
{
  ruby_xfree(ptr);
}

static size_t
fibermon_memsize(const void *ptr)
{
  return ptr ? sizeof(fibermon_t) : 0;
}

static const rb_data_type_t fibermon_data_type = {
    "fibermon",
    {fibermon_mark, fibermon_free, fibermon_memsize,},
};

static VALUE
fibermon_alloc(VALUE klass)
{
  VALUE volatile obj;
  fibermon_t *mon;

  obj = TypedData_Make_Struct(klass, fibermon_t, &fibermon_data_type, mon);
  
  mon->started = 0;
  mon->current_fib = Qnil;
  mon->entries = rb_xthread_queue_new();
  
  mon->obsolate_mon = Qnil;
  return obj;
}

static VALUE fibermon_start(VALUE);

static VALUE
fibermon_initialize(VALUE self)
{
  fibermon_t *mon;
  GetFiberMonPtr(self, mon);
  
  if (!mon->started) {
    rb_thread_create(fibermon_start, (void *)self);
    mon->started = 1;
  }
  return self;
}

static VALUE
fibermon_mon_start(VALUE self)
{
  fibermon_t *mon;
  GetFiberMonPtr(self, mon);

  rb_warn("This api is old interface. You don't need call this method.");
}


VALUE
rb_fibermon_new(void)
{
  VALUE mon;
  mon = fibermon_alloc(rb_cFiberMon);
  fibermon_initialize(mon);
}

VALUE
rb_fibermon_current(VALUE self)
{
  fibermon_t *mon;
  GetFiberMonPtr(self, mon);
  
  return mon->current_fib;
}

VALUE rb_fibermon_monitor_new(VALUE);

static VALUE
rb_fibermon_obsolate_mon(VALUE self)
{
  fibermon_t *mon;
  GetFiberMonPtr(self, mon);

  if (NIL_P(mon->obsolate_mon)) {
    mon->obsolate_mon = rb_fibermon_monitor_new(self);
  }
  return mon->obsolate_mon;
}

/*
static VALUE
fibermon_start_cond_wait(VALUE self)
{
  fibermon_t *mon;
  GetFiberMonPtr(self, mon);

  return rb_xthread_cond_wait(mon->wait_resume_cv, mon->wait_resume_mx, Qnil);
}
*/

static VALUE
fibermon_start_fib_resume(fibermon_t *mon)
{
  rb_fiber_resume(mon->current_fib, 0, NULL);
}

static VALUE
fibermon_start_fib_ensure(fibermon_t *mon)
{
  mon->current_fib = Qnil;
  return Qnil;
}

struct fibermon_start_fiber_creation_arg {
  VALUE args;
  VALUE proc;
};

static VALUE
fibermon_start_fiber_creation(VALUE x, struct fibermon_start_fiber_creation_arg *arg)
{
  rb_proc_call(arg->proc, arg->args);
}

static VALUE
fibermon_start(VALUE self)
{
  fibermon_t *mon;
  VALUE block;
  
  GetFiberMonPtr(self, mon);
  
  while(1) {
    VALUE entry;

    entry = rb_xthread_queue_pop(mon->entries);
    if (CLASS_OF(entry) == rb_cProc) {
      struct fibermon_start_fiber_creation_arg arg;
      arg.args = rb_ary_new3(1, self);
      arg.proc = entry;
  
      mon->current_fib =
	rb_fiber_new(fibermon_start_fiber_creation, (VALUE)&arg);
    }
    else {
      mon->current_fib = entry;
    }
    rb_ensure(fibermon_start_fib_resume, (VALUE)mon,
	      fibermon_start_fib_ensure, (VALUE)mon);
  }
}

VALUE
rb_fibermon_entry_fiber(VALUE self, VALUE fib)
{
  fibermon_t *mon;
  GetFiberMonPtr(self, mon);

  rb_xthread_queue_push(mon->entries, fib);
}


static VALUE
fibermon_entry(int argc, VALUE *argv, VALUE self)
{
  VALUE block;
  VALUE proc;
  
  rb_scan_args(argc, argv, "01&", &proc, &block);
  if (NIL_P(proc)) {
    proc = block;
  }
      
  return rb_fibermon_entry_fiber(self, proc);
}

VALUE rb_fibermon_monitor_wait_cond(VALUE);

static VALUE
fibermon_yield_obso(VALUE self)
{
  fibermon_t *mon;
  GetFiberMonPtr(self, mon);

  if (!rb_fibermon_monitor_valid_owner_p(mon->obsolate_mon)) {
    return rb_fiber_yield(0, NULL);
  }
  else {
    return rb_fibermon_monitor_wait_cond(mon->obsolate_mon);
  }
}

VALUE
rb_fibermon_yield(VALUE self)
{
  fibermon_t *mon;
  GetFiberMonPtr(self, mon);

  rb_xthread_queue_push(mon->entries, mon->current_fib);

  if (NIL_P(mon->obsolate_mon)) {
    return rb_fiber_yield(0, NULL);
  }
  else {
    return fibermon_yield_obso(self);
  }
}

VALUE
rb_fibermon_new_mon(VALUE self)
{
  return rb_fibermon_monitor_new(self);
}

VALUE rb_fibermon_cond_new(VALUE);

VALUE
rb_fibermon_new_cv(VALUE self)
{
    
  rb_warn("This api is old interface. You can use FiberMon#new_mon and FiberMon::Monitor#new_cv.");
  
  return rb_fibermon_cond_new(rb_fibermon_obsolate_mon(self));
}

VALUE rb_fibermon_monitor_synchronize(VALUE, VALUE (*)(VALUE), VALUE);

VALUE
rb_fibermon_synchronize(VALUE self)
{
  VALUE mon;
  
  rb_warn("This api is old interface. You can use FiberMon#new_mon and FiberMon::Monitor#synchronize.");
  
  mon = rb_fibermon_obsolate_mon(self);
  return rb_fibermon_monitor_synchronize(mon, rb_yield, mon);
}

typedef struct rb_fibermon_monitor_struct
{
  VALUE fibermon;
  
  VALUE owner;
  long count;
  VALUE mutex;
} fibermon_monitor_t;

#define GetFiberMonMonitorPtr(obj, tobj) \
    TypedData_Get_Struct((obj), fibermon_monitor_t, \
			 &fibermon_monitor_data_type, (tobj))

#define FIBERMON_MONITOR_CHECK_OWNER(obj) \
  { \
    fibermon_monitor_t *mon; \
    VALUE th = rb_thread_current(); \
    GetFiberMonMonitorPtr(obj, mon);  \
    if (mon->owner != th) { \
      rb_raise(rb_eThreadError, "current thread not owner"); \
    } \
  }

static void
fibermon_monitor_mark(void *ptr)
{
  fibermon_monitor_t *mon = (fibermon_monitor_t*)ptr;
  
  rb_gc_mark(mon->fibermon);
  rb_gc_mark(mon->owner);
  rb_gc_mark(mon->mutex);
}

static void
fibermon_monitor_free(void *ptr)
{
  ruby_xfree(ptr);
}

static size_t
fibermon_monitor_memsize(const void *ptr)
{
  return ptr ? sizeof(fibermon_monitor_t) : 0;
}

static const rb_data_type_t fibermon_monitor_data_type = {
    "fibermon_monitor",
    {fibermon_monitor_mark, fibermon_monitor_free, fibermon_monitor_memsize,},
};

static VALUE
fibermon_monitor_alloc(VALUE klass)
{
  VALUE volatile obj;
  fibermon_monitor_t *mon;

  obj = TypedData_Make_Struct(klass, fibermon_monitor_t,
			      &fibermon_monitor_data_type, mon);
  mon->fibermon = Qnil;
  mon->owner = Qnil;
  mon->count = 0;
  mon->mutex = rb_mutex_new();

  return obj;
}

static VALUE
fibermon_monitor_initialize(VALUE self, VALUE fibermon)
{
  fibermon_monitor_t *mon;
  GetFiberMonMonitorPtr(self, mon);

  mon->fibermon = fibermon;
  
  return self;
}

VALUE
rb_fibermon_monitor_new(VALUE fibermon)
{
  VALUE mon;
  
  mon = fibermon_monitor_alloc(rb_cFiberMonMonitor);
  fibermon_monitor_initialize(mon, fibermon);
  return mon;
}

VALUE
rb_fibermon_monitor_valid_owner_p(VALUE self)
{
  fibermon_monitor_t *mon;
  VALUE th = rb_thread_current();

  GetFiberMonMonitorPtr(self, mon);
  
  if (mon->owner == th) {
    return Qtrue;
  }
  else {
    return Qfalse;
  }
}

VALUE
rb_fibermon_monitor_try_enter(VALUE self)
{
  fibermon_monitor_t *mon;
  VALUE th = rb_thread_current();

  GetFiberMonMonitorPtr(self, mon);

  if (mon->owner != th) {
    if (rb_mutex_trylock(mon->mutex) == Qfalse) {
      return Qfalse;
    }
    mon->owner = th;
  }
  mon->count++;
  return Qtrue;
}

VALUE
rb_fibermon_monitor_enter(VALUE self)
{
  fibermon_monitor_t *mon;
  VALUE th = rb_thread_current();

  GetFiberMonMonitorPtr(self, mon);
  if (mon->owner != th) {
    rb_mutex_lock(mon->mutex);
    mon->owner = th;
  }
  mon->count += 1;
}

VALUE
rb_fibermon_monitor_exit(VALUE self)
{
  fibermon_monitor_t *mon;
  VALUE th = rb_thread_current();
  
  GetFiberMonMonitorPtr(self, mon);

  FIBERMON_MONITOR_CHECK_OWNER(self);
  mon->count--;
  if(mon->count == 0) {
    mon->owner = Qnil;
    rb_mutex_unlock(mon->mutex);
  }
}

VALUE
rb_fibermon_monitor_synchronize(VALUE self, VALUE (*func)(VALUE), VALUE arg)
{
  rb_fibermon_monitor_enter(self);
  return rb_ensure(func, arg, rb_fibermon_monitor_exit, self);
}

static VALUE
fibermon_monitor_synchronize(VALUE self)
{
  return rb_fibermon_monitor_synchronize(self, rb_yield, self);
}

VALUE
rb_fibermon_monitor_new_cond(VALUE self)
{
  rb_fibermon_cond_new(self);
}

static VALUE
rb_fibermon_monitor_entry_fiber(VALUE self, VALUE fiber)
{
  fibermon_monitor_t *mon;
  GetFiberMonMonitorPtr(self, mon);

  return rb_fibermon_entry_fiber(mon->fibermon, fiber);
}

struct fibermon_monitor_wait_cond_arg {
  fibermon_monitor_t *mon;
  long count;
};

static VALUE
rb_fibermon_monitor_wait_cond_yield(VALUE dummy)
{
  return rb_fiber_yield(0, NULL);
}

VALUE
rb_fibermon_monitor_wait_cond_ensure(struct fibermon_monitor_wait_cond_arg *arg)
{
  fibermon_monitor_t *mon = arg->mon;

  rb_mutex_lock(mon->mutex);
  mon->owner = rb_thread_current();
  mon->count = arg->count;
}

VALUE
rb_fibermon_monitor_wait_cond(VALUE self)
{
  fibermon_monitor_t *mon;
  struct fibermon_monitor_wait_cond_arg arg;
  
  GetFiberMonMonitorPtr(self, mon);
  arg.count = mon->count;
  mon->owner = Qnil;
  mon->count = 0;
  rb_mutex_unlock(mon->mutex);

  arg.mon = mon;
  rb_ensure(rb_fibermon_monitor_wait_cond_yield, Qnil,
	    rb_fibermon_monitor_wait_cond_ensure, (VALUE)&arg);
}

typedef struct rb_fibermon_cond_struct
{
  VALUE monitor;
  VALUE waitings;
  
} fibermon_cond_t;

#define GetFiberMonCondPtr(obj, tobj) \
  TypedData_Get_Struct((obj), fibermon_cond_t, \
		       &fibermon_cond_data_type, (tobj))

static void
fibermon_cond_mark(void *ptr)
{
  fibermon_cond_t *cv = (fibermon_cond_t*)ptr;
  
  rb_gc_mark(cv->monitor);
  rb_gc_mark(cv->waitings);
}

static void
fibermon_cond_free(void *ptr)
{
  ruby_xfree(ptr);
}

static size_t
fibermon_cond_memsize(const void *ptr)
{
  return ptr ? sizeof(fibermon_cond_t) : 0;
}

static const rb_data_type_t fibermon_cond_data_type = {
  "fibermon_cond",
  {fibermon_cond_mark, fibermon_cond_free, fibermon_cond_memsize,},
};

static VALUE
fibermon_cond_alloc(VALUE klass)
{
  VALUE volatile obj;
  fibermon_cond_t *cv;

  obj = TypedData_Make_Struct(klass, fibermon_cond_t,
			      &fibermon_cond_data_type, cv);
  
  cv->monitor = Qnil;
  cv->waitings = rb_xthread_fifo_new();
  
  return obj;
}

static VALUE
fibermon_cond_initialize(VALUE self, VALUE monitor)
{
  fibermon_cond_t *cv;
  GetFiberMonCondPtr(self, cv);

  cv->monitor = monitor;
  return self;
}

VALUE
rb_fibermon_cond_new(VALUE monitor)
{
  VALUE cv;

  cv = fibermon_cond_alloc(rb_cFiberMonConditionVariable);
  fibermon_cond_initialize(cv, monitor);
  return cv;
}

VALUE
rb_fibermon_cond_signal(VALUE self)
{
  fibermon_cond_t *cv;
  VALUE fb;

  GetFiberMonCondPtr(self, cv);

  fb = rb_xthread_fifo_pop(cv->waitings);
  if (!NIL_P(fb)) {
    rb_fibermon_monitor_entry_fiber(cv->monitor, fb);
  }
  return self;
}

VALUE
rb_fibermon_cond_broadcast(VALUE self)
{
  fibermon_cond_t *cv;
  VALUE fb;

  GetFiberMonCondPtr(self, cv);

  while (!NIL_P(fb = rb_xthread_fifo_pop(cv->waitings))) {
    rb_fibermon_monitor_entry_fiber(cv->monitor, fb);
  }
  return self;
}

VALUE
rb_fibermon_cond_wait(VALUE self)
{
  fibermon_cond_t *cv;
  VALUE fb;

  GetFiberMonCondPtr(self, cv);
  rb_xthread_fifo_push(cv->waitings, rb_fiber_current());
  
  return rb_fibermon_monitor_wait_cond(cv->monitor);
}

VALUE
rb_fibermon_cond_wait_until(VALUE self)
{
  while(!RTEST(rb_yield(Qnil))) {
    rb_fibermon_cond_wait(self);
  }
  return self;
}

VALUE
rb_fibermon_cond_wait_while(VALUE self)
{
  while(RTEST(rb_yield(Qnil))) {
    rb_fibermon_cond_wait(self);
  }
  return self;
}

Init_fiber_mon()
{
  rb_cFiberMon = rb_define_class("FiberMon", rb_cObject);
  rb_define_alloc_func(rb_cFiberMon, fibermon_alloc);
  rb_define_method(rb_cFiberMon, "initialize", fibermon_initialize, 0);
  rb_define_method(rb_cFiberMon, "current", rb_fibermon_current, 0);
  rb_define_method(rb_cFiberMon, "mon_start", fibermon_mon_start, 0);
  rb_define_alias(rb_cFiberMon,  "start", "mon_start");
  rb_define_method(rb_cFiberMon, "entry_fiber", fibermon_entry, -1);
  rb_define_alias(rb_cFiberMon,  "entry", "entry_fiber");
  rb_define_method(rb_cFiberMon, "yield", rb_fibermon_yield, 0);
  rb_define_method(rb_cFiberMon, "new_mon", rb_fibermon_new_mon, 0);
  rb_define_method(rb_cFiberMon, "new_cv", rb_fibermon_new_cv, 0);
  rb_define_alias(rb_cFiberMon,  "new_cond", "new_cv");
  rb_define_method(rb_cFiberMon, "synchronize", rb_fibermon_synchronize, 0);
  
  rb_cFiberMonMonitor =
    rb_define_class_under(rb_cFiberMon, "Monitor", rb_cObject);
  rb_define_alloc_func(rb_cFiberMonMonitor, fibermon_monitor_alloc);
  rb_define_method(rb_cFiberMonMonitor, "iniialize",
		   fibermon_monitor_initialize, 1);
  rb_define_method(rb_cFiberMonMonitor, "try_enter",
		   rb_fibermon_monitor_try_enter, 0);
  rb_define_method(rb_cFiberMonMonitor, "enter",
		   rb_fibermon_monitor_enter, 0);
  rb_define_method(rb_cFiberMonMonitor, "exit",
		   rb_fibermon_monitor_exit, 0);
  rb_define_method(rb_cFiberMonMonitor, "synchronize",
		   fibermon_monitor_synchronize, 0);
  rb_define_method(rb_cFiberMonMonitor, "new_cond",
		   rb_fibermon_monitor_new_cond, 0);
  rb_define_alias(rb_cFiberMonMonitor,  "new_cv", "new_cond");
  
  rb_cFiberMonConditionVariable =
    rb_define_class_under(rb_cFiberMon, "ConditionVariable", rb_cObject);
  rb_define_alloc_func(rb_cFiberMonConditionVariable, fibermon_cond_alloc);
  rb_define_method(rb_cFiberMonConditionVariable, "iniialize",
		   fibermon_cond_initialize, 1);
  rb_define_method(rb_cFiberMonConditionVariable, "signal",
		   rb_fibermon_cond_signal, 0);
  rb_define_method(rb_cFiberMonConditionVariable, "broadcast",
		   rb_fibermon_cond_broadcast, 0);
  rb_define_method(rb_cFiberMonConditionVariable, "wait",
		   rb_fibermon_cond_wait, 0);
  rb_define_method(rb_cFiberMonConditionVariable, "wait_until",
		   rb_fibermon_cond_wait_until, 0);
  rb_define_method(rb_cFiberMonConditionVariable, "wait_while",
		   rb_fibermon_cond_wait_while, 0);
}
