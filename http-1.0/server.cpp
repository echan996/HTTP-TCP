#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <netdb.h>
#include <inttypes.h>
#include <cstdlib>

#include "HttpRequest.h"
#include "HttpResponse.h"
#define BUFFERSIZE 65536

int service_client(int client_fd, string start_directory){ //given the file descriptor of a client (we are in a new thread), 
	//return an int that will be used for exit. (0 if ok, 1 if error, etc.)
	//done?:
	//receive a message from the client. this should be an http request.
	HttpRequest client_request = HttpRequest();
	char buf[BUFFERSIZE];
	int n_bytes_received = recv(client_fd, buf, BUFFERSIZE-1, 0);
	if(n_bytes_received>0){
		buf[n_bytes_received] = '\0';
	} else {
		cout << "Buffer size is 0!" << endl;
		exit(1);
	}
	//char* buf = "GET /a.txt HTTP/1.0\r\nUser-Agent: Test\r\nAccept: */*\r\nHost: localhost\r\nConnection: Close\r\n\r\n\0";
	vector<char> blob(buf, buf+n_bytes_received);
	for (auto i = blob.begin(); i != blob.end(); ++i)
    std::cerr << *i;
	//vector<char> blob(buf, buf+strlen(buf));
	//decode the http request into a certain format.
	int return_value = client_request.decode(blob);
	HttpResponse server_response = HttpResponse();
	vector<char> wire_response;
	if (return_value != 0){
		//error 400: malformed request!
		server_response.set_status(400);
		server_response.set_description("Bad Request");
		server_response.set_header("Connection", "close");
		wire_response = server_response.encode();
		ssize_t chars_sent = send(client_fd, &wire_response[0], wire_response.size(), 0);
		for (auto i = wire_response.begin(); i != wire_response.end(); ++i)
		std::cout << *i;
		if (chars_sent < 0){
			cerr << "Error in sending response!" << endl;
			return 1;
		} else 
			return 0;
	}
	string got_url = client_request.get_url();
	string requested_file = got_url.substr(got_url.find("/"));
	if (requested_file.compare("/") == 0) requested_file = "/index.html";
	string filepath = start_directory + requested_file;
	//fetch the appropriate file from the system. (http://www.cplusplus.com/doc/tutorial/files/)
	streampos file_size;
	char * file_block;
	ifstream file(filepath, ios::in|ios::binary|ios::ate); //ios::ate for end of file to find size...
	if(file.is_open()){
		file_size = file.tellg();
		file_block = new char[file_size];
		file.seekg(0,ios::beg);
		file.read(file_block,file_size);
		file.close();
		//return the file to the client
		//status 200: ok
		server_response.set_status(200);
		server_response.set_description("OK");
		server_response.set_header("Connection", "close");
		server_response.set_header("Content-Length", to_string(file_size));
		vector<char> payload(file_block,file_block+file_size);
		server_response.set_payload(payload);
		wire_response = server_response.encode();
		for (auto i = wire_response.begin(); i != wire_response.end(); ++i)
		std::cout << *i;
		ssize_t chars_sent = send(client_fd, &wire_response[0], wire_response.size(), 0);
		if (chars_sent < 0){
			cerr << "Error in sending response!" << endl;
			delete[] file_block; return 1;
		}
	} //file_block now contains the file in memory...
	else {
		//error 404: not exist
		server_response.set_status(404);
		server_response.set_description("Not Found");
		server_response.set_header("Connection", "close");
		wire_response = server_response.encode();
		for (auto i = wire_response.begin(); i != wire_response.end(); ++i)
		std::cout << *i;
		ssize_t chars_sent = send(client_fd, &wire_response[0], wire_response.size(), 0);
		if (chars_sent < 0){
			cerr << "Error in sending response!" << endl;
			return 1;
		} else 
			return 0;
	}
	
	delete[] file_block;
	return 0;
}
int main(int argc, char** argv){
	//default hostname: localhost. default port: 4000. default directory: . (current working directory)
	char* node_address = "localhost";
	int port_num = 4000; //to convert to a getaddrinfo, use std::string port_num_s = std::to_string(port_num); char const *port_num_char = port_num_s.c_str();
	char* endptr; //for strtoimax
	string directory = ".";
	if(argc != 1){
		node_address = argv[1];
		port_num = strtoimax(argv[2],&endptr,10);
		directory = argv[3];
	}
	struct addrinfo hints, *results;
	memset(&hints,0,sizeof hints);
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_flags=AI_PASSIVE;
	int status = getaddrinfo(argv[1], std::to_string(port_num).c_str(), &hints, &results);
    if (status) {
        fprintf(stderr, "%s\n", gai_strerror(status));
        return status;
    }
	struct addrinfo *p; int sockfd; int yes=1;
    for(p = results; p != NULL; p = p->ai_next) { //find a good ip to bind to
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) continue;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) return -1;
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        break;
    }
	freeaddrinfo(results);

	if (p == NULL)  {
		fprintf(stderr, "could not bind to address!\n");
		return 1;
	}
	if (listen(sockfd, BUFFERSIZE) == -1) {
		perror("listen");
		return 2;
	}
	fprintf(stderr,"a %s %s\n",argv[1],std::to_string(port_num).c_str());
	for(;;){//infinite loop: accept a connection, then in child code, service the connection.
		struct sockaddr_storage client_addr;
		socklen_t sin_size = sizeof client_addr;
		int client_fd = accept(sockfd, (struct sockaddr*) &client_addr, &sin_size);
		if (client_fd==-1){//error
			perror("accept");
		}
		if(fork() != 0){ //child: service the request (with client_fd)
			close(sockfd);
			//fprintf(stderr,"got here!\n");
			int service_result = service_client(client_fd, directory);
			close(client_fd);
			return service_result;
		}  else { //server:
			
			close(client_fd);
		}
	}
}