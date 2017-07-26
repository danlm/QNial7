/*==============================================================

  PROFILE.H:  header for PROFILE.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains the macros and prototypes for the profiling functions

================================================================*/

/* access Macros for callgraph node */

#define set_opid(_X_,_Y_) (_X_->opid = _Y_)
#define set_parent(_X_,_Y_) (_X_->parent = _Y_)
#define set_start_time(_X_,_Y_) (_X_->start_time = _Y_)
#define set_end_time(_X_,_Y_) (_X_->end_time = _Y_)
#define set_total_time(_X_,_Y_) (_X_->total_time = _Y_)
#define set_total_calls(_X_,_Y_) (_X_->total_calls = _Y_)
#define inc_total_calls(_X_) (_X_->total_calls++)
#define inc_num_children(_X_) (_X_->num_children++)
#define add_time(_X_) (_X_->total_time += (_X_->end_time - _X_->start_time))

/* size constants for profiler */

#define LOCLISTSIZE 20
#define SYMLISTSIZE 50
#define NODESIZE 5

extern void profile_ops_start(nialptr entr);
extern void profile_ops_stop(nialptr entr);
extern void clear_profiler(void);
