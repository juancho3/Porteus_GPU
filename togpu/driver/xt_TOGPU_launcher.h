/* GPU packet processor. Allows GPU processing over RX traffic */

/* (C) 2012 Juan Antonio Andres <juanantonio.andres@uam.es>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef TOGPU_LAUNCHER_HEADER
#define TOGPU_LAUNCHER_HEADER

#ifdef __cplusplus
extern "C" {
#endif

   void *gpu_alloc_pinned_mem( unsigned long size );
   void  gpu_free_pinned_mem( void *p );

#ifdef __cplusplus
}
#endif

#endif
