# HTTP-1.0

Implementation of HTTP-1.0 protocol in C++. Program includes client and server modules for web communication. Program is compiled using the included makefile. 

Make sure to:
Install boost (sudo apt-get install libboost-dev) prior to compiling the program.

Webserver module is invoked as follows:
$ ./web-server [host] [port] [file-directory]

The default arguments for the web-server are localhost, 4000, and cwd. Server handles concurrent connections by forking to service each new client that connects to the server. 

Webclient module is invoked as follows:
$ ./web-client [URL] [URL]...

The client accepts multiple URLs as arguments for objects requested for download. It first establishes a connection with the server specified by the URL before requesting the specific object. The file is then saved in the working directory of the client using the name given in the URL. 

