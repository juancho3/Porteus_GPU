/* GPU packet processor. Allows GPU processing over RX traffic */

/* (C) 2012 Juan Antonio Andres <juanantonio.andres@uam.es>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "xt_TOGPU.h"
#include "xt_TOGPU_launcher.h"

// --------------------------- BEGIN LOG subsystem ---------------------------------

#ifdef __NFGPU_LOG_LEVEL__
int nfgpu_log_level = __NFGPU_LOG_LEVEL__;
#else
//int nfgpu_log_level = NFGPU_LOG_ALERT;
int nfgpu_log_level = NFGPU_LOG_ALL;
#endif

static void nfgpu_generic_log( int level, const char *module, const char *filename,
                               int lineno, const char *func, const char *fmt, ... )
{
   va_list args;

   if( level < nfgpu_log_level )
      return;

#ifdef NFGPU_PRETTY_DEBUG
   switch( level )
   {
      case NFGPU_LOG_INFO:
         printf( "[%s] %s::%d %s() INFO: ", module, filename, lineno, func );
         break;
      case NFGPU_LOG_DEBUG:
         printf( "[%s] %s::%d %s() DEBUG: ", module, filename, lineno, func );
         break;
      case NFGPU_LOG_ALERT:
         printf( "[%s] %s::%d %s() ALERT: ", module, filename, lineno, func );
         break;
      case NFGPU_LOG_ERROR:
         printf( "[%s] %s::%d %s() ERROR: ", module, filename, lineno, func );
         break;
      case NFGPU_LOG_PRINT:
         printf( "[%s] %s::%d %s(): ", module, filename, lineno, func );
         break;
      default:
         break;
   } //endswitch
#else
   printf( "[%s] ", module );
#endif

   va_start( args, fmt );
   vprintf( fmt, args );
   va_end( args );
}


// ------------------------------ MAIN ------------------------------------

int main( int argc, char *argv[] )
{
   struct nfgpu_mem_info   mem_info;
   char                   *nfgpudev = "/dev/" NFGPU_DEV_NAME;
   void                   *ptr;
   int                     fd, err;

   // Welcome
   nfgpu_log( NFGPU_LOG_PRINT, "Welcome to TOGPU NEtfilter'r target launcher.\n" );
   nfgpu_log( NFGPU_LOG_PRINT, "TOGPU launcher will load CUDA kernel into GPU to allow processing network traffic.\n" );

   // Open "/dev/nfgpu" (gain access to driver)
   nfgpu_log( NFGPU_LOG_PRINT, "Opening %s device ...\n", nfgpudev );
   fd = open( nfgpudev, O_RDWR );
   if( fd <= 0 )
   {
      nfgpu_log( NFGPU_LOG_ERROR, "Error: xt_TOGPU driver must be loaded into kernel!\n" );
      return -1;
   } //endif

   // Stop any previous CUDA network processing
   ioctl( fd, TOGPU_IOCTL_STOP, NULL );

   // Alloc pinned memory at GPU's internal memory
   nfgpu_log( NFGPU_LOG_PRINT, "Allocating %d bytes from internal GPU memory ...\n", NFGPU_MAX_MEMORY );
   ptr = (void *) gpu_alloc_pinned_mem( NFGPU_MAX_MEMORY );  // One extra page
   if( ptr == NULL )
   {
      nfgpu_log( NFGPU_LOG_ERROR, "Error: Sorry, your GPU does not support pinned memory or has not enough free memory!\n" );
      return -1;
   } //endif
   mem_info.uva  = ptr;
   mem_info.size = NFGPU_MAX_MEMORY;

   // Call driver to map GPU memory into kernel space
   nfgpu_log( NFGPU_LOG_PRINT, "Calling ioctl to map GPU internal memory into netfilter's target ...\n" );
   err = ioctl( fd, TOGPU_IOCTL_SET_BUFFERS, (unsigned long) &mem_info );
   if( err < 0 )
   {
      nfgpu_log( NFGPU_LOG_ERROR, "Error: GPU memory could NOT be mapped into kernel space!\n" );
      return -1;
   } //endif

   // Invoke GPU code to process network packets
   // Packets will be passed to GPU kernel directly !!!
   nfgpu_log( NFGPU_LOG_PRINT, "Starting CUDA kernel. Processing network traffic ...\n" );
   nfgpu_log( NFGPU_LOG_PRINT, "(don't forget to configure 'iptables' to inject traffic into GPU)\n" );

   // Launch CUDA network processing
   ioctl( fd, TOGPU_IOCTL_START, NULL );
   if( err < 0 )
   {
      nfgpu_log( NFGPU_LOG_ERROR, "Error: GPU CUDA program could not be started!\n" );
      return -1;
   } //endif

   // Use your CUDA API here
   aes_init();
   aes_test();

   // Stop CUDA network processing
   ioctl( fd, TOGPU_IOCTL_STOP, NULL );

   // Free GPU memory
   gpu_free_pinned_mem( ptr );

   return 0;
}
