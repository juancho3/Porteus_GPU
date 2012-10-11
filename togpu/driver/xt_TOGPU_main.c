/* GPU packet processor. Allows GPU processing over RX traffic */

/* (C) 2012 Juan Antonio Andres <juanantonio.andres@uam.es>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/ip.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/netfilter.h>
#include <linux/netdevice.h>
#include <linux/if.h>
#include <linux/inetdevice.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/hugetlb.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <asm/page.h>
#include <asm/cache.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/highmem.h>
#include <asm/current.h>
#include <net/protocol.h>
#include <net/checksum.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter/x_tables.h>

#include "xt_TOGPU.h"

#ifdef NFGPU_CPU_CLOCK_PROFILING
#include <asm/msr.h>
#endif

MODULE_LICENSE("GPL");  // Note: With any other license, module will not link !!!
MODULE_AUTHOR("Juan Antonio Andres Saaez <juanantonio.andres@uam.es>");
MODULE_DESCRIPTION("Xtables: Netfilter's GPU target (GPU packet processor)");

extern int walk_page( unsigned long addr, unsigned long end, struct mm_walk *walk );

// ------------------------------ Internal DATA ------------------------------------

static struct task_struct   *nfgpu_thread_task;
static spinlock_t            nfgpu_slock;
static struct kernsym        nfgpu_sym_netdev_alloc;
static struct kernsym        nfgpu_sym_skb_release_data;

static struct cdev           nfgpu_cdev;
static struct class         *nfgpu_cls;
static dev_t                 nfgpu_devno;
static atomic_t              nfgpu_av = ATOMIC_INIT(1);

static bool                  nfgpu_running;
static struct page          *nfgpu_pages      [ NFGPU_MAX_PAGES ];
static unsigned long         nfgpu_uva;
static unsigned long         nfgpu_kva;
static unsigned long         nfgpu_size;
static unsigned long         nfgpu_npages;
static unsigned long         nfgpu_skbs[ NFGPU_MAX_SKBS ];
static unsigned long         nfgpu_next;

#ifdef ALLOC_VMAP_PAGES
static unsigned long         nfgpu_tpages;
static unsigned long         nfgpu_pages_base [ NFGPU_MAX_PAGES ];
static unsigned long         nfgpu_pages_len  [ NFGPU_MAX_PAGES ];
static unsigned long         nfgpu_pages_num  [ NFGPU_MAX_PAGES ];
static struct page         **nfgpu_pages_vmap [ NFGPU_MAX_PAGES ];
#endif

// Profiling support
static unsigned long         nfgpu_packets;
static unsigned long         nfgpu_average;

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
         printk( "[%s] %s::%d %s() INFO: ", module, filename, lineno, func );
         break;
      case NFGPU_LOG_DEBUG:
         printk( "[%s] %s::%d %s() DEBUG: ", module, filename, lineno, func );
         break;
      case NFGPU_LOG_ALERT:
         printk( "[%s] %s::%d %s() ALERT: ", module, filename, lineno, func );
         break;
      case NFGPU_LOG_ERROR:
         printk( "[%s] %s::%d %s() ERROR: ", module, filename, lineno, func );
         break;
      case NFGPU_LOG_PRINT:
         printk( "[%s] %s::%d %s(): ", module, filename, lineno, func );
         break;
      default:
         break;
   } //endswitch
#else
   printk( "[%s] ", module );
#endif

   va_start( args, fmt );
   vprintk( fmt, args );
   va_end( args );
}

// --------------------------- Profiling support ---------------------------------

static int nfgpu_thread( void  *data )
{
   unsigned long           j0, j1, avg, pkts;
   int                     delay = 5*HZ;  // Seconds between messages

   while( true )
   {
      j0 = jiffies;
      j1 = j0 + delay;
      while( time_before(jiffies, j1) )
      {
         if( kthread_should_stop() )  return 0;
         schedule();
      } //wend

#ifdef NFGPU_CPU_CLOCK_PROFILING

      // Show profiling
      //spin_lock( &nfgpu_slock );
      avg = nfgpu_average;
      pkts = nfgpu_packets;
      mb();
      nfgpu_average = 0;
      nfgpu_packets = 0;
      //spin_unlock( &nfgpu_slock );

      nfgpu_log( NFGPU_LOG_PRINT, "Profiling: CPU clock cycles average [%ld] packets [%ld]\n", avg, pkts );

#endif
   } //wend

   return 0;
}

// --------------------------- Netfilter's target ---------------------------------

static unsigned int nfgpu_target(       struct sk_buff         *skb,
                                  const struct xt_action_param *par
                                )
{
   //const struct xt_togpu_info  *info = (struct xt_togpu_info *) par->targinfo;  //RFU (GPU destination?)
   unsigned long   timestamp_start, timestamp_end, index;
   volatile unsigned long  *target;

   //nfgpu_log( NFGPU_LOG_PRINT, "nfgpu_target(): Received packet [%d bytes]\n", skb->len );

   // Process only PRE-ROUTING traffic with this target
   //if( par->hooknum != NF_INET_PRE_ROUTING )  return NF_ACCEPT;

   // CUDA kernel must be running in order to process network traffic
   if( nfgpu_running == false )  return NF_ACCEPT;
   index = (((unsigned long) skb->head) >> NFGPU_SKB_OFFSET) - (nfgpu_kva >> NFGPU_SKB_OFFSET);
   //nfgpu_log( NFGPU_LOG_PRINT, "nfgpu_target(): Received SKB [%p == %p] Idx [%d]\n", skb->head, mapped, index );

   // Check if SKB data address corresponds with GPU memory mapped KVA
   if( (((unsigned long) skb->head) & NFGPU_MEMORY_MASK) != (nfgpu_kva & NFGPU_MEMORY_MASK) )  return NF_ACCEPT;
   if( index >= NFGPU_MAX_SKBS )  return NF_ACCEPT;

   // Profiling support
#ifdef NFGPU_CPU_CLOCK_PROFILING
   rdtscl( timestamp_start );  // Remember: CPU clock cycles  (only in x86_64)
#endif

   // Synchronize with GPU kernel with direct memory accesses
   target      = (unsigned long *) nfgpu_kva;
   *target     = index;
   *(target+1) = (unsigned long) skb->len;
   while( *target != 0 );  // Wait by polling till packet is processed

   // Profiling support
#ifdef NFGPU_CPU_CLOCK_PROFILING
   rdtscl( timestamp_end );  // Remember: CPU clock cycles
   if( timestamp_end > timestamp_start )
   {
      //spin_lock( &nfgpu_slock );  // Don't worry, but do NOT stop please!
      nfgpu_average = ((nfgpu_average << 8) + (timestamp_end-timestamp_start)) / ((1<<8)+1);
      nfgpu_packets++;
      //spin_unlock( &nfgpu_slock );
   } //endif
#endif

   // Continue chain packet processing
   return NF_ACCEPT;
}

static struct xt_target  nfgpu_target_reg  __read_mostly =
{
   .name           = "TOGPU",
   .family         = AF_INET,
   .table          = "mangle",
   .target         = nfgpu_target,
   .targetsize     = sizeof(struct xt_togpu_info),
   .hooks          = 1 << NF_INET_PRE_ROUTING,
   .me             = THIS_MODULE,
};

struct sk_buff *__netdev_alloc_skb_togpu( struct net_device *dev, unsigned int length, gfp_t gfp_mask )
{
   struct sk_buff  *(*run)(struct net_device *, unsigned int, gfp_t) = nfgpu_sym_netdev_alloc.run;
   struct sk_buff  *ret;
   unsigned long    skblength, headtodata;
   unsigned long    newhead=0, oldhead=0;

   // STEP 1: Call native skb allocator
   ret = run( dev, length, gfp_mask );

   // STEP 2: Switch data buffer with a reallocated one
   //         (only if no fragments present and not a jumbo packet size)
   if( nfgpu_running != false )
   {
      skblength  = (unsigned long) ret->end  - (unsigned long) ret->head;
      headtodata = (unsigned long) ret->data - (unsigned long) ret->head;
      if( (skb_shinfo(ret)->nr_frags == 0) && (skblength <= NFGPU_SKB_SIZE) )
      {
         //nfgpu_log( NFGPU_LOG_PRINT, "__netdev_alloc_skb PRE:  head [%p] data [%p] tail [%p] end [%p]\n", ret->head, ret->data, ret->tail, ret->end );  // Internal debug

         // Find a free slot and assign it
         while( nfgpu_skbs[nfgpu_next] != 0 )
         {
            // Wait forever till memory is available :-(
            if( ++nfgpu_next >= NFGPU_MAX_SKBS )  nfgpu_next = 1;  // 0 is reserved for sync with GPU
         } //wend

         // Assign new address to SKB data
         oldhead = (unsigned long) ret->head;
         nfgpu_skbs[ nfgpu_next ] = (unsigned long) ret->head;
         newhead = nfgpu_kva + (nfgpu_next << NFGPU_SKB_OFFSET);
         ret->head = (void *)  newhead;
         ret->data = (void *) (newhead + headtodata);
         skb_reset_tail_pointer( ret );
         ret->end  =          (newhead + skblength);

         //nfgpu_log( NFGPU_LOG_PRINT, "__netdev_alloc_skb (len=%d) old [%p] new [%p]\n", length, oldhead, newhead );  // Internal debug
         //nfgpu_log( NFGPU_LOG_PRINT, "__netdev_alloc_skb POST: head [%p] data [%p] tail [%p] end [%p]\n", ret->head, ret->data, ret->tail, ret->end );  // Internal debug
      } //endif
   } //endif

   return ret;
}

void skb_release_data_togpu( struct sk_buff *skb )
{
   void (*run)(struct sk_buff *) = nfgpu_sym_skb_release_data.run;
   unsigned long  newhead=0, oldhead=0, index, ddata, dtail, dend;

   // STEP 1: Check if skb corresponds with a reallocated one
   oldhead = (unsigned long) skb->head;
   if( nfgpu_kva != 0 )
   {
      if( (oldhead & NFGPU_MEMORY_MASK) == (nfgpu_kva & NFGPU_MEMORY_MASK) )
      {
         newhead = oldhead;
         ddata = (unsigned long) skb->data - oldhead;
         dtail = (unsigned long) skb->tail - oldhead;
         dend  = (unsigned long) skb->end  - oldhead;
         index = (oldhead >> NFGPU_SKB_OFFSET) - (nfgpu_kva >> NFGPU_SKB_OFFSET);
         if( index < NFGPU_MAX_SKBS )
         {
            newhead = nfgpu_skbs[ index ];
            if( (index < NFGPU_MAX_SKBS) && (newhead != 0) )
            {
               skb->head = (void *)  newhead;
               skb->data = (void *) (newhead + ddata);
               skb->tail =          (newhead + dtail);
               skb->end  =          (newhead + dend);
               nfgpu_skbs[ index ] = 0;  // Free resource for future hooked allocs
            } //endif
         } //endif

         //nfgpu_log( NFGPU_LOG_PRINT, "skb_release_data() old [%p] new [%p]\n", oldhead, newhead );  // Internal debug
      } //endif
   } //endif

   // STEP 2: Continue releasing SKB with original data pointer
   run( skb );
}

// --------------------------- Device operations ---------------------------------

#ifdef ALLOC_VMAP_PAGES
static struct page  **nfgpu_alloc_pages( void *addr, size_t len )
{
   int           nr_pages  = DIV_ROUND_UP( offset_in_page(addr) + len, PAGE_SIZE );
   struct page **pages     = (struct page **) kmalloc( nr_pages * sizeof(*pages), GFP_KERNEL );
   void         *page_addr = (void *) ( (unsigned long) addr & PAGE_MASK );
   int           i;

   if( pages == NULL )  return NULL;

   // Alloc pages prior to vmap()
   for( i=0; i<nr_pages; i++ )
   {
      pages[i] = alloc_page( GFP_KERNEL );
      if( pages[i] == NULL )
      {
         kfree( pages );
         return NULL;
      } //endif
      page_addr += PAGE_SIZE;
   } //endfor

   return  pages;
}
#endif

static int nfgpu_skb_pte_process( pte_t *pte, unsigned long addr, unsigned long end, struct mm_walk *walk )
{
#ifdef ALLOC_VMAP_PAGES
   unsigned long  maddr, mlen;
#endif

   if( !is_swap_pte(*pte) && pte_present(*pte) )
   {
       nfgpu_pages[ nfgpu_npages ] = pte_page(*pte);
       nfgpu_log( NFGPU_LOG_PRINT, "Found page [%p] associated to UVA [%p]\n", nfgpu_pages[nfgpu_npages], addr );

#ifdef ALLOC_VMAP_PAGES
       // Alloc new pages for a kernel virtual mapping over same GPU memory !!!
       maddr = pte_pfn(*pte) << PAGE_SHIFT;
       if( pmd_large(*pte) mlen = PAGE_SIZE * 512; // == PG_LEVEL_2M;
       else                mlen = PAGE_SIZE;       // == PG_LEVEL_4K;
       nfgpu_pages_base [ nfgpu_npages ] = maddr;
       nfgpu_pages_len  [ nfgpu_npages ] = mlen;
       nfgpu_pages_vmap [ nfgpu_npages ] = nfgpu_alloc_pages( (void *) maddr, mlen );
       nfgpu_pages_num  [ nfgpu_npages ] = DIV_ROUND_UP( offset_in_page(maddr) + mlen, PAGE_SIZE );
       nfgpu_tpages += nfgpu_pages_num[ nfgpu_npages ];
#endif

       nfgpu_npages++;
   } //endif

   return 0;
}

static int nfgpu_set_mempool( char __user *buf )
{
   struct mm_walk         mmw = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
   struct mm_struct      *mm  = current->mm;
   struct nfgpu_mem_info  gb;
   int                    err=0;
#ifdef ALLOC_VMAP_PAGES
   struct page          **pages;
   int                    i, j;
#endif

   // Avoid reentrances
   spin_lock( &nfgpu_slock );

   // Get data from ioctl
   copy_from_user( &gb, buf, sizeof(struct nfgpu_mem_info) );

   // Prepare global data
   nfgpu_uva    = (unsigned long) gb.uva;
   nfgpu_size   = gb.size;
   nfgpu_npages = 0;
#ifdef ALLOC_VMAP_PAGES
   nfgpu_tpages = 0;  // New kernel pages associated to GPU memory
#endif

   // Collect all pages associated to GPU memory (probably LARGE pages)
   // and alloc enough 4KB pages to create a virtual mapping
   nfgpu_log( NFGPU_LOG_PRINT, "Obtaining pages associated to GPU memory ...\n" );

   // Note: Must be according to page levels -> This should work kernel option 'mem=nopentium'
   mmw.pte_entry = nfgpu_skb_pte_process;    // Works with 4KB pages
   //mmw.pmd_entry = nfgpu_skb_pte_process;  // Works with 'large' pages (2MB)
   mmw.mm = mm;  // Process MM where VMAs can be followed
   walk_page( nfgpu_uva, nfgpu_uva + nfgpu_size, &mmw );

#ifdef ALLOC_VMAP_PAGES
   // Create a final array with allocated pages
   pages = (struct page **) kmalloc( nfgpu_tpages * sizeof(*pages), GFP_KERNEL );
   if( pages == NULL )  return -EFAULT;
   for( i=0,j=0; i<nfgpu_npages; i++ )
   {
      if( nfgpu_pages_vmap[i] == NULL )  return -EFAULT;
      memcpy( &pages[j], nfgpu_pages_vmap[i], nfgpu_pages_num[i] * sizeof(*pages) );
      j += nfgpu_pages_num[i];
   } //endfor
   nfgpu_log( NFGPU_LOG_PRINT, "Allocated new %d pages for KVA [%p]\n", nfgpu_tpages );
#endif

   // Map pages assigned to GPU, to kernel virtual address space
#ifdef ALLOC_VMAP_PAGES
   nfgpu_kva = (unsigned long) vmap( pages, nfgpu_tpages, GFP_KERNEL, PAGE_KERNEL );
#else
   nfgpu_kva = (unsigned long) vmap( nfgpu_pages, nfgpu_npages, GFP_KERNEL, PAGE_KERNEL );
#endif

   if( !nfgpu_kva )
   {
      nfgpu_log( NFGPU_LOG_PRINT, "Error: No KVA from GPU UVAs could be obtained (mapped)\n" );
      err = -EFAULT;

      // Set all 'GPU descriptors' unassigned
      memset( nfgpu_skbs, 0, sizeof(nfgpu_skbs) );
   }
   else
   {
      nfgpu_log( NFGPU_LOG_PRINT, "KVA [%p] was mapped from UVA [%p]. Good!\n", nfgpu_kva, nfgpu_uva );
   } //endif

   // Free table with all pages used for virtual mapping
#ifdef ALLOC_VMAP_PAGES
   kfree( pages );
#endif

   // Allow reentrance
   spin_unlock( &nfgpu_slock );

   return err;
}

ssize_t nfgpu_read( struct file *filp, char __user *buf, size_t c, loff_t *fpos )
{
   return -EFAULT;
}

ssize_t nfgpu_write( struct file *filp, const char __user *buf, size_t count, loff_t *fpos )
{
   return -EFAULT;
}

static unsigned int nfgpu_poll( struct file *filp, poll_table *wait )
{
   return 0;
}

static long nfgpu_ioctl( struct file *filp, unsigned int cmd, unsigned long arg )
{
   int err = 0;

   if( _IOC_TYPE(cmd) != NFGPU_IOCTL_MAGIC )  return -ENOTTY;
   if( _IOC_NR(cmd) > 4 )                     return -ENOTTY;

   if( _IOC_DIR(cmd) & _IOC_READ )
      err = !access_ok( VERIFY_WRITE, (void __user *) arg, _IOC_SIZE(cmd) );
   else if( _IOC_DIR(cmd) & _IOC_WRITE )
      err = !access_ok( VERIFY_READ, (void __user *) arg, _IOC_SIZE(cmd) );
   if( err )
      return -EFAULT;

   // Execute cmd
   switch( cmd )
   {
      case TOGPU_IOCTL_SET_BUFFERS:

         nfgpu_log( NFGPU_LOG_PRINT, "Ioctl [ TOGPU_IOCTL_SET_BUFFERS ] called!\n" );
         err = nfgpu_set_mempool( (char *) arg );
         break;

      case TOGPU_IOCTL_GET_BUFFERS:  // RFU
         break;

      case TOGPU_IOCTL_START:

         nfgpu_log( NFGPU_LOG_PRINT, "Ioctl [ TOGPU_IOCTL_START ] called!\n" );
         nfgpu_running = true;
         break;

      case TOGPU_IOCTL_STOP:

         nfgpu_log( NFGPU_LOG_PRINT, "Ioctl [ TOGPU_IOCTL_STOP ] called!\n" );
         if( (nfgpu_running != false) && (nfgpu_kva != 0) )
         {
            // Unmap old buffer to avoid wasting all VMALLOC space!
            vunmap( (void *) nfgpu_kva );
            //nfgpu_kva = 0;  Let remaining allocated SKBs to be free
         } //endif
         nfgpu_running = false;
         break;

      default:
         err = -ENOTTY;
         break;
   } //endswitch

   return err;
}

int nfgpu_open( struct inode *inode, struct file *filp )
{
   if( !atomic_dec_and_test(&nfgpu_av) )
   {
      atomic_inc( &nfgpu_av );
      return -EBUSY;
   } //endif
   //filp->private_data = &nfgpudev;
   return 0;
}

int nfgpu_release(struct inode *inode, struct file *file)
{
   atomic_set( &nfgpu_av, 1 );
   return 0;
}

static int nfgpu_mmap( struct file *filp, struct vm_area_struct *vma )
{
   return -EINVAL;
}

static struct file_operations  nfgpu_ops =
{
   .owner          = THIS_MODULE,
   .read           = nfgpu_read,
   .write          = nfgpu_write,
   .poll           = nfgpu_poll,
   .unlocked_ioctl = nfgpu_ioctl,
   .open           = nfgpu_open,
   .release        = nfgpu_release,
   .mmap           = nfgpu_mmap,
};

static int nfgpu_internal_init( void )
{
   struct device  *device;
   int             result;
   int             devno;

   // Create and alloc a dev class prior to creating a device
   nfgpu_cls = class_create( THIS_MODULE, "NFGPU_DEV_NAME" );
   if( IS_ERR(nfgpu_cls) )
   {
      result = PTR_ERR( nfgpu_cls );
      nfgpu_log( NFGPU_LOG_ERROR, "Can't create dev class for NFGPU\n" );
      return result;
   } //endif

   // Alloc dev structure now
   result = alloc_chrdev_region( &nfgpu_devno, 0, 1, NFGPU_DEV_NAME );
   devno = MAJOR( nfgpu_devno );
   nfgpu_devno = MKDEV( devno, 0 );
   if( result < 0 )
   {
      nfgpu_log( NFGPU_LOG_ERROR, "Can't get major\n" );
   }
   else
   {
      memset( &nfgpu_cdev, 0, sizeof(struct cdev) );
      cdev_init( &nfgpu_cdev, &nfgpu_ops );
      nfgpu_cdev.owner = THIS_MODULE;
      nfgpu_cdev.ops = &nfgpu_ops;
      result = cdev_add( &nfgpu_cdev, nfgpu_devno, 1 );
      if( result )
      {
          nfgpu_log( NFGPU_LOG_ERROR, "Can't add device %d", result );
      } //endif

      // Create '/dev/togpu' device
      device = device_create( nfgpu_cls, NULL, nfgpu_devno, NULL, NFGPU_DEV_NAME );
      if( IS_ERR(device) )
      {
         nfgpu_log( NFGPU_LOG_ERROR, "Creating device failed\n" );
         result = PTR_ERR( device );
      } //endif
   } //endif

   // Init resources
   spin_lock_init( &nfgpu_slock );
   nfgpu_kva = 0;
   nfgpu_uva = 0;
   nfgpu_next = 1;  // 0 is reserced for sync with GPU
   nfgpu_npages = 0;
   nfgpu_packets = 0;  // Profiling
   nfgpu_average = 0;  // Profiling
   nfgpu_running = false;

   // Start hook subsystem for 'skb' allocation routines
   hook_init();

   nfgpu_log( NFGPU_LOG_PRINT, "Hooking '__netdev_alloc_skb' to hack SKB allocs\n" );
   symbol_hook( &nfgpu_sym_netdev_alloc, "__netdev_alloc_skb", (unsigned long *) __netdev_alloc_skb_togpu );
   nfgpu_log( NFGPU_LOG_PRINT, "Hooking 'skb_release_data' to hack SKB frees\n" );
   symbol_hook( &nfgpu_sym_skb_release_data, "skb_release_data", (unsigned long *) skb_release_data_togpu );

   // Start support thread
   nfgpu_log( NFGPU_LOG_PRINT, "Launching profiling thread\n" );
   nfgpu_thread_task = kthread_create( nfgpu_thread, NULL, "togpu_support" );
   if( nfgpu_thread_task != NULL )
   {
      wake_up_process( nfgpu_thread_task );
   } //endif

   nfgpu_log( NFGPU_LOG_PRINT, "Netfilter GPU target loaded\n");

   return xt_register_target( &nfgpu_target_reg );
}

static int __init  nfgpu_init( void )
{
   return nfgpu_internal_init();
}

static void __exit  nfgpu_exit( void )
{
   if( nfgpu_thread_task != NULL )
   {
      kthread_stop( nfgpu_thread_task );
   } //endif

   // Unregister netfilter's hook
   xt_unregister_target( &nfgpu_target_reg );
}

module_init( nfgpu_init );
module_exit( nfgpu_exit );
