 /**
 * ntable.c
  
  Contributed by John Gibbons
  
 * --------
 *
 * Simple user level Nial hash table mechanism keyed on phrases.
 *
 * Hash tables are Nial data structures consisting of an array
 * of 5 values.
 *
 * They can be saved and restored in workspaces across
 * Nial sessions.
 *
 * - The first value is a unique random phrase to identify the array a hash table
 * - The second and third are arrays of keys and values of the same size.
 * - The fourth entry is an integer array containing the current no of
 *   entries, the no of probes for the last insert, and the number of
 *   deletions.
 * - The final entry is a slot for metadata
 *
 * The size of the array of keys and values is a power of two.
 *
 */

#include "switches.h"

#ifdef NTABLES

/* IOLIB */
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#ifdef UNIXSYS
#include <sys/mman.h>
#endif
#include <sys/fcntl.h>

/* LIMITLIB */
#include <limits.h>

/* STDLIB */
#include <stdlib.h>

/* SJLIB */
#include <setjmp.h>

/* MATHLIB */
#include <math.h>

#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "ops.h"
#include "fileio.h"

#include <string.h>

#define IHT_TBL_SIZE 5
#define IHT_DEF_SIZE 32
#define IHT_CTRL_SIZE 3

/* Control array entries */
#define IHT_NCOUNT  0
#define IHT_NPROBES 1
#define IHT_NDELETE 2

/* Percentage full to expand table */
#define IHT_DEF_EXPAND 70


/* Use a random phrase as the indicator of a hash table */
#define IHT_FLAG "ITH6234046554383511"
static nialptr iht_key;

/* Our null pointer value, this is zero which is not a phrase */
static nialptr iht_null;

/* macro to check for a hash table */
#define ishtable(x) (kind(x) == atype && tally(x) == IHT_TBL_SIZE && fetch_array(x,0) == iht_get_flag_phrase())


static inline int hk_eq(nialptr k1, nialptr k2) {
  if (kind(k1) == kind(k2)) {
    switch(kind(k1)) {
    case inttype:
      return (k1 == k2);
    case chartype:
      return (strcmp(pfirstchar(k1), pfirstchar(k2)) == 0);
    case phrasetype:
      return (k1 == k2);
    default:
      return 0;
    }
  } else {
    return 0;
  }
}


/**
 * Create a randomised phrase as the identifier of
 * hash tables.
 */
static nialptr iht_get_flag_phrase() {
  static int iht_initialised = 0;

  if (iht_initialised == 0) {
    iht_key = makephrase(IHT_FLAG);
    incrrefcnt(iht_key);
    iht_null = createint(0);
    incrrefcnt(iht_null);
    iht_initialised = 1;
  }

  return iht_key;
}


/**
 * Create a Null initialised array of a given size
 */
static nialptr make_htable_part(nialint htsize)
{
  nialint i;
  nialptr res = new_create_array(atype, 1, 0, &htsize);

  for (i = 0; i < htsize; i++) {
    store_array(res, i, Null);
  }

  return res;
}


/**
 * iht_create:
 *
 * Create a user level hashtable, a single argument is provided
 * as an estimate of the hash table size.
 */
void iht_create(void)
{
  nialptr x = apop();
  nialptr res;
  nialptr ht_keys;
  nialptr ht_vals;
  nialptr ht_ctrl;
  nialint ctrl_size = IHT_CTRL_SIZE;
  nialint ihtbl_size = IHT_DEF_SIZE;
  nialint *ctrl_ptr;

  if (isint(x)) {
    nialint xval = intval(x);

    /* allocate the control array */
    res = make_htable_part(IHT_TBL_SIZE);

    /* table size is power of two */
    while(ihtbl_size < xval)
      ihtbl_size *= 2;

    /* create the key and value arrays */
    ht_keys = make_htable_part(ihtbl_size);
    ht_vals = make_htable_part(ihtbl_size);

    /* Create the control array */
    ht_ctrl = new_create_array(inttype, 1, 0, &ctrl_size);
    ctrl_ptr = pfirstint(ht_ctrl);
    ctrl_ptr[IHT_NCOUNT] = 0;
    ctrl_ptr[IHT_NPROBES] = 0;
    ctrl_ptr[IHT_NDELETE] = 0;

    /* build the basic table */
    store_array(res, 0, iht_get_flag_phrase());
    store_array(res, 1, ht_keys);
    store_array(res, 2, ht_vals);
    store_array(res, 3, ht_ctrl);
    store_array(res, 4, Null);

    /* push result */
    apush(res);
    freeup(x);
  } else {
    apush(makefault("?tcreate_arg not an integer"));
    freeup(x);
  }

  return;
}


/**
 * The universal hash function for strings/phrases
 * from Sedgewick's book 'Algorithms in C'
 */

