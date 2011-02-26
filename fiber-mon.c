/**********************************************************************

  fiber-mon.c -
   	Copyright (C) 2010-2011 Keiju ISHITSUKA
				(Penta Advanced Labrabries, Co.,Ltd)

**********************************************************************/

#include "ruby.h"

#include "xthread.h"

VALUE rb_cFiberMon;
VALUE rb_cFiberMonMonitor;
VALUE rb_cFiberMonConditionVariable;

typedef struct rb_fiber_mon_struct
{
  long started;
  VALUE current_fib;
  VALUE entries;

  VALUE obsolate_mon;
} fiber_mon_t;

#define GetFiberMonPtr(obj, tobj) \
  TypedData_Get_Struct((obj), fiber_mon_t, &fiber_mon_data_type, (tobj))

static void
fiber_mon_mark(void *ptr)
{
  fiber_mon_t *mon = (fiber_mon_t*)ptr;
  
  rb_gc_mark(mon->current_fib);
  rb_gc_mark(mon->entries);
  rb_gc_mark(mon->obsolate_mon);
}

static void
fiber_mon_free(void *ptr)
{
  ruby_xfree(ptr);
}

static size_t
fiber_mon_memsize(const void *ptr)
{
  return ptr ? sizeof(fiber_mon_t) : 0;
}

static const rb_data_type_t fiber_mon_data_type = {
    "fiber_mon",
    {fiber_mon_mark, fiber_mon_free, fiber_mon_memsize,},
};

static VALUE
fiber_mon_alloc(VALUE klass)
{
  VALUE volatile obj;
  fiber_mon_t *mon;

  obj = TypedData_Make_Struct(klass, fiber_mon_t, &fiber_mon_data_type, mon);
  
  mon->started = 0;
  mon->current_fib = Qnil;
  mon->entries = rb_queue_new();
  
  mon->obsolate_mon = Qnil;
  return obj;
}

static VALUE fiber_mon_start(VALUE);

static VALUE
fiber_mon_initialize(VALUE self)
{
  fiber_mon_t *mon;
  GetFiberMonPtr(self, mon);
  
  if (!mon->started) {
    rb_thread_create(fiber_mon_start, (void *)self);
    mon->started = 1;
  }
  return self;
}

static VALUE
fiber_mon_mon_start(VALUE self)
{
  fiber_mon_t *mon;
  GetFiberMonPtr(self, mon);

  rb_warn("This api is old interface. You don't need call this method.");
}


VALUE
rb_fiber_mon_new(void)
{
  VALUE mon;
  mon = fiber_mon_alloc(rb_cFiberMon);
  fiber_mon_initialize(mon);
}

VALUE
rb_fiber_mon_current(VALUE self)
{
  fiber_mon_t *mon;
  GetFiberMonPtr(self, mon);
  
  return mon->current_fib;
}

VALUE rb_fiber_mon_monitor_new(VALUE);

static VALUE
rb_fiber_mon_obsolate_mon(VALUE self)
{
  fiber_mon_t *mon;
  GetFiberMonPtr(self, mon);

  if (NIL_P(mon->obsolate_mon)) {
    mon->obsolate_mon = rb_fiber_mon_monitor_new(self);
  }
  return mon->obsolate_mon;
}

/*
static VALUE
fiber_mon_start_cond_wait(VALUE self)
{
  fiber_mon_t *mon;
  GetFiberMonPtr(self, mon);

  return rb_cond_wait(mon->wait_resume_cv, mon->wait_resume_mx, Qnil);
}
*/

static VALUE
fiber_mon_start_fib_resume(fiber_mon_t *mon)
{
  rb_fiber_resume(mon->current_fib, 0, NULL);
}

static VALUE
fiber_mon_start_fib_ensure(fiber_mon_t *mon)
{
  mon->current_fib = Qnil;
  return Qnil;
}

struct fiber_mon_start_fiber_creation_arg {
  VALUE args;
  VALUE proc;
};

