/*==============================================================

MODULE   REGEXP.C

COPYRIGHT NIAL Systems Limited  1983-2016

This contains low level Nial operations used by regexp.ndf code
included in defs.ndf if REGEXP is defined.
 
 Contributed by John Gibbons

================================================================*/


/* Q'Nial file that selects features */

#include "switches.h"

#ifdef REGEXP /* ( */

/* standard library header files */

/* IOLIB */
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/fcntl.h>

/* LIMITLIB */
#include <limits.h>

/* STDLIB */
#include <stdlib.h>

/* SJLIB */
#include <setjmp.h>

/* STLIB */
#include <string.h>

/* REGEXLIB */
#include <regex.h>


/* Q'Nial header files */

#include "absmach.h"
#include "qniallim.h"
#include "lib_main.h"
#include "ops.h"
#include "trs.h"
#include "fileio.h"



#define FIRSTCHAR_OR_NULL(a,i) (tally(fetch_array(a,i))?pfirstchar(fetch_array(a,i)):"")


/**
 *  Hold precompiled regular expressions.
 *  These are indexed by an integer in the range 0..16
 */
static regex_t *re_compiled[16] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
				   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
 

/**
 * The Posix flags to match to Nial
 */
#define NUM_OPTS 6
static int posix_flags[NUM_OPTS] = { REG_EXTENDED, REG_ICASE, REG_NOSUB, 
				     REG_NEWLINE, REG_NOTBOL, REG_NOTEOL};

/**
 * translate Nial options to Posix options 
 */
static int reg_options(nialint opts) {
  int i, mask = 1, res = 0;

  for (i = 0; i < NUM_OPTS; i++, mask <<= 1)
    if (mask&opts) {
      res |= posix_flags[i];
    }
  
  return res;
}



/**
 * The following code implements the nial primitive to compile 
 * a regular expression
 *
 *    regexp_c  <index> <regex string> <options>
 */
void
iregexp_c(void)
{
  nialptr     z = apop();
  nialptr     re_index;
  nialptr     re_string;
  nialptr     re_options; 
  nialint     index;
  int         res;
  regex_t *re;


  /* validate the arguments */

  if (tally(z) != 3 || kind(z) != atype) {
    apush(makefault("?args"));
    freeup(z);
    return;
  }

  /* Extract the args */
  re_index = fetch_array(z, 0);
  re_string = fetch_array(z, 1);
  re_options = fetch_array(z, 2);

  /* validate types */
  if (!isint(re_index) || !isint(re_options) || kind(re_string) != chartype || tally(re_string) == 0) {
    apush(makefault("?argtypes"));
    freeup(z);
    return;
  }

  /* Clear any previously compiled regex */
  index = intval(re_index) & 0x0f;
  if (re_compiled[index] != NULL) {
    regfree(re_compiled[index]);
    re_compiled[index] = NULL;
  }
    
  /* allocate a regex_t structure */
  re = (regex_t*)malloc(sizeof(regex_t));
  if (re == NULL) {
    apush(makefault("?malloc error"));
    freeup(z);
    return;
  }

  /* compile the regex */
  res = regcomp(re, pfirstchar(re_string), reg_options(intval(re_options)));
  if (res == 0)
    re_compiled[index] = re;
  else
  {
    regfree(re);
      apush(makefault("?regex compile error"));
      freeup(z);
      return;
  }

  /* return the result true for success */
  apush(True_val);
  freeup(z);
  return;
}


/**
 * The following code implements the nial primitive to test 
 * for a match against a regular expression
 *
 *    regexp_t  <regex index> <match string> <options>
 *
 * The function returns true is there is a match, or false if not.
 */
void
iregexp_t(void)
{
  nialptr     z = apop();
  nialptr     re_index;
  nialptr     re_string;
  nialptr     re_options; 
  nialint     index;
  int         res;
  regex_t     *re;
  char        *p = "";

  /* validate the arguments */
  if (tally(z) != 3 || kind(z) != atype) {
    apush(makefault("?args"));
    freeup(z);
    return;
  }

  /* Extract the args */
  re_index = fetch_array(z, 0);
  re_string = fetch_array(z, 1);
  re_options = fetch_array(z, 2);

  /* validate integer types */
  if (!isint(re_index) || !isint(re_options)) {
    apush(makefault("?argtypes"));
    freeup(z);
    return;
  }
  
  /* handle some immediate failures */
  if (kind(re_string) != chartype && re_string != Null) {
    apush(makefault("?argtypes"));
    freeup(z);
    return;
  }

  /* Check for a compiled regex */
  index = intval(re_index) & 0x0f;
  if (re_compiled[index] == NULL) {
    apush(makefault("?regex"));
    freeup(z);
    return;
  }
    
  /* get the regex_t structure */
  re = (regex_t*)re_compiled[index];

  /* get the string to be matched, null case already handled */
  if (kind(re_string) == chartype)
    p = pfirstchar(re_string);

  /* perform the match */
  res = regexec(re, p, 0, NULL, reg_options(intval(re_options)));
  if (res == 0) {
    apush(True_val);
  } else if (res == REG_NOMATCH) {
    apush(False_val);
  } else {
    apush(makefault("?memory"));
  }

  freeup(z);
  return;
}


