/*
 *    main.cpp  --  AIS Decoder
 *
 *    Copyright (C) 2013
 *      Astra Paging Ltd / AISHub (info@aishub.net) and 2015 Pocket Mariner Ltd.
 *
 *    AISDecoder is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    AISDecoder uses parts of GNUAIS project (http://gnuais.sourceforge.net/)
 *
 *
 *
cd build
cmake ../ -DCMAKE_BUILD_TYPE=RELEASE
make
 *
 *
 * Modified by Pocket Mariner 2015 to support serial/USB out and tcp sockets as well as udp sockets
 * Retries (rather than fails)  if network connection is required and socket is closed or not open
 * ingores write fails with SIGNAL
 */

#include "main.h"
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include "config.h"
#include "sounddecoder.h"
#include "callbacks.h"

#define HELP_AUDIO_DRIVERS "file"

#define HELP_AUDIO_DEVICE ""

#define HELP_MSG \
		"Usage: " PROJECT_NAME " -h hostname -p port [-t protocol]  [-f /path/fifo] [-l] [-d] [-n]\n\n"\
		"-h\tDestination host or IP address\n"\
		"-p\tDestination port\n"\
		"-t\tProtocol 0=UDP 1=TCP (UDP default)\n"\
		"-f\tFull path to 48kHz raw audio file (default /tmp/aisfifo)\n"\
		"-l\tLog sound levels to console (stderr)\n"\
		"-d\tLog NMEA sentences to console (stderr)\n"\
		"-n\tOutput to serial/NMEA device name e.g. /dev/ttyO0 (BeagleBone Black J1)\n"\
		"-H\tDisplay this help\n"

#define MAX_BUFFER_LENGTH 2048
#define TIME_STAMP_LENGTH 21
static char buffer[MAX_BUFFER_LENGTH];
static unsigned int buffer_count=0;

static int sock;
static struct addrinfo* addr=NULL;
static struct sockaddr_in serv_addr;
static int protocol=0; //1 = TCP, 0 for UDP

static int initSocket(const char *host, const char *portname);
static int show_levels=0;
static int debug_nmea=0;
static int serial_nmea=0;
static char* serial_device=NULL;
static int ttyfd=-1; //file descriptor for console outpu
static int share_nmea_via_ip=0;

char *host=NULL,*port=NULL;
static char timestamp[TIME_STAMP_LENGTH];
 

void sound_level_changed(float level, int channel, unsigned char high) {
    if (high != 0)
        fprintf(stderr, "Level on ch %d too high: %.0f %%\n", channel, level);
    else
        fprintf(stderr, "Level on ch %d: %.0f %%\n", channel, level);
}