static VALUE
fiber_mon_start_fiber_creation(VALUE x, struct fiber_mon_start_fiber_creation_arg *arg)
{
  rb_proc_call(arg->proc, arg->args);
}

static VALUE
fiber_mon_start(VALUE self)
{
  fiber_mon_t *mon;
  VALUE block;
  
  GetFiberMonPtr(self, mon);
  
  while(1) {
    VALUE entry;

    entry = rb_queue_pop(mon->entries);
    if (CLASS_OF(entry) == rb_cProc) {
      struct fiber_mon_start_fiber_creation_arg arg;
      arg.args = rb_ary_new3(1, self);
      arg.proc = entry;
  
      mon->current_fib =
	rb_fiber_new(fiber_mon_start_fiber_creation, (VALUE)&arg);
    }
    else {
      mon->current_fib = entry;
    }
    rb_ensure(fiber_mon_start_fib_resume, (VALUE)mon,
	      fiber_mon_start_fib_ensure, (VALUE)mon);
  }
}

VALUE
rb_fiber_mon_entry_fiber(VALUE self, VALUE fib)
{
  fiber_mon_t *mon;
  GetFiberMonPtr(self, mon);

  rb_queue_push(mon->entries, fib);
}


static VALUE
fiber_mon_entry(int argc, VALUE *argv, VALUE self)
{
  VALUE block;
  VALUE proc;
  
  rb_scan_args(argc, argv, "01&", &proc, &block);
  if (NIL_P(proc)) {
    proc = block;
  }
      
  return rb_fiber_mon_entry_fiber(self, proc);
}

VALUE rb_fiber_mon_monitor_wait_cond(VALUE);

static VALUE
fiber_mon_yield_obso(VALUE self)
{
  fiber_mon_t *mon;
  GetFiberMonPtr(self, mon);

  if (!rb_monitor_vaid_owner_p(mon->obsolate_mon)) {
    return rb_fiber_yield(0, NULL);
  }
  else {
    return rb_fiber_mon_monitor_wait_cond(mon->obsolate_mon);
  }
}

VALUE
rb_fiber_mon_yield(VALUE self)
{
  fiber_mon_t *mon;
  GetFiberMonPtr(self, mon);

  rb_queue_push(mon->entries, mon->current_fib);

  if (NIL_P(mon->obsolate_mon)) {
    return rb_fiber_yield(0, NULL);
  }
  else {
    return fiber_mon_yield_obso(self);
  }
}

VALUE
rb_fiber_mon_new_mon(VALUE self)
{
  return rb_fiber_mon_monitor_new(self);
}

VALUE rb_fiber_mon_cond_new(VALUE);

VALUE
rb_fiber_mon_new_cv(VALUE self)
{
    
  rb_warn("This api is old interface. You can use FiberMon#new_mon and FiberMon::Monitor#new_cv.");
  
  return rb_fiber_mon_cond_new(rb_fiber_mon_obsolate_mon(self));
}

VALUE rb_fiber_mon_monitor_synchronize(VALUE, VALUE (*)(VALUE), VALUE);

VALUE
rb_fiber_mon_synchronize(VALUE self)
{
  VALUE mon;
  
  rb_warn("This api is old interface. You can use FiberMon#new_mon and FiberMon::Monitor#synchronize.");
  
  mon = rb_fiber_mon_obsolate_mon(self);
  return rb_fiber_mon_monitor_synchronize(mon, rb_yield, mon);
}

typedef struct rb_fiber_mon_monitor_struct
{
  VALUE fiber_mon;
  
  VALUE owner;
  long count;
  VALUE mutex;
} fiber_mon_monitor_t;

#define GetFiberMonMonitorPtr(obj, tobj) \
    TypedData_Get_Struct((obj), fiber_mon_monitor_t, \
			 &fiber_mon_monitor_data_type, (tobj))

#define FIBER_MON_MONITOR_CHECK_OWNER(obj) \
  { \
    fiber_mon_monitor_t *mon; \
    VALUE th = rb_thread_current(); \
    GetFiberMonMonitorPtr(obj, mon);  \
    if (mon->owner != th) { \
      rb_raise(rb_eThreadError, "current thread not owner"); \
    } \
  }

