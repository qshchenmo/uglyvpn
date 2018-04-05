
#include <stdio.h>
#include <sys/timerfd.h> 
#include <sys/time.h>
#include <time.h>
#include <unistd.h>


int uvpn_timer_create(int expsec)
{
    int fd = -1;
    struct timespec now;
    struct itimerspec new_value; 

    fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (fd < 0)
    {
        return -1;
    }
    
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) 
    {
        return -1;
    }    
    
    new_value.it_value.tv_sec = now.tv_sec + expsec; 
    new_value.it_value.tv_nsec = now.tv_nsec; 
    new_value.it_interval.tv_sec = expsec; 
    new_value.it_interval.tv_nsec = 0; 

    if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &new_value, NULL) == -1) 
    {
        close(fd);
        return -1;
    }
 
    return fd;   
}


