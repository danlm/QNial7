/*==============================================================

  ABSMACH.H:  HEADER FOR ABSMACH.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains the macros that support the abstract machine model,
   i,e. heap memory management, the stack, the atom table and array
   representation.


  ================================================================*/


/*
The heap memory management routines of Q'Nial view the available memory
as a contiguous collection of words which are addressed by integer
offsets. Each word holds one integer of C data. The memory is dynamically
allocated in blocks consisting of an even number of words. The insistence
on using blocks of even length forces all C double precision floating
point numbers to align on double-word boundaries (for machines that use
two 32 bit words for a 64-bit double floating point representation.) This
is required for some architectures (SPARC, RS6000) and usually faster on most
byte-oriented architectures (Inteel 80x86, Motorola 680X0).
(We assume that malloc() allocates the heap array mem[] on such a boundary.)


  The memory block layout:

An allocated block has the following format:
 +------+--------+----------------------+-------+-------------+--------------+
 | size | refcnt | sortflg,kind,valence | tally | data ...| U | shape vector |
 +------+--------+----------------------+-------+-------------+--------------+
 ^                                              ^
 |                                              |
 block address                                  array address

where is U represents some unused space if the block is larger than needed.


When the block is free it has the form:
 +-----+---------+------+--+---------+-----+-----------+
 |size | FREETAG | fwdlink | bcklink | ... | -(hdrptr) |
 +-----+---------+------+--+---------+-----+-----------+
 ^
 |
 block address

The block address is used by memory management, and the array address is
used for referring to the array the block contains. They relate by
           block address + hdrsize = array address.

The block header size is an even number of nialints to ensure that the array
address is aligned. The tally field in the header is redundant, it could
be computed from the shape, but is included to take advantage of the space
needed to ensure alignment. (An experiment could be tried with a header
size of two words, moving the reference count to the end of the block.
However, it is believed that the extra cost in accessing the reference
count field and computing the tally would be less preferable.)

Both the block and array addresses are even integer offsets in the
range of an array of nialints (currently equal to int on all systems).
Within the header, the third word packs in the valence, the storage kind
and a flag indicating whether the array is in lexical order. (The latter
permits a speedup on certain searching operations.)

In the data field, the data is packed, either consisting of actual data
stored as bits, bytes, words, double words, or as integer offsets to
other heap entries.

The shape is attached at the end. Keeping the tally is redundant, but
the space is available to achieve alignment and its presence reduces
overhead in many routines.

Any array, atom or otherwise, can be represented in this layout.

The macros that refer to array properties use the array address,
the ones that refer to memory management use the block address.

The heap management is done with a double linked list of free areas.
A free block can be detected from either end. From the front, the
refcnt field of the header is -1 for a free block or >=0 for an
allocated array. From the end, the last word is a negative number whose
opposite is the offset for the block (its block address) for a free
block, or the last word of the shape for an allocated array. For an
array with no axes (a single, all atoms), the last word must be a zero
to mark that it is allocated. (This requirement forces real number atoms
to need at least 7 words and eight are used to preserve alignment. This
could be reduced to 6 if the above mentioned short header idea were
exploited.)

The use of allocation tags at both ends of the block allows
adjacent free blocks to be merged without requiring that the
free list be kept in memory order. The latter technique (used in
earlier versions) can lead to excessively slow memory recovery
in situations where a large number of small free blocks are generated.

*/



/*  the minimum size allowed to be left as a free block. This
    is space for the header and trailer. It is large enough for
    integer, boolean,real  and character atoms. We need two words for
    a double float for reals and it works best if all atoms occupy the
    same size container.  The trailer word is needed to mark an empty
    block from an allocated one, and for a real atom, this cannot
    overlap the data. For an empty list, the trailer will be 0 when
    allocated (shape).
*/

/* #include "unixif.h" */    /* MAJ put here so it is in all C files It could be  */

#define MACRO_BOOL_BITS 1

/* macros for memory allocation purposes. bx is a block address */

/* block header macros */

#define FREETAG (-1)         /* indicates that the block is free. */
#define TERMINATOR (-3)      /* used to terminate the fwdlink chain */
#define LOCKEDBLOCK (-5)     /* used for the final block in freelist */

/* size field */
#define blksize(bx)   ((nialhdr*)&mem[bx])->size
#define set_blksize(bx,n)   ((nialhdr*)&mem[bx])->size = n

