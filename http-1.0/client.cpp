#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <cstring>
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

using namespace std;

int main(int argc, char** argv){
	if (argc != 2){
		printf("Usage: ./web-client [URL]");
		return 1;
	}
	string url = argv[1];
	size_t host_start = url.find("://");
	if (host_start == string::npos) host_start = 0;
	else host_start += 3;
	size_t host_end = host_start;
	while(url[host_end] != ':' && url[host_end] != '/'){
		host_end++;
		if (host_end == url.size()) break;
	}
	string scheme = "HTTP";
	string server_name = url.substr(host_start,host_end-host_start);
	string port = "";
	if(url[host_end] == ':'){
		host_end++;
		size_t port_end = url.find("/",host_end);
		if(port_end == string::npos){
			port = url.substr(host_end);
		} else {
		port = url.substr(host_end,port_end-host_end);
		}
	}
	if(port.compare("") == 0) port = "80";
	size_t string_path_pos = url.find("/",host_end);
	string path;
	if (string_path_pos == string::npos){
		path = "/";
	} else {
		path = url.substr(string_path_pos);
	}
	
	cerr << server_name << " " << port << " " << path << endl;
	//we have parsed the URL!
	struct addrinfo hints, *results;
	memset(&hints,0,sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	//find a socket to connect to
	int status = getaddrinfo(server_name.c_str(), port.c_str(), &hints, &results);
    if (status) {
        fprintf(stderr, "%s\n", gai_strerror(status));
        return status;
    }
	struct addrinfo *p; int sockfd;
    for(p = results; p != NULL; p = p->ai_next) { //find a good ip to bind to
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) continue;
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        break;
    }
	freeaddrinfo(results);
	if (p == NULL)  {
		fprintf(stderr, "could not connect to address!\n");
		return -1;
	}
	//we have connected. request the page... TODO:
	//construct an HTTP request. sockfd contains the server socket we want to send to.
	HttpRequest client_request = HttpRequest();
	client_request.set_url(url);
	client_request.set_method("GET");
	client_request.set_header("Accept","*/*");
	client_request.set_header("Connection","Close");
	vector<char> wire_request = client_request.encode();
	for (auto i = wire_request.begin(); i != wire_request.end(); ++i){
    cerr << *i;
	}
	ssize_t chars_sent = send(sockfd, &wire_request[0], wire_request.size(), 0);
	if (chars_sent <= 0){
		//error
		cerr << "Error in sending!" << endl;
	}
	//receive the response.
	unsigned int buffer_size = 65536; unsigned int vector_pos = 0; int bytes_received; unsigned int size_left=0;
	vector<char> response_buffer(buffer_size);
	while(1){
		size_left= response_buffer.size() - vector_pos;
		if (size_left < (buffer_size/4)){
				buffer_size *= 2;
				response_buffer.resize(buffer_size);
				size_left = response_buffer.size() - vector_pos;
			}
		if((bytes_received = recv(sockfd,&response_buffer[vector_pos],size_left,0)) > 0){
			vector_pos+=bytes_received;
		} else { //error or end of file
			break;
		}

	} //now, vector_pos should point 1 past the end of the vector...
	
	response_buffer.resize(vector_pos);
	
	for (auto i = response_buffer.begin(); i != response_buffer.end(); ++i)
    cerr << *i;
	
	//parse the response.
	HttpResponse server_response = HttpResponse();
	server_response.decode(response_buffer);
	if(server_response.get_status() != 200){
		//error
		cerr << "error: " << server_response.get_description() << endl;
		return server_response.get_status();
	} else { //save the file
		size_t found = path.find_last_of("/") + 1;
		path = path.substr(found);
		ofstream out(path, ios::out|ios::binary);
		if (out.is_open()){
		vector<char> payload = server_response.get_payload();
		copy(payload.begin(), payload.end(), std::ostreambuf_iterator<char>(out));
		out.close();
		} else {
			cerr << "vector not open" << endl;
		}
	}
	return 0;
}