static void
fiber_mon_monitor_mark(void *ptr)
{
  fiber_mon_monitor_t *mon = (fiber_mon_monitor_t*)ptr;
  
  rb_gc_mark(mon->fiber_mon);
  rb_gc_mark(mon->owner);
  rb_gc_mark(mon->mutex);
}

static void
fiber_mon_monitor_free(void *ptr)
{
  ruby_xfree(ptr);
}

static size_t
fiber_mon_monitor_memsize(const void *ptr)
{
  return ptr ? sizeof(fiber_mon_monitor_t) : 0;
}

static const rb_data_type_t fiber_mon_monitor_data_type = {
    "fiber_mon_monitor",
    {fiber_mon_monitor_mark, fiber_mon_monitor_free, fiber_mon_monitor_memsize,},
};

static VALUE
fiber_mon_monitor_alloc(VALUE klass)
{
  VALUE volatile obj;
  fiber_mon_monitor_t *mon;

  obj = TypedData_Make_Struct(klass, fiber_mon_monitor_t,
			      &fiber_mon_monitor_data_type, mon);
  mon->fiber_mon = Qnil;
  mon->owner = Qnil;
  mon->count = 0;
  mon->mutex = rb_mutex_new();

  return obj;
}

static VALUE
fiber_mon_monitor_initialize(VALUE self, VALUE fiber_mon)
{
  fiber_mon_monitor_t *mon;
  GetFiberMonMonitorPtr(self, mon);

  mon->fiber_mon = fiber_mon;
  
  return self;
}

VALUE
rb_fiber_mon_monitor_new(VALUE fiber_mon)
{
  VALUE mon;
  
  mon = fiber_mon_monitor_alloc(rb_cFiberMonMonitor);
  fiber_mon_monitor_initialize(mon, fiber_mon);
  return mon;
}

