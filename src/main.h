#ifndef __MAIN_H
#define __MAIN_H


// ============================= Include files ==========================


    #include "stdio.h"
    #include <string.h>
    #include <stdlib.h>
    #include <pthread.h>
    #include <stdint.h>
    #include <errno.h>
    #include <unistd.h>
    #include <math.h>
    #include <sys/time.h>
    #include <sys/timeb.h>
    #include <signal.h>
    #include <fcntl.h>
    #include <ctype.h>
    #include <sys/stat.h>
    #include <sys/ioctl.h>



int openTcpSocket(const char *host, const char *portname);
int openUdpSocket(const char *host, const char *portname);
int openSerialOut();
#endif
