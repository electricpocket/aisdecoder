
     main.cpp  --  AIS Decoder
 
     Copyright (C) 2013
       Astra Paging Ltd / AISHub (info@aishub.net) and Copyright (C) 2015 Pocket Mariner Ltd.
       
  Modified by Pocket Mariner 2015 to support serial/USB out on BeagleBone and tcp sockets as well as udp sockets
  Retries (rather than fails)  if network connection is required and socket is closed or not open
  ingores write fails with SIGNAL
 
     AISDecoder is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation; either version 2 of the License, or
     (at your option) any later version.
 
     AISDecoder uses parts of GNUAIS project (http://gnuais.sourceforge.net/)
 
 
To build:-
 
cd build
cmake ../ -DCMAKE_BUILD_TYPE=RELEASE
make


 
 
