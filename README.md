
     main.cpp  --  AIS Decoder
     
     Demodulates GMSK encoded AIS data to AIS sentences.
 
     
     Based on aisdecoder from  Astra Paging Ltd / AISHub (info@aishub.net)  
     
     http://forum.aishub.net/ais-decoder/ais-decoder-beta-release/ 
       
Modified by Pocket Mariner (C) 2015 to read directly from a fifo and not depend on audio (e.g. pulse) and to support tcp sockets as well as udp sockets. Also serial/USB out on BeagleBone (-n option). Retries (rather than fails)  if network connection is required and socket is closed or not open. Ingores write fails with SIGNAL
 
     AISDecoder is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation; either version 2 of the License, or
     (at your option) any later version.
 
     AISDecoder uses parts of GNUAIS project (http://gnuais.sourceforge.net/)
 
 
To build:-
 
cd build
cmake ../ -DCMAKE_BUILD_TYPE=RELEASE
make

To run:- (reads sound data from /tmp/aisfifo)


#Have aisdecoder listen to a fifo
rm /tmp/aisfifo
mkfifo /tmp/aisfifo
./aisdecoder -h 54.225.113.225 -p 7011 -t 1 -n /dev/ttyO1 -d -f /tmp/aisfifo

then startup rtl_fm or gun-radio to output demodulated AIS data to the fifo (e.g. /tmp/aisfifo)

 
 
