#ifndef _UGLYVPN_LOG_H_
#define _UGLYVPN_LOG_H_

#include <signal.h>
#include <errno.h>

void uvpn_syslog (int priority, char *format, ...);

#endif