static      nialint
hash(char *s, nialint tsz)
{
  nialint h = 0, a = 31415, b = 27183;

  for(; *s != '\000'; s++, a = a*b % (tsz-1))
    h = (a*h + *s)%tsz;

  return h;
}


/**
 * ht_hash computes two hash keys, one for primary lookup and
 * one for probes. The probe value attempts to use more of the
 * hash key to avoid overlapping chains when two phrases hash
 * to the same value.
 */
static void ht_hash(nialptr hkey, nialint tbsize, nialint *nhash1, nialint *nhash2) {
  static int primes[8] = {
    9923, 9929, 9931, 9941, 9949, 9967, 9973, 10007
  };
  nialint hval;

  switch(kind(hkey)) {
  case phrasetype: 
  case chartype:
    hval = hash(pfirstchar(hkey), tbsize);
    break;

  case inttype:
    hval = intval(hkey);
    break;

  default:
    hval = 0;
  }

  *nhash1 = hval%tbsize;
  *nhash2 = primes[((hval%tbsize)+(hval/tbsize))&07];
}



/**
 * iht_insert1: base code for hashing, insert a single key/value pair.
 *              If there is no space the fnction returns -1, otherwise
 *              the number of probes required to insert the value. The
 *              supplied table arrays are used to simplify
 *              later functions.
 *
 * The itype (insertion type) is used to distinguish between the 
 * different ways that the value is inserted. 
 *          0 - pure new insertion
 *          1 - insertion into an emptied slot
 *          2 - replacement of existing value
 */
static nialint iht_insert1(nialptr keya, nialptr vala, nialptr hkey, nialptr hval, nialint *itype) {
  nialint tkeya = tally(keya);
  nialint hbase, hlook, hprobe;
  nialint nprobes = 0;

  /* assume a new insert */
  *itype = 0;

  /* Get the two hash keys */
  ht_hash(hkey, tkeya, &hbase, &hprobe);

  hlook = hbase;
  for(;;) {
    nialptr klook = fetch_array(keya, hlook);

    nprobes++;

    if (hk_eq(klook,hkey)) {
      /* Found an existing entry, this is a replacement */
      replace_array(vala, hlook, hval);
      *itype = 2;
      return nprobes;
    } else if (klook == Null) {
      /* empty slot, no further chain */
      replace_array(keya, hlook, hkey);
      replace_array(vala, hlook, hval);

      return nprobes;
    } else if (klook == iht_null) {
      /* 
       * deleted slot, can re-use only if same key as original. 
       * Otherwise we may end up with duplicated keys
       */
      nialptr vlook = fetch_array(vala, hlook);
      if (hk_eq(vlook,hkey)) {
        replace_array(keya, hlook, hkey);
        replace_array(vala, hlook, hval);
        *itype = 1;

        return nprobes;
      }
    }
    
    /* If we get here then look for another slot */
    hlook = (hlook + hprobe) % tkeya;

    /* check if we have looped around, i.e. table full */
    if (hlook == hbase)
      return -1;
  }

  return -1; /* should not be reached */
}


/**
 * iht_delete1: support routine to delete an entry by key
 */
static nialint iht_delete1(nialptr keya, nialptr vala, nialptr hkey) {
  nialint tkeya = tally(keya);
  nialint hbase, hlook, hprobe;

  /* Get the two hash keys */
  ht_hash(hkey, tkeya, &hbase, &hprobe);

  hlook = hbase;
  for(;;) {
    nialptr klook = fetch_array(keya, hlook);

    if (hk_eq(klook,hkey)) {
      /* 
       * Found an existing entry. To delete the entry we clear out the 
       * key array slot. And set the value array slot to hodl the key.
       * This will allow us to re-use the slot for the original key
       * while avoiding a duplicated key.
       */
      replace_array(keya, hlook, iht_null);
      replace_array(vala, hlook, keya);
      return 1;
    } else if (klook == Null) {
      return 0;
    } else {
      /* look for another slot */
      hlook = (hlook + hprobe) % tkeya;

      /* check if we have looped around, i.e. table full */
      if (hlook == hbase)
        return 0;
    }
  }

  return -1; /* should not be reached */
}


/**
 * iht_getkeys: gets the list of keys in a hash table.
 * the Nial level routine sorts it.
 */

void iht_getkeys(void)
{ nialptr tbl = apop();


  /* The tbl arg must be a valid hashtable */
  if (!ishtable(tbl)) {
    apush(makefault("?tgetkeys needs a valid hashtable"));

  }
  else {
    nialptr keya, res;
    nialint i, j, tkeya;
    nialptr ctrl = fetch_array(tbl,3);
    nialint *ctrl_ptr = pfirstint(ctrl);
    nialint cnt = ctrl_ptr[IHT_NCOUNT];

    keya = fetch_array(tbl, 1);
    tkeya = tally(keya);
    res = new_create_array(atype,1,cnt,&cnt);
    j = 0;
    for (i = 0; i < tkeya; i++) {
      nialptr hkey = fetch_array(keya, i);
      if (hkey != Null && hkey != iht_null) {
	store_array(res,j,hkey);
	j++;
      }
    }
    apush(res);
  }
  freeup(tbl);
  return;
}