char *time_stamp()
{


time_t ltime;
ltime=time(NULL);
struct tm *tm;
tm=localtime(&ltime);

sprintf(timestamp,"%04d-%02d-%02d %02d:%02d:%02d", tm->tm_year+1900, tm->tm_mon, 
    tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
return timestamp;
}



void nmea_sentence_received(const char *sentence,
		unsigned int length,
		unsigned char sentences,
		unsigned char sentencenum) {
	if (sentences == 1) {

		//if (debug_nmea) fprintf(stderr, "length %d", strlen(sentence));
		if (debug_nmea) fprintf(stderr, "%s,%s", time_stamp(), sentence);
		if (serial_nmea && ttyfd > 0) write(ttyfd, sentence,strlen(sentence));

		if (share_nmea_via_ip && protocol ==0)
		{
			if (sendto(sock, sentence, length, 0, addr->ai_addr, addr->ai_addrlen) == -1)
			{
				if (!openUdpSocket(host, port) )
				{
					if (debug_nmea) fprintf(stderr, "%s Failed to to re-open remote socket address!\n",time_stamp() );

				}
				else
				{
					sendto(sock, sentence, length, 0, addr->ai_addr, addr->ai_addrlen);
				}
			}
		}
		else if (share_nmea_via_ip && protocol==1)
		{
			//fprintf(stderr, "%s, write to remote socket address 1\n", time_stamp());
			if (write(sock, sentence, strlen(sentence)) < 0 )
			{
				//it might have timed out - close it and re-open it.

				//fprintf(stderr, "Failed to write to remote socket address!\n");

				if (!openTcpSocket(host, port) )
				{
					if (debug_nmea) fprintf(stderr, "%s Failed to to re-open remote socket address!\n",time_stamp());
					// abort();
				}
				else
				{
					write(sock, sentence, strlen(sentence));
				}

			}

		}


	} else {
		if (buffer_count + length < MAX_BUFFER_LENGTH) {
			memcpy(&buffer[buffer_count], sentence, length);
			buffer_count += length;
		} else {
			buffer_count=0;
		}

		if (sentences == sentencenum && buffer_count > 0) {
			if (debug_nmea) fprintf(stderr, "%s,%s", time_stamp(),buffer);
			if (serial_nmea && ttyfd > 0) write(ttyfd, buffer,strlen(buffer));

			if (share_nmea_via_ip && protocol ==0)
			{
				if (sendto(sock, buffer, buffer_count, 0, addr->ai_addr, addr->ai_addrlen) == -1)
				{
					if (!openUdpSocket(host, port) )
					{
						if (debug_nmea) fprintf(stderr,"%s Failed to to re-open remote socket address!\n",time_stamp());

					}
					else
					{
						sendto(sock, buffer, buffer_count, 0, addr->ai_addr, addr->ai_addrlen);
					}
				}
			}
			else if (share_nmea_via_ip && protocol==1)
			{
				//fprintf(stderr, "%s write to remote socket address 2\n",time_stamp());
				if (write(sock,  buffer, buffer_count) < 0 )
				{
					//it might have timed out - close it and re-open it.

					//fprintf(stderr, "%s Failed to write to remote socket address!\n",time_stamp());

					if (!openTcpSocket(host, port) )
					{
						if (debug_nmea) fprintf(stderr, "%s Failed to to re-open remote socket address!\n",time_stamp());
						//abort();
					}
					else
					{
						write(sock,  buffer, buffer_count);
					}

				}

			}

			buffer_count=0;
		};
	}
}

#define CMD_PARAMS "h:p:t:ln:dHf:"



int main(int argc, char *argv[]) {
    Sound_Channels channels = SOUND_CHANNELS_STEREO;
    char *file_name=NULL;
    const char *params=CMD_PARAMS;
    int hfnd=0, pfnd=0;

    // We expect write failures to occur but we want to handle them where
	// the error occurs rather than in a SIGPIPE handler.
	signal(SIGPIPE, SIG_IGN);

	protocol=0;
    file_name="/tmp/aisfifo"; //stereo 48k pcm
    int opt;
    while ((opt = getopt(argc, argv, params)) != -1) {
        switch (opt) {
        case 'h':
            host = optarg;
            hfnd = 1;
            break;
        case 'p':
            port = optarg;
            pfnd = 1;
            break;
		case 't':
			protocol = atoi(optarg);
			break;
		case 'l':
            show_levels = 1;
            break;
        case 'f':
            file_name = optarg;
            break;
        case 'd':
            debug_nmea = 1;
            break;
		case 'n':
			serial_nmea = 1;
			serial_device = optarg;
			break;
        case 'H':
        default:
            fprintf(stderr, HELP_MSG);
            return EXIT_SUCCESS;
            break;
        }
    }

    if (argc < 2) {
        fprintf(stderr, HELP_MSG);
        return EXIT_FAILURE;
    }

    if (!hfnd) {
        fprintf(stderr, "Host is not set\n");
        return EXIT_FAILURE;
    }

    if (!pfnd) {
        fprintf(stderr, "Port is not set\n");
        return EXIT_FAILURE;
    }

	if (hfnd && pfnd) share_nmea_via_ip =1;


	if (share_nmea_via_ip && !initSocket(host, port)) {
		fprintf(stderr, "%s Failed to configure networking\n",time_stamp());

	}

    if (show_levels) on_sound_level_changed=sound_level_changed;
    on_nmea_sentence_received=nmea_sentence_received;
    Sound_Driver driver = DRIVER_FILE;

    int OK=0;

    if (serial_nmea) openSerialOut();
    OK=initSoundDecoder(channels, driver, file_name);

    int stop=0;
    if (OK) {
        runSoundDecoder(&stop);
    } else {
        fprintf(stderr, "%s %s\n", time_stamp(), errorSoundDecoder);
    }
    freeSoundDecoder();
    freeaddrinfo(addr);
    
    return 0;
}

int initSocket(const char *host, const char *portname) {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	if (sock > -1) close(sock);

	char* testMessage = "aisdecoder connection\r\n";//"!AIVDM,1,1,,A,B3P<iS000?tsKD7IQ9SQ3wUUoP06,0*6F\r\n";
	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_DGRAM;
	hints.ai_protocol=IPPROTO_UDP;
	if (protocol==1)
	{
		hints.ai_family=AF_INET;
		hints.ai_socktype=SOCK_STREAM;
		hints.ai_protocol=0; //let the server decide
	}


	hints.ai_flags=AI_ADDRCONFIG;

	int err=getaddrinfo(host, portname, &hints, &addr);
	if (err!=0) {
		fprintf(stderr, "%s Failed to resolve remote socket address!\n",time_stamp());

		return 0;
	}

	sock=socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (sock==-1) {
		fprintf(stderr, "%s %s",time_stamp(), strerror(errno));

		return 0;
	}
	if (protocol ==1 )
	{
		struct hostent *server;
		server = gethostbyname(host);
		if (server == NULL)
		{
			fprintf(stderr,"%s ERROR, no such host %s",time_stamp(),host);
			exit(0);
		}
		bzero((char *) &serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		bcopy((char *)server->h_addr,
				(char *)&serv_addr.sin_addr.s_addr,
				server->h_length);
		int portno=atoi(portname);
		serv_addr.sin_port = htons(portno);

		if (connect(sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0)
		{
			fprintf(stderr, "%s Failed to connect to remote socket address!\n",time_stamp());
			return 0;
		}

		if (write(sock, testMessage, strlen(testMessage)) < 0 )
		{
			fprintf(stderr, "%s Failed to write to remote socket address!\n",time_stamp());
			return 0;
		}
	}
	return 1;
}

int openTcpSocket(const char *host, const char *portname) {


	if (protocol!=1) return -1;
	if (! initSocket(host, portname)) return -1;
	return 1;

}

int openUdpSocket(const char *host, const char *portname) {


	if (protocol!=0) return -1;
	if (! initSocket(host, portname)) return -1;
	return 1;

}

int openSerialOut()
{
	//https://billwaa.wordpress.com/2014/10/02/beaglebone-black-enable-on-board-serial-uart/
	//to enable other serial ports
	//e.g.
	// Activate UART1
	//echo BB-UART1 > /sys/devices/bone_capemgr.*/slots
	//
	//On BeagleBone Black /dev/ttyO0 is enabled by default and attached to the console
	//you can write to it
	//echo Hello > /dev/ttyO0
	//You can set its speed to the NMEA 38400
	//stty -F /dev/ttyO0 38400  - set console to 38400
	//stty -a -F /dev/ttyO0
	//You can connect and listen to the console on your mac using
	//screen /dev/tty.usbserial 38400
	//exit using ctrl-a d
	char* testMessage = "aisdecoder nmea connection\r\n";//"!AIVDM,1,1,,A,B3P<iS000?tsKD7IQ9SQ3wUUoP06,0*6F\r\n";
	struct termios oldtio, newtio;       //place for old and new port settings for serial port
	ttyfd = open(serial_device, O_RDWR);
	if (ttyfd < 0 )
	{
		fprintf(stderr, "%s Failed to open NMEA port on %s!\n",time_stamp(),serial_device );
		return 0;
	}
	//if (true) //strcmp(serial_device,"/dev/ttyO0")==0 || strcmp(serial_device,"/dev/ttyO1")==0)
	//{
		tcgetattr(ttyfd,&oldtio); // save current port settings
		tcgetattr(ttyfd,&newtio);
		// set new port settings for canonical input processing
		// NMEA 0183 for AIS - 38400, 8data, 1 stop bit , no parity, no handshake
		newtio.c_cflag = B38400 | CS8 | CLOCAL ;
		newtio.c_iflag = IGNPAR;
		newtio.c_oflag = 0;
		newtio.c_lflag = 0;       //ICANON;
		newtio.c_cc[VMIN]=1;
		newtio.c_cc[VTIME]=0;
		tcflush(ttyfd, TCIFLUSH);
		tcsetattr(ttyfd,TCSANOW,&newtio);
	//}
	if( write(ttyfd,testMessage,strlen(testMessage)) <= 0)
	{
		fprintf(stderr, "%s Failed to write to NMEA port %s!\n",time_stamp(), serial_device );
		return 0;
	}

	return 1;

}

