#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#include "log.h"

void uvpn_syslog (int priority, char *format, ...)
{
   static volatile sig_atomic_t in_syslog= 0;
   char buf[255];
   va_list ap;

   if(!in_syslog) {
      in_syslog = 1;
    
      va_start(ap, format);
      vsnprintf(buf, sizeof(buf)-1, format, ap);
      syslog(priority, "%s", buf);
      va_end(ap);

      in_syslog = 0;
   }
}
