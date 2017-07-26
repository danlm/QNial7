/*==============================================================

  MODULE   NSFML.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  Interface to NSFML Audio Routines

================================================================*/


/* Q'Nial file that selects features */

#include "switches.h"

#ifdef NSFML_AUDIO

/* standard library header files */

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

/* STLIB */
#include <string.h>

/* SIGLIB */
#include <signal.h>

/* SJLIB */
#include <setjmp.h>

/* PROCESSLIB */
#include <sys/types.h>
#ifdef UNIXSYS
#include <pwd.h>
#endif
#include <fcntl.h>
#ifdef UNIXSYS
#include <netdb.h>
#endif
#include <errno.h>

/* Q'Nial header files */

#include "absmach.h"
#include "qniallim.h"
#include "lib_main.h"
#include "ops.h"
#include "trs.h"
#include "fileio.h"

#ifdef UNIXSYS
#include "unixif.h"
#endif
#ifdef WINNIAL
#include "windowsif.h"
#endif

#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <string.h>

#include <SFML/Audio.h>


/**
 * Generic C reference model to map local C pointers to
 * integers for Nial
 */

typedef struct {
  int ref_type;
  void *ref_ptr;
} CRefType, *CRefPtr; 

#define MAX_CREF   1024
static int num_cref = 0;
static CRefPtr crefs[MAX_CREF];

/**
 * Define the reference types we will handle
 */

#define FREE_CREF_SLOT         0
#define NSFML_SOUND_REF        1
#define NSFML_MUSIC_REF        2
#define NSFML_SOUNDBUFFER_REF  3


/* Macro to validate a reference index */
#define VALID_CREF_INDEX(i) (0 <= (i) && (i) < MAX_CREF && crefs[(i)] != NULL)
#define VALID_CREF(i, rtype) (0 <= (i) && (i) < MAX_CREF && crefs[(i)] != NULL && crefs[(i)]->ref_type == rtype)


/**
 * Allocate a reference entry and return the index.
 * Return -1 if no free references.
 */
static int alloc_cref(int rtype) {
  static int init = 0;
  int i;

  if (init == 0) {
    int j;
    for (j = 0; j < MAX_CREF; j++) {
      CRefPtr p = (CRefPtr)malloc(sizeof(CRefType));
      crefs[j] = p;
      p->ref_type = FREE_CREF_SLOT;
      p->ref_ptr  = NULL;
    }
    
    init = 1;
  }
  
  for (i = 0; i < num_cref; i++) {
    if (crefs[i]->ref_type == FREE_CREF_SLOT) {
      crefs[i]->ref_type = rtype;
      crefs[i]->ref_ptr = NULL;
      return i;
    }
  }

  if (i < MAX_CREF) {
    num_cref++;
    crefs[i]->ref_type = rtype;
    crefs[i]->ref_ptr = NULL;
    return i;
  }

  return -1;
}
  

static void free_cref(int i) {
  if (crefs[i]->ref_ptr != NULL) {
    switch (crefs[i]->ref_type) {
      case NSFML_MUSIC_REF:
        sfMusic_destroy((sfMusic*)crefs[i]->ref_ptr);
        break;
      case NSFML_SOUND_REF:
        sfSound_destroy((sfSound*)crefs[i]->ref_ptr);
        break;
      case NSFML_SOUNDBUFFER_REF:
        sfSoundBuffer_destroy((sfSoundBuffer*)crefs[i]->ref_ptr);
        break;
      default:
        /* Nothing at moment */
        break;
    }
  }
  
  crefs[i]->ref_type = FREE_CREF_SLOT;
  crefs[i]->ref_ptr  = NULL;
  
  return;
}

/* -------------------- Low Level Code -------------------------- */



void insfml_music_from_file(void) {
  nialptr x = apop();
  nialint i = -1;
  sfMusic *music = NULL;
  
  /* Check argument */
  if (kind(x) != chartype && kind(x) != phrasetype) {
    apush(makefault("?args"));
    goto error_exit;
  }
  
  /* Find a reference slot */
  i = alloc_cref(NSFML_MUSIC_REF);
  if (i == -1) {
    apush(makefault("?nomem"));
    goto error_exit;
  }
  
  /* Load music from file */
  music = sfMusic_createFromFile(pfirstchar(x));
  if (!music) {
    apush(makefault("?failed"));
    goto error_exit;
  }
  
  /* All good, return the index */
  crefs[i]->ref_ptr = music;
  apush(createint(i));
  freeup(x);
  return;
  
  error_exit:
    if (i != -1)
      free_cref(i);
    freeup(x);
    return;
  
}