/**
 * iht_rehash: add an array of key/value pairs. this function is
 * only called during a table expansion and all values will fit.
 */
static void iht_rehash(nialptr nkeya, nialptr nvala, nialptr keya, nialptr vala) {
  nialint i;
  nialint tkeya = tally(keya);
  nialint itype = 0;

  for (i = 0; i < tkeya; i++) {
    nialptr hkey = fetch_array(keya, i);
    if (hkey != Null && hkey != iht_null) {
      nialptr hval = fetch_array(vala, i);
      iht_insert1(nkeya, nvala, hkey, hval, &itype);
    }
  }
}


/**
 * iht_insert_value: static routine to implement the actual insertion
 *
 * All arguments are valid at this point. The function will increase
 * the size of the table to guarantee an insertion.
 *
 * The return value is the number of probes required.
 *
 */
static nialint iht_insert_value(nialptr tbl, nialptr hkey, nialptr hval) {
  nialptr keya = fetch_array(tbl, 1);
  nialint tkeya = tally(keya);
  nialptr vala = fetch_array(tbl, 2);
  nialptr ctrla = fetch_array(tbl, 3);
  nialint *ctrl_ptr = pfirstint(ctrla);
  nialint itype = 0;
  nialint nprobes = 0;

  /* Check if we should expand and/or rehash the table */
  if (((100*(ctrl_ptr[IHT_NCOUNT]+ctrl_ptr[IHT_NDELETE]+1))/IHT_DEF_EXPAND) > tkeya) {
    nialptr nkeya;
    nialptr nvala;

    if (((100*(ctrl_ptr[IHT_NCOUNT]+1))/IHT_DEF_EXPAND) > tkeya) {
      /* Expand the table */
      nkeya = make_htable_part(2*tkeya);
      nvala = make_htable_part(2*tkeya);
    } else {
      /* keep the same size */
      nkeya = make_htable_part(tkeya);
      nvala = make_htable_part(tkeya);
    }

    /* Add the existing elements to the new arrays */
    iht_rehash(nkeya, nvala, keya, vala);

    /* add the new entry, this will now succeed */
    nprobes = iht_insert1(nkeya, nvala, hkey, hval, &itype);
    ctrl_ptr = pfirstint(ctrla);
    /* Check that this is not just a replacement */
    if (itype != 2)
      ctrl_ptr[IHT_NCOUNT]++;            /* one more entry */
    ctrl_ptr[IHT_NPROBES] = nprobes;   /* probe count    */
    ctrl_ptr[IHT_NDELETE] = 0;         /* rehash cleared this */

    /* put new arrays into the table */
    replace_array(tbl, 1, nkeya);
    replace_array(tbl, 2, nvala);

    return nprobes;
  } else {
    /* directly insert entry */
    nprobes = iht_insert1(keya, vala, hkey, hval, &itype);
    ctrl_ptr = pfirstint(ctrla);
    /* If just a replacement the count does not increase */
    if (itype != 2)
      ctrl_ptr[IHT_NCOUNT]++;
    ctrl_ptr[IHT_NPROBES] = nprobes;

    /* Did the insert replace a deleted entry */
    if (itype == 1)
      ctrl_ptr[IHT_NDELETE]--;

    return nprobes;
  }
}


/**
 * Retrieve a value from a hash table
 */
static int iht_get_value(nialptr tbl, nialptr hkey, nialptr *hval)
{
  nialptr keya = fetch_array(tbl, 1);
  nialptr vala = fetch_array(tbl, 2);
  nialint tkeya = tally(keya);
  nialint hbase, hlook, hprobe;
  int not_finished = 0;

  /* Get the two hash keys */
  ht_hash(hkey, tkeya, &hbase, &hprobe);

  hlook = hbase;
  while(not_finished == 0) {
    nialptr klook = fetch_array(keya, hlook);

    if (hk_eq(klook,hkey)) {
      /* Found the entry */
      *hval = fetch_array(vala, hlook);
      return 0;
    } else if (klook == Null) {
      /* empty slot, no further chain */
      return 1;
    } else {
      /* look for another slot */
      hlook = (hlook + hprobe) % tkeya;
      if (hlook == hbase) {
	    /* looped around without finding data */
	    not_finished = 1;
      }
    }
  }

  return 1;
}

/**
 * iht_set:
 *
 * add an entry to a  hash table. This primitive is used in the form
 *
 *          tbl htset [key, val]
 *   or     htset [tbl, [key, val]]
 *
 * key must be a phrase.
 */