/* free tag field */
#define freetag(bx)   ((nialhdr*)&mem[bx])->ref_count
#define set_freetag(bx) ((nialhdr*)&mem[bx])->ref_count = FREETAG
#define set_lockedtag(bx) ((nialhdr*)&mem[bx])->ref_count = LOCKEDBLOCK
#define reset_freetag(bx)   ((nialhdr*)&mem[bx])->ref_count = 0

/* allocated or free tests */
#define allocated(bx)   (((nialhdr*)&mem[bx])->ref_count != FREETAG  && ((nialhdr*)&mem[bx])->ref_count != LOCKEDBLOCK)
#define isfree(bx)   (bx < memsize && ((nialhdr*)&mem[bx])->ref_count == FREETAG)
#define islocked(bx)   (bx < memsize && ((nialhdr*)&mem[bx])->ref_count == LOCKEDBLOCK)


/* link fields */
#define fwdlink(bx)   ((nialhdr*)&mem[bx])->hdrdata.free_block.fwd_link
#define bcklink(bx)   ((nialhdr*)&mem[bx])->hdrdata.free_block.bck_link

/* trailer handling macros */
#define endptr(bx) *(((nialptr*)&mem[bx+blksize(bx)])-1)
#define set_endinfo(bx) endptr(bx) = (-bx)
#define reset_endinfo(bx) endptr(bx) = 0

#define isprevfree(bx) (*(((nialptr*)&mem[bx])-1) < 0)
#define prevblk(bx) (-(*(((nialptr*)&mem[bx])-1)))

/* macro to get array pointer from block pointer */
#define arrayptr(bx) (bx+hdrsize)


/* macros for the array header. x is an array address  */

/* macro to get block pointer from array pointer */
#define blockptr(x) (x-hdrsize)
#define dataptr(x)  (nialword*)(&mem[x])


/* The skv word holds the sort flag (first byte),
                      the kind (second byte), and
                      the valence (last two bytes).
*/

struct skv {
  char        sk[2];
  short       val;
};

#define sorted(x) ((nialhdr*)&mem[blockptr(x)])->hdrdata.allocatedblock.flags.skv.sk[0]
#define is_sorted(x) (sorted(x)==1)

#define kind(x) ((nialhdr*)&mem[blockptr(x)])->hdrdata.allocatedblock.flags.skv.sk[1]
#define valence(x) ((nialhdr*)&mem[blockptr(x)])->hdrdata.allocatedblock.flags.skv.val

#define set_kind(x,k) kind(x) = (char) k
#define set_valence(x,v) valence(x) = v
#define set_sorted(x,s) sorted(x) = (char) s

/* reference count field */
#define refcnt(x) ((nialhdr*)&mem[blockptr(x)])->ref_count
#define set_refcnt(x,y)    refcnt(x) = y

/* tally field */
#define tally(x) ((nialhdr*)&mem[blockptr(x)])->hdrdata.allocatedblock.block_tally
#define set_tally(x,n) tally(x) = n

/* ???? what is the type of shape */
/* macro to get the shape */
#define shpptr(x,v) (((nialint*)&mem[blockptr(x)+blksize(blockptr(x))])-v)


/* macros to get appropriately typed C pointers
  to the beginning of the data */
#define pfirstchar(x) (char *)dataptr(x)
#define pfirstint(x) (nialint *)dataptr(x)
#define pfirstitem(x) (nialptr*)dataptr(x)  /* already a (nialptr *) */
#define pfirstreal(x) (double *)dataptr(x)

/* fetch and store macros for the homogeneous arrays.
   The val version is used on atoms */

/* inttype */
#define fetch_int(x,i)   (*(pfirstint(x)+i))
#define store_int(x,i,n) *(pfirstint(x)+i) = n
#define intval(x) (*(pfirstint(x)))

/* chartype */
#define fetch_char(x,i) (*(pfirstchar(x) + i))
#define store_char(x,i,c) (*(pfirstchar(x) + i) = c)
#define charval(x) (*pfirstchar(x))

/* booltype */
#ifdef OLD_BOOL_ORDER
#define retrieve_bit(w,p) (((nialint)(w)>>(p))&1) /* retrieves a bit from a word  */
#else
#define retrieve_bit(w,p) (((nialint)(w)>>(BoolPackBase - (p)))&NIALONEBIT)
#define store_bit(_w,_p,_o,_b) store_int(_w,_p,(((fetch_int(_w,_p)&~(NIALONEBIT<<(BoolPackBase-(_o))))) | (((nialint)_b)<<(BoolPackBase-(_o)))))
#endif