void insfml_play(void) {
  nialptr x = apop();
  nialint i = -1;
  sfMusic *music = NULL;
  
  /* Check argument */
  if (kind(x) != inttype || !VALID_CREF_INDEX(intval(x))) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  /* Play appropriately */
  i = intval(x);
  switch(crefs[i]->ref_type) {
    case NSFML_MUSIC_REF:
      sfMusic_play((sfMusic*)crefs[i]->ref_ptr);
      break;
    case NSFML_SOUND_REF:
      sfSound_setRelativeToListener((sfSound*)crefs[i]->ref_ptr, sfFalse);
      sfSound_play((sfSound*)crefs[i]->ref_ptr);
      break;
    default:
      apush(makefault("?invalid_play"));
      freeup(x);
      return;
  }
  
  apush(True_val);
  freeup(x);
  return;
}


void insfml_pause(void) {
  nialptr x = apop();
  nialint i = -1;
  sfMusic *music = NULL;
  
  /* Check argument */
  if (kind(x) != inttype || !VALID_CREF_INDEX(intval(x))) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  /* Play appropriately */
  i = intval(x);
  switch(crefs[i]->ref_type) {
    case NSFML_MUSIC_REF:
      sfMusic_pause((sfMusic*)crefs[i]->ref_ptr);
      break;
    case NSFML_SOUND_REF:
      sfSound_pause((sfSound*)crefs[i]->ref_ptr);
      break;
    default:
      apush(makefault("?invalid_pause"));
      freeup(x);
      return;
  }
  
  apush(True_val);
  freeup(x);
  return;
}


void insfml_stop(void) {
  nialptr x = apop();
  nialint i = -1;
  sfMusic *music = NULL;
  
  /* Check argument */
  if (kind(x) != inttype || !VALID_CREF_INDEX(intval(x))) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  /* Play appropriately */
  i = intval(x);
  switch(crefs[i]->ref_type) {
    case NSFML_MUSIC_REF:
      sfMusic_stop((sfMusic*)crefs[i]->ref_ptr);
      break;
    case NSFML_SOUND_REF:
      sfSound_stop((sfSound*)crefs[i]->ref_ptr);
      break;
    default:
      apush(makefault("?invalid_stop"));
      freeup(x);
      return;
  }
  
  apush(True_val);
  freeup(x);
  return;
}


void insfml_create_sound(void) {
  nialptr x = apop();
  sfSound *sound = NULL;
  nialint i = -1;
  
  if (kind(x) != inttype) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  i = alloc_cref(NSFML_SOUND_REF);
  if (i == -1) {
    apush(makefault("?nomem"));
    freeup(x);
    return;
  }
  
  sound = sfSound_create();
  if (!sound) {
    free_cref(i);
    apush(makefault("?sound_error"));
    freeup(x);
    return;
  }
  
  crefs[i]->ref_ptr = sound;
  apush(createint(i));
  freeup(x);
  return;
}


void insfml_soundbuffer_from_file(void) {
  nialptr x = apop();
  nialint i = -1;
  sfSoundBuffer *soundb = NULL;
  
  /* Check argument */
  if (kind(x) != chartype && kind(x) != phrasetype) {
    apush(makefault("?args"));
    goto error_exit;
  }
  
  /* Find a reference slot */
  i = alloc_cref(NSFML_SOUNDBUFFER_REF);
  if (i == -1) {
    apush(makefault("?nomem"));
    goto error_exit;
  }
  
  /* Load sound buffer from file */
  soundb = sfSoundBuffer_createFromFile(pfirstchar(x));
  if (!soundb) {
    apush(makefault("?failed"));
    goto error_exit;
  }
  
  /* All good, return the index */
  crefs[i]->ref_ptr = soundb;
  apush(createint(i));
  freeup(x);
  return;
  
  error_exit:
    if (i != -1)
      free_cref(i);
    freeup(x);
    return;
  
}