/**
 * The following code implements the nial primitive to match 
 * against a regular expression
 *
 *    regexp_t  <regex index> <match string> <nmatch> <options>
 *
 * The function returns a Nial array structure of the matching groups or Null
 */
void
iregexp_m(void)
{
  nialptr     z = apop();
  nialptr     re_index;
  nialptr     re_string;
  nialptr     re_options;
  nialptr     re_nmatch;
  nialint     index, i;
  int         res;
  regmatch_t  match[128];
  regmatch_t  *matches = match;
  nialint     nmatch;
  regex_t     *re;
  char        *p = "";   

  /* validate the arguments */
  if (tally(z) != 4 || kind(z) != atype) {
    apush(makefault("?args"));
    freeup(z);
    return;
  }

  /* Extract the args */
  re_index = fetch_array(z, 0);
  re_string = fetch_array(z, 1);
  re_nmatch = fetch_array(z, 2);
  re_options = fetch_array(z, 3);

  /* validate numeric args */
  if (!isint(re_index) || !isint(re_nmatch) || intval(re_nmatch) < 0 || !isint(re_options)) {
    apush(makefault("?argtypes"));
    freeup(z);
    return;
  }

  /* validate string argument */
  if (kind(re_string) != chartype && re_string != Null) {
    apush(makefault("argtypes"));
    freeup(z);
    return;
  } else {
    /* Null case already handled */
    if (kind(re_string) == chartype)
      p = pfirstchar(re_string);
  }

  /* Check for a compiled regex */
  index = intval(re_index) & 0x0f;
  if (re_compiled[index] == NULL) {
    apush(makefault("?regex"));
    freeup(z);
    return;
  }
    
  /* get the regex_t structure */
  re = (regex_t*)re_compiled[index];

  /* sort out the number of matches */
  nmatch = intval(re_nmatch);
  nmatch = (nmatch > 128)? 128: nmatch;
  for(i = 0; i < nmatch; i++) {
    matches[i].rm_so = -1;
    matches[i].rm_eo = -1;
  }

  /* perform the match */
  res = regexec(re, p, nmatch, match, reg_options(intval(re_options)));
  if (res == 0) {
    nialptr ares;
    nialint mcount = 0;
    nialint elen = 2;
    nialint i;

    /* count the real matches */
    for (i = 0; i < nmatch; i++)
      if(match[i].rm_so == -1)
	break;
      else
	mcount++;
    /* Create the result array */
    ares = new_create_array(atype, 1, mcount, &mcount);
    

    /* copy substring details to the result */
    for (i = 0; i < mcount; i++) {
      nialptr ent = new_create_array(inttype, 1, elen, &elen);
      store_int(ent,0, match[i].rm_so);             /* starting point */
      store_int(ent,1, match[i].rm_eo - matches[i].rm_so); /* length */
      store_array(ares, i, ent);
    }
    apush(ares);
  }
  else if (res == REG_NOMATCH) {
    apush(Null);
  }
  else {
    apush(makefault("match failed in regexp search"));
  }

  freeup(z);
  return;
}


/**
 * The following code implements a simple nial primitive to extract
 * substrings.
 *
 *    re_extract <string> [<start> <end>]
 */
void
iregexp_extract(void)
{
  nialptr     z = apop();
  nialptr     re_dims;
  nialptr     re_string;
  nialint     re_start;
  nialint     re_len;
  nialptr     res;
  char        *p;


  /* validate the arguments */
  if (tally(z) != 2 || kind(z) != atype) {
    apush(makefault("?args"));
    freeup(z);
    return;
  }

  /* Split the arguments */
  splitfb(z, &re_string, &re_dims);
  if (kind(re_dims) != inttype || tally(re_dims) != 2) {
    apush(makefault("?args"));
    freeup(z);
    return;
  } else {
    nialint *ip = pfirstint(re_dims);
    re_start = *ip++;
    re_len   = *ip++;
  }

  /* handle the case of a null input string or a null extraction */
  if (re_string == Null || kind(re_string) != chartype || re_len == 0) {
    apush(Null);
    freeup(z);
    return;
  } else {
    p = pfirstchar(re_string);
  }
  
  /* Allow negative start position to mean chars from end */
  if (re_start < 0)
    re_start += strlen(p);
 
  /* validate substring ranges */
  if (re_start < 0 || re_len < 0 || (re_start + re_len) > strlen(p)) {
    apush(makefault("?range"));
    freeup(z);
    return;
  }

  /* finally extract the substring */
  res = new_create_array(chartype, 1, 0, &re_len);

  p += re_start;
  strncpy(pfirstchar(res), p, re_len);
  apush(res);
  freeup(z);
  return;
}

