// Gerald Abut
// ICS 451 Assignment 12: Learning Switch Simulation
// 5/4/2015

Notes:
Project simulates a Learning Switch (in the Data-Link layer). 
This was implemented using the C programming language and multi-threading to send and receive packets. 
Each switch will "learn"​ by storing the address of the packet it receives on a table. 
Each switch has it's own separate table for sending or forwarding packets.

How to run:
ething.c and ethlearn.c was compiled using gcc.
debugging prints can be displayed by make PRINT = 1

test 1:
ethlearn  45600/45700  45601/45800
ethlearn  45700/45600  45701/45900
ethping   45800  45601  800  12:34:44:55:66:77  99:88:77:66:55:44
ethping   45900  45701  300  99:88:77:66:55:44  12:34:44:55:66:77  

test 2:
ethlearn  45100/45200  45101/45300  45102/45400
ethlearn  45200/45100  45201/45500
ethlearn  45300/45101  45301/45600
ethping   45400  45102  100  12:34:44:55:66:77  99:88:77:66:55:44
ethping   45500  45201  200  99:88:77:66:55:44  12:34:44:55:66:77  
ethping   45600  45301  300  ab:cd:ef:00:01:02  12:34:44:55:66:77 