void insfml_soundbuffer_to_file(void) {
  nialptr x = apop();
  nialptr soundbptr, fname;
  sfSoundBuffer *soundb = NULL;
  
  /* Check argument */
  if (kind(x) != atype && tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  /* separate arguments and validate */
  splitfb(x, &soundbptr, &fname);
  if (kind(soundbptr) != inttype || !istext(fname) || tally(fname) == 0 || !VALID_CREF(intval(soundbptr), NSFML_SOUNDBUFFER_REF)) {
    apush(makefault("?arg_types"));
    freeup(x);
    return;
  }
  
  soundb = (sfSoundBuffer*)crefs[intval(soundbptr)]->ref_ptr;
  if (!sfSoundBuffer_saveToFile(soundb, pfirstchar(fname))) {
    apush(makefault("?save_to_file_failed"));
    freeup(x);
    return;
  }
  
  apush(True_val);
  freeup(x);
  return;  
}


void insfml_soundbuffer_from_memory(void) {
  nialptr x = apop();
  nialint i = -1;
  sfSoundBuffer *soundb = NULL;
  
  /* Check argument */
  if (kind(x) != chartype) {
    apush(makefault("?args"));
    goto error_exit;
  }
  
  /* Find a reference slot */
  i = alloc_cref(NSFML_SOUNDBUFFER_REF);
  if (i == -1) {
    apush(makefault("?nomem"));
    goto error_exit;
  }
  
  /* Load sound buffer from memory */
  soundb = sfSoundBuffer_createFromMemory(pfirstchar(x), tally(x));
  if (!soundb) {
    apush(makefault("?failed"));
    goto error_exit;
  }
  
  /* All good, return the index */
  crefs[i]->ref_ptr = soundb;
  apush(createint(i));
  freeup(x);
  return;
  
  error_exit:
    if (i != -1)
      free_cref(i);
    freeup(x);
    return;
  
}


void insfml_soundbuffer_from_samples(void) {
  nialptr x = apop();
  nialint i = -1;
  sfSoundBuffer *soundb = NULL;
  nialptr samples, samplecount, channelcount, samplerate;
  
  /* Check argument */
  if (kind(x) != atype || tally(x) != 4) {
    apush(makefault("?args"));
    goto error_exit;
  }
  
  samples      = fetch_array(x, 0);
  samplecount  = fetch_array(x, 1);
  channelcount = fetch_array(x, 2);
  samplerate   = fetch_array(x, 3);
  
  if (kind(samples) != chartype || kind(samplecount) != inttype || kind(channelcount) != inttype || kind(samplerate) != inttype) {
    apush(makefault("?arg_types"));
    freeup(x);
    return;
  }
  
  
  /* Find a reference slot */
  i = alloc_cref(NSFML_SOUNDBUFFER_REF);
  if (i == -1) {
    apush(makefault("?nomem"));
    goto error_exit;
  }
  
  /* Load sound buffer from memory */
  soundb = sfSoundBuffer_createFromSamples((sfInt16*)pfirstchar(samples),
                                           (size_t)intval(samplecount),
                                           (unsigned int)intval(channelcount),
                                           (unsigned int)intval(samplerate));
  if (!soundb) {
    apush(makefault("?failed"));
    goto error_exit;
  }
  
  /* All good, return the index */
  crefs[i]->ref_ptr = soundb;
  apush(createint(i));
  freeup(x);
  return;
  
  error_exit:
    if (i != -1)
      free_cref(i);
    freeup(x);
    return;
  
}


void insfml_sound_set_buffer(void) {
  nialptr x = apop();
  nialint isound, ibuffer;
  
  if (kind(x) != inttype || tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  isound  = fetch_int(x, 0);
  ibuffer = fetch_int(x, 1);
  
  if (!VALID_CREF(isound, NSFML_SOUND_REF) || !VALID_CREF(ibuffer, NSFML_SOUNDBUFFER_REF)) {
    apush(makefault("?arg_types"));
    freeup(x);
    return;
  }
  
  sfSound_setBuffer((sfSound*)crefs[isound]->ref_ptr, (sfSoundBuffer*)crefs[ibuffer]->ref_ptr);
  apush(True_val);
  freeup(x);
  return;
}


void insfml_destroy(void) {
  nialptr x = apop();
  
  if (kind(x) != inttype || !VALID_CREF_INDEX(intval(x))) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  free_cref(intval(x));
  apush(True_val);
  freeup(x);
  return;
}


void insfml_get_channel_count(void) {
  nialptr x = apop();
  sfSoundBuffer *buffer;
  
  if (kind(x) != inttype || !VALID_CREF(intval(x), NSFML_SOUNDBUFFER_REF)) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  buffer = (sfSoundBuffer*)crefs[intval(x)]->ref_ptr;
  apush(createint(sfSoundBuffer_getChannelCount(buffer)));
  freeup(x);
  return;
}


void insfml_get_sample_count(void) {
  nialptr x = apop();
  sfSoundBuffer *buffer;
  
  if (kind(x) != inttype || !VALID_CREF(intval(x), NSFML_SOUNDBUFFER_REF)) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  buffer = (sfSoundBuffer*)crefs[intval(x)]->ref_ptr;
  apush(createint(sfSoundBuffer_getSampleCount(buffer)));
  freeup(x);
  return;
}


void insfml_get_sample_rate(void) {
  nialptr x = apop();
  sfSoundBuffer *buffer;
  
  if (kind(x) != inttype || !VALID_CREF(intval(x), NSFML_SOUNDBUFFER_REF)) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  buffer = (sfSoundBuffer*)crefs[intval(x)]->ref_ptr;
  apush(createint(sfSoundBuffer_getSampleRate(buffer)));
  freeup(x);
  return;
}


void insfml_get_samples(void) {
  nialptr x = apop();
  size_t sampleCount;
  nialint reslen;
  nialptr res;
  sfSoundBuffer *buffer;
  
  if (kind(x) != inttype || !VALID_CREF(intval(x), NSFML_SOUNDBUFFER_REF)) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  buffer = (sfSoundBuffer*)crefs[intval(x)]->ref_ptr;
  sampleCount = sfSoundBuffer_getSampleCount(buffer);
  reslen = (nialint)sampleCount*2;
  res = new_create_array(chartype, 1, 0, &reslen);
  memcpy(pfirstchar(res), sfSoundBuffer_getSamples(buffer), 2*sampleCount);
  
  apush(res);
  freeup(x);
  return;
}


#endif /* NSFML_AUDIO */