#ifdef MACRO_BOOL_BITS
#define fetch_bool(x,i)  retrieve_bit(fetch_int( x,(i)/boolsPW ),((i)%boolsPW))
#define store_bool(x,i,b) store_bit(x,(i)/boolsPW, (i)%boolsPW,((b)&NIALONEBIT))
#endif

/* store_bool(x,i,b) is a routine */
#define boolval(x) fetch_bool(x,0)

/* realtype */
#define fetch_real(x,i) (*(pfirstreal(x) + i))
#define store_real(x,i,r) *(pfirstreal(x) + i) = r
#define realval(x)   (*pfirstreal(x))


/* macros for atom table types (phrases and faults) */
#define tknchar(x,i)  fetch_char(x,i)
#define tknlength(x) (strlen(pfirstchar(x)))

#define invalidptr       (nialptr)(-1)  /* Used to fill arrays in
                                         * create_array */

/* macros to define the array representation kinds */

/* code in the system makes assumptions about the order of the types below */

#define atype 1
#define booltype 2
#define inttype 3
#define realtype 4
#define chartype 6
#define phrasetype 7
#define faulttype 8

/* tests on arrays */
#define atomic(x) (kind(x) >= booltype && valence(x)==0)
#define istext(x) (kind(x) ==chartype || kind(x)==phrasetype || tally(x)==0)
#define isstring(x) ((kind(x)==chartype || tally(x)==0 ) && valence(x)==1)
#define isint(x) (kind(x)==inttype && valence(x)==0)
#define isnonnegint(x) (isint(x) && intval(x)>=0)
#define isposint(x) (isint(x) && intval(x)>0)

/* tests on kinds */
#define homotype(k) (k>=booltype && k<=chartype)
#define numeric(k) (k>=booltype && k<=realtype)
#define tknkind(k) (k >= phrasetype)

/* macros associated with the atom table for phrases and faults */

#define linadj 239           /* linear adjustment in hash scheme. This must
                              * be a prime number. */

#define vacant (nialptr)(-8) /* Vacant hash table location. */
#define held   (nialptr)(-4) /* held hash table location */


#ifndef DEBUG

/* reference count macros. Replaced by routines in DEBUG version */
#define incrrefcnt(x) refcnt(x)++
#define decrrefcnt(x) refcnt(x)--

#endif


/* the following macros are used instead of procedures if the corresponding
   switches are turned on. These achieve in-line procedures in C in an
   inconvenient way. If apush(x); is used before an else, put it in { }
   to avoid a C syntax error. Similarly with storearray.
*/

/* *************** Uncertaing of these ****************** */
#ifdef FETCHARRAYMACRO
#define fetch_array(_x,i) (*(((nialptr*)&mem[_x])+i))
#endif

#ifdef STOREARRAYMACRO
#define store_array(_x,i,v) { nialptr _v_ = v; incrrefcnt(_v_);	\
    *(((nialptr*)&mem[_x])+i) = _v_; }
#endif

#ifdef FREEUPMACRO
#define freeup(x) { nialptr _y_ = x; if (refcnt(_y_)==0) freeit(_y_); }
#else
#define freeup(x) freeit(x)
#endif



/* macros used with the stack */

#define top           stkarea[topstack]
#define topm1         stkarea[topstack-1]


#define swap() { register nialptr _y_ = topm1; topm1 = top; top = _y_; }
#define movetop(n) { stkarea[topstack-(n)] = stkarea[topstack]; topstack--; }
#define fastpop() (topstack==(-1) ? stackempty() : stkarea[topstack--])


#ifdef STACKMACROS

/* macros to do apush and apop without procedure calls */

#define apush(x)   { register nialptr _y_ = x;\
  if (++topstack == stklim) growstack(); stkarea[topstack] = _y_;\
  incrrefcnt(_y_); }

/* apop macro uses global _x_ */
#define apop() (topstack==(-1) ? stackempty() : (_x_ = stkarea[topstack],\
	       stkarea[topstack--] = invalidptr, decrrefcnt(_x_), _x_))

#endif

/* below are a few definitions allowing to use a C buffer for strictly local
   work in C routines, or for local work with a stack discipline if one takes
   care to maintain ptrCbuffer correctly */

