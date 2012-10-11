#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netfilter.h>
#include <linux/types.h>

#include <xtables.h>

enum
{
   O_TO_GPU = 0,
};

enum
{
   PARAM_TO_GPU = (1<<O_TO_GPU),
};

struct xt_togpu_info
{
   u_int32_t to;
};

static void TOGPU_help(void)
{
   printf(
      "TOGPU target options:\n"
      "  --to-gpu           <gpuindex>         Set GPU to send packets to\n"
   );
}

static struct option TOGPU_opts[] =
{
   { "to-gpu", 1, NULL, '1' },
   { .name = NULL }
};

static int TOGPU_parse(       int                      c,
                              char                   **argv,
                              int                      invert,
                              unsigned int            *flags,
                        const void                    *entry,
                              struct xt_entry_target **target
                      )
{
   struct xt_togpu_info  *togpu_info = (struct xt_togpu_info *) (*target)->data;
   unsigned int           value;

   switch( c )
   {
      case '1':  // "--to-gpu" parameter

         xtables_param_act( XTF_ONLY_ONCE, "TOGPU", "--to-gpu", *flags & PARAM_TO_GPU );
         if( !xtables_strtoui(optarg,NULL,&value,0,UINT32_MAX) )
         {
            xtables_param_act( XTF_BAD_VALUE, "TOGPU", "--to-gpu", optarg );
         } //endif
         *flags |= PARAM_TO_GPU;
         togpu_info->to = value;
         return 1;
   } //endswitch

   return 1;
}

static void TOGPU_check( unsigned int flags )
{
   if( !(flags & PARAM_TO_GPU) )
   {
      xtables_error( PARAMETER_PROBLEM, "TOGPU: You must specify destination GPU" );
   } //endif
}

static void TOGPU_print( const void                    *ip,
                         const struct xt_entry_target  *target,
                               int                      numeric
                       )
{
   const struct xt_togpu_info *togpu_info = (const struct xt_togpu_info *) target->data;

   printf( " --to-gpu=%u", togpu_info->to );
}

static struct xtables_target TOGPU_reg =
{
   .name           = "TOGPU",
   .version        = XTABLES_VERSION,
   .family         = NFPROTO_IPV4,
   .size           = XT_ALIGN(sizeof(struct xt_togpu_info)),
   .userspacesize  = XT_ALIGN(sizeof(struct xt_togpu_info)),
   .help           = TOGPU_help,
   .print          = TOGPU_print,
   .parse          = TOGPU_parse,
   .final_check    = TOGPU_check,
   .extra_opts     = TOGPU_opts,
};

void _init(void)
{
   xtables_register_target( &TOGPU_reg );
}

