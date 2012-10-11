/* GPU packet processor. Allows GPU processing over RX traffic */

/* (C) 2012 Juan Antonio Andres <juanantonio.andres@uam.es>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef TOGPU_HEADER
#define TOGPU_HEADER

#include <asm/ioctls.h>

#define NFGPU_CPU_CLOCK_PROFILING
//#define NFGPU_PRETTY_DEBUG
#define NFGPU_DEV_NAME                       "togpu"

#define NFGPU_MAX_MEMORY                    (1L<<25)  // =32MB
#define NFGPU_MAX_PAGES                     (NFGPU_MAX_MEMORY/PAGE_SIZE)
#define NFGPU_SKB_OFFSET                     12
#define NFGPU_SKB_SIZE                      (1L<<NFGPU_SKB_OFFSET)  // =4KB
#define NFGPU_MAX_SKBS                      (NFGPU_MAX_MEMORY/NFGPU_SKB_SIZE)
#define NFGPU_MEMORY_MASK                   ~(NFGPU_MAX_MEMORY-1L)

// Log levels
#define NFGPU_LOG_ALL                        0
#define NFGPU_LOG_INFO                       1
#define NFGPU_LOG_DEBUG                      2
#define NFGPU_LOG_ALERT                      3
#define NFGPU_LOG_ERROR                      4
#define NFGPU_LOG_PRINT                      5

#define NFGPU_IOCTL_MAGIC                    't'

#define TOGPU_IOCTL_SET_BUFFERS              _IOW(NFGPU_IOCTL_MAGIC, 1, struct nfgpu_mem_info)
#define TOGPU_IOCTL_GET_BUFFERS              _IOR(NFGPU_IOCTL_MAGIC, 2, struct nfgpu_mem_info)
#define TOGPU_IOCTL_START                    _IO(NFGPU_IOCTL_MAGIC, 3)
#define TOGPU_IOCTL_STOP                     _IO(NFGPU_IOCTL_MAGIC, 4)

#define PKPRE                                "[" NFGPU_DEV_NAME "] "
#define OP_JMP_SIZE                          5
#define IN_ERR(x)                            (x<0)

#define nfgpu_do_log(level, module, ...)     nfgpu_generic_log( level, module, __FILE__, __LINE__, __func__, ##__VA_ARGS__ )
#define nfgpu_log(level, ...)                nfgpu_do_log( level, NFGPU_DEV_NAME, ##__VA_ARGS__ )
#define dbg(...)                             nfgpu_log( NFGPU_LOG_DEBUG, ##__VA_ARGS__ )

struct xt_togpu_info
{
   unsigned int    to;
};

struct nfgpu_mem_info
{
   void           *uva;
   unsigned long   size;
};

struct kernsym
{
	void           *addr;
	void           *end_addr;
	unsigned long   size;
	char           *name;
	bool            name_alloc;
	unsigned char   orig_start_bytes[OP_JMP_SIZE];
	void           *new_addr;
	unsigned long   new_size;
	bool            found;
	bool            hijacked;
	void           *run;
};

int   hook_init( void );
int   symbol_hook( struct kernsym *, const char *, unsigned long * );
void  symbol_restore( struct kernsym * );
void  symbol_info( struct kernsym * );
int   find_symbol_address( struct kernsym *, const char * );

#endif