#define Cbufferfull (ptrCbuffer == finCbuffer)
#define Cbufferempty (ptrCbuffer == startCbuffer)
#define reservechars(n) { if (finCbuffer - ptrCbuffer < n) extendCbuffer(n); }

#define pushch(a) {if(Cbufferfull)extendCbuffer(INITCBUFFERSIZE);\
*ptrCbuffer++ = a;}

#define popch (*(--ptrCbuffer))


#ifndef DEBUG

/* macro to implement a copy of one item. Avoids a procedure
  call in many inner loops. Replaced by a procedure in DEBUG mode */

#define copy1(z, sz, x, sx) {\
  int _kx = kind(x);\
  switch(_kx)\
  {\
    case atype:\
      { nialptr _xi = fetch_array(x,sx);\
        store_array(z,sz,_xi);\
        break;\
       }\
    case chartype: store_char(z,sz,fetch_char(x,sx)); break;\
    case booltype: store_bool(z,sz,fetch_bool(x,sx)); break;\
    case inttype: store_int(z,sz,fetch_int(x,sx)); break;\
    case realtype: store_real(z,sz,fetch_real(x,sx)); break;\
  }\
}

#endif

/* function prototypes for routines exported from absmach.c */

extern void extendCbuffer(nialint n);
extern void reserveints(nialint n);

extern void deallocate_atomtbl(void);
extern void freeit(nialptr x);
extern nialint pickshape(nialptr x, nialint i);
extern nialptr new_create_array(int k, int v, nialint t, nialint *extents);
extern nialptr stackempty(void);

extern nialptr createatom(unsigned int k, char *s);
extern nialptr createint(nialint x);
extern nialptr createbool(int x);
extern nialptr createchar(char c);
extern nialptr createreal(double x);
extern nialptr createcplx(double val[2]);
extern void mkstring(char *buf);
extern nialptr makestring(char *buf);
extern void mknstring(char *buf, nialint n);
extern nialptr makephrase(char *s);
extern nialptr makefault(char *f);
extern void buildfault(char *msgptr);
#ifndef MACRO_BOOL_BITS
extern int fetch_bool(nialptr x, nialint i);
#endif
#ifndef STACKMACROS
extern void apush(nialptr x);
extern nialptr apop(void);

#endif

extern void copy(nialptr z, nialint sz, nialptr x, nialint sx, nialint cnt);

#ifdef DEBUG
extern void copy1(nialptr z, nialint sz, nialptr x, nialint sx);

#endif
extern nialptr fetchasarray(nialptr x, nialint i);
extern nialptr explode(nialptr x, int v, nialint t, nialint s, nialint e);
extern nialptr implode(nialptr x);
extern void clearstack(void);
extern void clearheap(void);

#ifdef DEBUG
void nabort(int flag);
#endif

#ifdef DEBUG
extern void incrrefcnt(nialptr x);
extern void decrrefcnt(nialptr x);

#endif

extern void mklist(nialint n);
extern nialptr nial_extend(nialptr x, nialptr y);
extern void replace_array(nialptr z, nialint i, nialptr x);
#ifndef MACRO_BOOL_BITS
extern void store_bool(nialptr z, nialint i, int b);
#endif
#ifndef FETCHARRAYMACRO
extern nialptr fetch_array(nialptr x, nialint i);

#endif

#ifndef STOREARRAYMACRO
extern void store_array(nialptr x, nialint i, nialptr z);

#endif

extern int  equalshape(nialptr x, nialptr y);
extern void expand_heap(nialint n);
extern void setup_abstract_machine(nialint initialmemsize);
extern void clear_abstract_machine(void);
extern nialint *calcshpptr(nialptr x, int v);
extern int  equalsymbol(nialptr x, char *s);
extern nialptr stackempty(void);
extern void growstack(void);
extern int  homotest(nialptr x);
extern nialint checkavailspace(void);
extern void checkfortemps(void);

extern int doprintf;


#define my_realloc(p,s,os) realloc(p,s)


/* defines to hanlde case insensitive string compares */

#define STRCASECMP strcasecmp
#define STRNCASECMP strncasecmp


/* changes to support introspection for debugging */

extern nialint set_delayed_release_flag(nialint flag);
extern void get_validation_counters(nialint *afc, nialint *baic, nialint * tmc);
extern void validate_delayed_block_list();
extern nialint get_delayed_release_count();
extern nialint validate_block_structure(nialptr x);


