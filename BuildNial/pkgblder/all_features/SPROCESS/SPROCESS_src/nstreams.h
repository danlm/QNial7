
/* ---------------- Buffers ------------------ */


/*
 * Internal buffering mechanism for streams
 */
#define SP_BUFFSIZE  4096
typedef struct SP_Buff {
  struct  SP_Buff   *next;
  int        count;
  int        d_start;    /* Index of first character in buffer */
  int        d_end;      /* Last+1 */
  unsigned   char buff[SP_BUFFSIZE];
} SP_Buffer, *SP_BuffPtr;



/* --------------- Streams ------------------ */


/* Modes of buffering */
#define IOS_INTERNAL          0
#define IOS_PROTOCOL          1
#define IOS_BUFFERED          2
#define IOS_LINE              3


/* Stream status values */
#define IOS_NORMAL            0
#define IOS_EOF              -1


/* Stream flags */
#define IOS_NOFLAGS          0x000
#define IOS_CHARSTREAM       0x001

/* Read whatever characters are available  */
#define IOS_NOLIMIT          -1

/* Some common  wait values */
#define IOS_NO_WAIT          0
#define IOS_INDEFINITE_WAIT -1


/**
 * Stream structure
 */
typedef struct {
  int          index;
  int          fd;
  int          mode;
  int          flags;
  int          status;
  nialint      count;
  SP_BuffPtr   first;
  SP_BuffPtr   last;
} SP_Stream, *SP_StreamPtr;



/* --------------- Nial Support ----------------- */

extern int  nio_createStream();

extern void nio_freeStream(int ios);

extern int  nio_set_fd(int ios, int fd);
extern int  nio_set_mode(int ios, int mode);
extern int  nio_set_flags(int ios, int mode);

extern void nio_set_nonblock(int fd, int flag);


/* -------------- Primitives ------------------- */

extern void inio_openStream(void);
extern void inio_closeStream(void);
extern void inio_write(void);
extern void inio_writeln(void);
extern void inio_count(void);
extern void inio_write_stream(void);
extern void inio_read(void);
extern void inio_readln(void);
extern void inio_status(void);

extern void inio_block_array(void);
extern void inio_unblock_array(void);

extern void inio_add_count(void);
extern void inio_get_count(void);

extern void inio_newpipe(void);
extern void inio_socketpair(void);