void iht_set()
{
  nialptr args = apop();
  nialptr tbl, hdata;

  /* args must be a pair */
  if (tally(args) != 2) {
    apush(makefault("?tset_args expects a pair"));
    freeup(args);
    return;
  }

  /* Split arguments into table and key/val pair */
  splitfb(args, &tbl, &hdata);

  /* The tbl arg must be a valid hashtable */
  if (ishtable(tbl)) {
    if (tally(hdata) == 2) {
      nialptr hkey, hval;
      splitfb(hdata, &hkey, &hval);

      /* key must be a phrase */
      if (kind(hkey) == phrasetype || kind(hkey) == chartype || kind(hkey) == inttype) {
	/* -- do the insert -- */
	iht_insert_value(tbl, hkey, hval);
	apush(hval);
	freeup(args);
	return;
      } else {
	apush(makefault("?tset invalid key"));
	freeup(args);
	return;
      }

    } else {
      apush(makefault("?tset needs a key/value pair"));
      freeup(args);
      return;
    }
  } else {
    apush(makefault("?tset needs a valid hashtable"));
    freeup(args);
    return;
  }
}


/**
 * ht_get - retrieve a value from a hashtable
 *
 *    tbl ht_get phrase
 */
void iht_get()
{
  nialptr args = apop();
  nialptr tbl, hkey, hright, hdef;

  /* args must be a pair */
  if (tally(args) != 2) {
    apush(makefault("?tget expects a pair"));
    freeup(args);
    return;
  }

  /* Split arguments into table and key */
  splitfb(args, &tbl, &hright);

  if(tally(hright) == 1 || kind(hright) == chartype) {
    hkey = hright;
    hdef = invalidptr;
  } else if (tally(hright) == 2) {
    splitfb(hright, &hkey, &hdef);
  } else {
    apush(makefault("?tget second arg not a pair"));
    freeup(args);
    return;
  }

  /* The tbl arg must be a valid hashtable */
  if (ishtable(tbl)) {
    /* key must be a phrase */
    if (kind(hkey) == phrasetype || kind(hkey) == chartype || kind(hkey) == inttype) {
      nialptr hval;
      if (iht_get_value(tbl, hkey, &hval) == 0) {
	/* found an entry */
	apush(hval);
      } else {
	/* No entry found */
	if(hdef == invalidptr) {
	  apush(makefault("?tget found no entry"));
	} else {
	  apush(hdef);
	}
      }
    } else {
      apush(makefault("?tget invalid key"));
    }
  } else {
    apush(makefault("?tget invalid table"));
  }

  freeup(args);
  return;
}


void iis_ht_table(void) {
  nialptr x = apop();

  if (ishtable(x)) {
    apush(True_val);
  } else {
    apush(False_val);
  }

  freeup(x);
  return;
}


/**
 * iht_setmeta:
 *
 * set the metadata for a hash table. This primitive is used in the form
 *
 *          tbl htSetMeta val
 *   or     htsetmeta [tbl, val]
 *
 * metadata is anything defined by the programmer
 */
void iht_setmeta()
{
  nialptr args = apop();
  nialptr tbl, mdata;

  /* args must be a pair */
  if (tally(args) != 2) {
    apush(makefault("?tSetMeta expects a pair"));
    freeup(args);
    return;
  }

  splitfb(args, &tbl, &mdata);
  if (ishtable(tbl)) {
    replace_array(tbl, 4, mdata);
    apush(mdata);
  } else {
    apush(makefault("?tSetMeta not table"));
  }

  freeup(args);
  return;
}


void iht_delete()
{
  nialptr args = apop();
  nialptr tbl, key;

  if(tally(args) == 2) {
    splitfb(args, &tbl, &key);

    if (ishtable(tbl) && (kind(key) == phrasetype || kind(key) == chartype || kind(key) == inttype))  {
      nialptr keya = fetch_array(tbl, 1);
      nialptr vala = fetch_array(tbl, 2);
      nialint res = iht_delete1(keya, vala, key);

      /* decrement the count of entries in the table */
      if (res == 1) {
	nialptr ctrla = fetch_array(tbl, 3);
	nialint *ctrl_ptr = pfirstint(ctrla);
	ctrl_ptr[IHT_NCOUNT]--;
	ctrl_ptr[IHT_NDELETE]++;
      }

      apush(createint(res));
    } else {
      apush(makefault("?tdel_args"));
    }
  } else {
    apush(makefault("?tdel_args"));
  }

  freeup(args);
  return;
}


void iht_getmeta()
{
  nialptr tbl = apop();

  if (ishtable(tbl)) {
    apush(fetch_array(tbl, 4));
  } else {
    apush(makefault("?tgetmeta expects table"));
  }

  freeup(tbl);
  return;
}

#endif /* NTABLES */