VALUE
rb_fiber_mon_monitor_valid_owner_p(VALUE self)
{
  fiber_mon_monitor_t *mon;
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
rb_fiber_mon_monitor_try_enter(VALUE self)
{
  fiber_mon_monitor_t *mon;
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
rb_fiber_mon_monitor_enter(VALUE self)
{
  fiber_mon_monitor_t *mon;
  VALUE th = rb_thread_current();

  GetFiberMonMonitorPtr(self, mon);
  if (mon->owner != th) {
    rb_mutex_lock(mon->mutex);
    mon->owner = th;
  }
  mon->count += 1;
}

VALUE
rb_fiber_mon_monitor_exit(VALUE self)
{
  fiber_mon_monitor_t *mon;
  VALUE th = rb_thread_current();
  
  GetFiberMonMonitorPtr(self, mon);

  FIBER_MON_MONITOR_CHECK_OWNER(self);
  mon->count--;
  if(mon->count == 0) {
    mon->owner = Qnil;
    rb_mutex_unlock(mon->mutex);
  }
}

VALUE
rb_fiber_mon_monitor_synchronize(VALUE self, VALUE (*func)(VALUE), VALUE arg)
{
  rb_fiber_mon_monitor_enter(self);
  return rb_ensure(func, arg, rb_fiber_mon_monitor_exit, self);
}

static VALUE
fiber_mon_monitor_synchronize(VALUE self)
{
  return rb_fiber_mon_monitor_synchronize(self, rb_yield, self);
}

VALUE
rb_fiber_mon_monitor_new_cond(VALUE self)
{
  rb_fiber_mon_cond_new(self);
}

static VALUE
rb_fiber_mon_monitor_entry_fiber(VALUE self, VALUE fiber)
{
  fiber_mon_monitor_t *mon;
  GetFiberMonMonitorPtr(self, mon);

  return rb_fiber_mon_entry_fiber(mon->fiber_mon, fiber);
}

struct fiber_mon_monitor_wait_cond_arg {
  fiber_mon_monitor_t *mon;
  long count;
};

static VALUE
rb_fiber_mon_monitor_wait_cond_yield(VALUE dummy)
{
  return rb_fiber_yield(0, NULL);
}

VALUE
rb_fiber_mon_monitor_wait_cond_ensure(struct fiber_mon_monitor_wait_cond_arg *arg)
{
  fiber_mon_monitor_t *mon = arg->mon;

  mon->owner = rb_thread_current();
  mon->count = arg->count;
}

VALUE
rb_fiber_mon_monitor_wait_cond(VALUE self)
{
  fiber_mon_monitor_t *mon;
  struct fiber_mon_monitor_wait_cond_arg arg;
  
  GetFiberMonMonitorPtr(self, mon);
  arg.count = mon->count;
  mon->owner = Qnil;
  mon->count = 0;
  
  arg.mon = mon;
  rb_ensure(rb_fiber_mon_monitor_wait_cond_yield, Qnil,
	    rb_fiber_mon_monitor_wait_cond_ensure, (VALUE)&arg);
}

typedef struct rb_fiber_mon_cond_struct
{
  VALUE monitor;
  VALUE waitings;
  
} fiber_mon_cond_t;

#define GetFiberMonCondPtr(obj, tobj) \
  TypedData_Get_Struct((obj), fiber_mon_cond_t, \
		       &fiber_mon_cond_data_type, (tobj))

static void
fiber_mon_cond_mark(void *ptr)
{
  fiber_mon_cond_t *cv = (fiber_mon_cond_t*)ptr;
  
  rb_gc_mark(cv->monitor);
  rb_gc_mark(cv->waitings);
}

static void
fiber_mon_cond_free(void *ptr)
{
  ruby_xfree(ptr);
}

static size_t
fiber_mon_cond_memsize(const void *ptr)
{
  return ptr ? sizeof(fiber_mon_cond_t) : 0;
}

static const rb_data_type_t fiber_mon_cond_data_type = {
  "fiber_mon_cond",
  {fiber_mon_cond_mark, fiber_mon_cond_free, fiber_mon_cond_memsize,},
};

static VALUE
fiber_mon_cond_alloc(VALUE klass)
{
  VALUE volatile obj;
  fiber_mon_cond_t *cv;

  obj = TypedData_Make_Struct(klass, fiber_mon_cond_t,
			      &fiber_mon_cond_data_type, cv);
  
  cv->monitor = Qnil;
  cv->waitings = rb_fifo_new();
  
  return obj;
}

static VALUE
fiber_mon_cond_initialize(VALUE self, VALUE monitor)
{
  fiber_mon_cond_t *cv;
  GetFiberMonCondPtr(self, cv);

  cv->monitor = monitor;
  return self;
}

VALUE
rb_fiber_mon_cond_new(VALUE monitor)
{
  VALUE cv;

  cv = fiber_mon_cond_alloc(rb_cFiberMonConditionVariable);
  fiber_mon_cond_initialize(cv, monitor);
  return cv;
}

VALUE
rb_fiber_mon_cond_signal(VALUE self)
{
  fiber_mon_cond_t *cv;
  VALUE fb;

  GetFiberMonCondPtr(self, cv);

  fb = rb_fifo_pop(cv->waitings);
  if (!NIL_P(fb)) {
    rb_fiber_mon_monitor_entry_fiber(cv->monitor, fb);
  }
  return self;
}

VALUE
rb_fiber_mon_cond_broadcast(VALUE self)
{
  fiber_mon_cond_t *cv;
  VALUE fb;

  GetFiberMonCondPtr(self, cv);

  while (!NIL_P(fb = rb_fifo_pop(cv->waitings))) {
    rb_fiber_mon_monitor_entry_fiber(cv->monitor, fb);
  }
  return self;
}

VALUE
rb_fiber_mon_cond_wait(VALUE self)
{
  fiber_mon_cond_t *cv;
  VALUE fb;

  GetFiberMonCondPtr(self, cv);
  rb_fifo_push(cv->waitings, rb_fiber_current());
  
  return rb_fiber_mon_monitor_wait_cond(cv->monitor);
}

VALUE
rb_fiber_mon_cond_wait_until(VALUE self)
{
  while(!RTEST(rb_yield)) {
    rb_fiber_mon_cond_wait(self);
  }
  return self;
}

VALUE
rb_fiber_mon_cond_wait_while(VALUE self)
{
  while(RTEST(rb_yield)) {
    rb_fiber_mon_cond_wait(self);
  }
  return self;
}

Init_fibermon()
{
  rb_cFiberMon = rb_define_class("FiberMon", rb_cObject);
  rb_define_alloc_func(rb_cFiberMon, fiber_mon_alloc);
  rb_define_method(rb_cFiberMon, "initialize", fiber_mon_initialize, 0);
  rb_define_method(rb_cFiberMon, "current", rb_fiber_mon_current, 0);
  rb_define_method(rb_cFiberMon, "mon_start", fiber_mon_mon_start, 0);
  rb_define_alias(rb_cFiberMon,  "start", "mon_start");
  rb_define_method(rb_cFiberMon, "entry_fiber", fiber_mon_entry, -1);
  rb_define_alias(rb_cFiberMon,  "entry", "entry_fiber");
  rb_define_method(rb_cFiberMon, "yield", rb_fiber_mon_yield, 0);
  rb_define_method(rb_cFiberMon, "new_mon", rb_fiber_mon_new_mon, 0);
  rb_define_method(rb_cFiberMon, "new_cv", rb_fiber_mon_new_cv, 0);
  rb_define_alias(rb_cFiberMon,  "new_cond", "new_cv");
  rb_define_method(rb_cFiberMon, "synchronize", rb_fiber_mon_synchronize, 0);
  
  rb_cFiberMonMonitor =
    rb_define_class_under(rb_cFiberMon, "Monitor", rb_cObject);
  rb_define_alloc_func(rb_cFiberMonMonitor, fiber_mon_monitor_alloc);
  rb_define_method(rb_cFiberMonMonitor, "iniialize",
		   fiber_mon_monitor_initialize, 1);
  rb_define_method(rb_cFiberMonMonitor, "try_enter",
		   rb_fiber_mon_monitor_try_enter, 0);
  rb_define_method(rb_cFiberMonMonitor, "enter",
		   rb_fiber_mon_monitor_enter, 0);
  rb_define_method(rb_cFiberMonMonitor, "exit",
		   rb_fiber_mon_monitor_exit, 0);
  rb_define_method(rb_cFiberMonMonitor, "synchronize",
		   fiber_mon_monitor_synchronize, 0);
  rb_define_method(rb_cFiberMonMonitor, "new_cond",
		   rb_fiber_mon_monitor_new_cond, 0);
  rb_define_alias(rb_cFiberMonMonitor,  "new_cv", "new_cond");
  
  rb_cFiberMonConditionVariable =
    rb_define_class_under(rb_cFiberMon, "ConditionVariable", rb_cObject);
  rb_define_alloc_func(rb_cFiberMonConditionVariable, fiber_mon_cond_alloc);
  rb_define_method(rb_cFiberMonConditionVariable, "iniialize",
		   fiber_mon_cond_initialize, 1);
  rb_define_method(rb_cFiberMonConditionVariable, "signal",
		   rb_fiber_mon_cond_signal, 0);
  rb_define_method(rb_cFiberMonConditionVariable, "broadcast",
		   rb_fiber_mon_cond_broadcast, 0);
  rb_define_method(rb_cFiberMonConditionVariable, "wait",
		   rb_fiber_mon_cond_wait, 0);
  rb_define_method(rb_cFiberMonConditionVariable, "wait_until",
		   rb_fiber_mon_cond_wait_until, 0);
  rb_define_method(rb_cFiberMonConditionVariable, "wait_while",
		   rb_fiber_mon_cond_wait_while, 0);
}
