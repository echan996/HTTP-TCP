# TCP atop UDP

The program builds a TCP-like connection atop UDP by using the same protocols in TCP as a means of guaranteeing reliable data transmission.

The protocol used in this project is similar to that of TCP-Tahoe. Modules are tested by communicating with one another. The executables for the TCP-like communication atop UDP are compiled using the supplied makefile. However, the two supplied modules cannot communicate with external sources and serve simple as a demonstration of concept of TCP. The server module will attempt to send a file to the connected client and on success, diff should return nothing.  

<code>$ diff [init] [received]</code>

The <b>server module</b> is invoked as follows:  
<code>$ ./server [PORT] [FILE]</code>

The <b>client module</b> is invoked as follows:  
<code>$ ./client [SERVER IP] [PORT]</code>

The code was tested on Vagrant inside of a VM. The client and server VMs are connected on a private network (10.0.0.0/24)
Client's IP is 10.0.0.2
Server's IP is 10.0.0.3

## Known Errors
---
- Client is unable to transfer multiple GB sized files. Error is most likely a result of integer overflow somewhere.