/**
 * The following code implements a simple nial primitive to split
 * a string around a series of substrings.
 *
 *    re_split <string> [[<start> <end>]...]
 */
void
iregexp_split(void)
{
  nialptr     z = apop();
  nialptr     re_dims;
  nialptr     re_string;
  nialptr     res;
  nialint     nslices, ndims, i, next = 0;
  char        *p;


  /* validate the arguments */
  if (tally(z) != 2 || kind(z) != atype) {
    apush(makefault("?args"));
    freeup(z);
    return;
  }

  /* Split the arguments */
  splitfb(z, &re_string, &re_dims);

  /* handle the abnormal cases immediately */
  if (re_string == Null || kind(re_string) != chartype) {
    apush(Null);
    freeup(z);
    return;
  } else {
    p = pfirstchar(re_string);
  }

  /* Check the dimensions array */
  if (kind(re_dims) != atype) {
    apush(makefault("?args"));
    freeup(z);
    return;
  } 

  /* Validate the dimensions before we start */
  ndims = tally(re_dims);
  for (i = 0; i < ndims; i++) {
    nialptr adim = fetch_array(re_dims, i);
    nialint *ptr, pstart, plen;

    if (kind(adim) != inttype || tally(adim) != 2) {
      apush(makefault("?ranges"));
      freeup(z);
      return;
    }

    ptr = pfirstint(adim);
    pstart = *ptr++;
    plen   = *ptr++;

    if (pstart < 0 || plen < 0 || (pstart + plen) > strlen(p)) {
      apush(makefault("?ranges"));
      freeup(z);
      return;
    }

  }

  /* Split the string */
  next = 0;
  nslices = ndims + 1;
  res = new_create_array(atype, 1, 0, &nslices);
  apush(res);

  for (i = 0; i < ndims; i++) {
    nialptr adim = fetch_array(re_dims, i);
    nialint *ptr, pstart, plen, slen;

    ptr = pfirstint(adim);
    pstart = *ptr++;
    plen   = *ptr++;

    slen = pstart - next;
    if (slen == 0) {
      store_array(res, i, Null);
    } else {
      nialptr slice = new_create_array(chartype, 1, 0, &slen);
      strncpy(pfirstchar(slice), p+next, slen);
      store_array(res, i, slice);
    }

    next += (slen + plen);
  }    

  /* add the final piece */
  if (next == strlen(p)) {
    store_array(res, nslices-1, Null);
  } else {
    nialint slen = strlen(p) - next;
    nialptr slice = new_create_array(chartype, 1, 0, &slen);
    strncpy(pfirstchar(slice), p+next, slen);
    store_array(res, nslices-1, slice);
  }    
    
  freeup(z);
  return;
}


/**
 * The following code implements a simple nial primitive to splice
 * a value in between the elements of a list 
 *
 *    re_splice <value> <list>
 */
void
iregexp_splice(void)
{
  nialptr     z = apop();
  nialptr     sval;
  nialptr     slist;
  nialptr     res;
  nialint     reslen;
  nialint     i, j;

  /* validate the arguments */
  if (tally(z) != 2 || kind(z) != atype) {
    apush(makefault("?args"));
    freeup(z);
    return;
  }

  
  /* Split the arguments */
  splitfb(z, &sval, &slist);

  if (kind(slist) != atype) {
    apush(makefault("?argtypes"));
    freeup(z);
    return;
  }

  if (slist == Null) {
    apush(Null);
    freeup(z);
    return;
  } 

  reslen = 2*tally(slist) - 1;
  res = new_create_array(atype, 1, 0, &reslen);
  apush(res);
  for (i = 0, j = 0; i < tally(slist); i++) {
    store_array(res, j++, fetch_array(slist, i));
    if (j < reslen)
      store_array(res, j++, sval);
  }

  freeup(z);
  return;
}


#endif /* ) REGEXP */

