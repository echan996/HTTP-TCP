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
#include <cstdint>
#include <sys/time.h>
#include "TCPMessage.h"

#define TIMEOUT_MS 500
#define PACKETSIZE 1032
#define MAX_SEQNO 30720
const int FIN = 1, SYN = 2, ACK = 4;
using namespace std;
int rcvWin = 30720;
socklen_t addr_len;
//FIN=1, SYN=2, ACK=4
//const int SYNACK = 0; const int PACKET = 1; const int TIMEOUT = 2; const int FIN = 4;

timeval expire_time;
uint16_t current_seq_no;
uint16_t current_ack_no;
int retransmitting = 0;
int last_successful_ack_no = 0;
int second_last_successful_ack_no = 0;

int handshake_ack(int sockfd, addrinfo *p, uint16_t seqNo, uint16_t ackNo, uint16_t flags= ACK){
	TCPMessage ack_message;
	ack_message.seqNo = seqNo;
	ack_message.ackNo = ackNo;
	ack_message.rcvWin = rcvWin;
	ack_message.flags = flags;
	if (flags&ACK){
		cout << "Sending ACK packet " << current_ack_no;
		if (retransmitting){
			cout << " Retransmission";
			retransmitting = 0;
		}
		cout << endl;
	}
	vector<uint8_t> ack_wire = encode(ack_message);
	string ack_string(ack_wire.begin(),ack_wire.end());
	return sendto(sockfd, ack_string.c_str(), ack_string.size(), 0, p->ai_addr, p->ai_addrlen);
}

int handshake(int sockfd, addrinfo *p){
	//return on success... otherwise keep looping!
	TCPMessage syn_message;
	syn_message.seqNo = current_seq_no;
	syn_message.ackNo = current_ack_no;
	syn_message.rcvWin = rcvWin;
	syn_message.flags = SYN;
	vector<uint8_t> syn_wire = encode(syn_message);
	int bytes_sent, bytes_received;
	char synack_buffer[1024];
	setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO, (char*)&expire_time, sizeof(struct timeval));
	//send SYN
	TCPMessage synack_message;
	string syn_string(syn_wire.begin(),syn_wire.end());
	while ((bytes_sent = sendto(sockfd, syn_string.c_str(), syn_string.size(), 0, p->ai_addr, p->ai_addrlen)) != -1){
			//receive SYNACK. if no SYNACK received, try again
		vector<uint8_t> synack_wire;
		addr_len = sizeof p->ai_addr;
		if ((bytes_received = recvfrom(sockfd, synack_buffer, 1024, 0, p->ai_addr, &addr_len)) != -1){
			synack_wire.insert(synack_wire.end(), &synack_buffer[0], &synack_buffer[bytes_received]);
			synack_message = decode(synack_wire);
			if ((synack_message.flags == (SYN|ACK)) && (synack_message.seqNo == current_ack_no)){ //send synack and break from loop. if the sender's seq = the one we expect (ack) then we are good.
				//send an ack.
				while (handshake_ack(sockfd, p, synack_message.ackNo, (current_ack_no+8)%MAX_SEQNO) <= 0) continue; //it'll break when bytes sent (but not necessarily received)
				current_seq_no = synack_message.ackNo;
				current_ack_no = (current_ack_no+8)%MAX_SEQNO;
				//suppose we receive another SYNACK. then we try sending an ack again? BIG SHIT BIG BIG SHIT SHIT 
				return 0;
			}
		}
	}
}

int receive_file(int sockfd, addrinfo *p){
	//rcvWin, current_ack_no, current_seq_no, are global
	//for now, assume no packet loss.
	/*
	psuedocode:
	receive data from server, with appropriate sequence numbers. store them in some buffer and then acknowledge the data.
	as soon as you have a guaranteed piece of data (aka no lost packets), store everything into the file.
	*/
	
	//open a file for writing.
	ofstream outfile; int bytes_sent, bytes_received;
	outfile.open("received.data", ios::out | ios::binary);
	addr_len = sizeof p->ai_addr;
	int n_times_acked = 0;
	while(1){
		//receive a chunk from the server.
		char chunk[PACKETSIZE];
		if ((bytes_received = recvfrom(sockfd, chunk, PACKETSIZE, 0, p->ai_addr, &addr_len)) != -1){
			vector<uint8_t> server_wire;
			server_wire.insert(server_wire.end(), &chunk[0], &chunk[bytes_received]);
			//if incoming packet's seq number matches my ACK, update my ACK and send new ACK. 
			
			//reply with ACK.
			TCPMessage serverMessage = decode(server_wire);
			cout << "Receiving data packet " << serverMessage.seqNo << endl;
			retransmitting = (serverMessage.seqNo != current_ack_no);
			if (serverMessage.seqNo == current_ack_no){
					//process data

				if(current_ack_no != last_successful_ack_no && current_ack_no != second_last_successful_ack_no){
				string payload (serverMessage.payload.begin(), serverMessage.payload.end());
				outfile << payload;
				}
				if(serverMessage.flags & FIN){
					cerr << "Previous packet was a FIN" << endl;
					current_seq_no = serverMessage.ackNo;
					current_ack_no = (serverMessage.seqNo+8)%MAX_SEQNO;
					break;
				}
				second_last_successful_ack_no = last_successful_ack_no;
				last_successful_ack_no = current_ack_no;
				current_ack_no = (current_ack_no + (serverMessage.payload.size() + 8))%MAX_SEQNO;
				n_times_acked = 0;
				
			} else {
				n_times_acked += 1;
				if (n_times_acked >= 7 && (current_ack_no != last_successful_ack_no)) {
					cerr << "ACKed too many times, setting current_ack_no from " << current_ack_no << " to " << last_successful_ack_no << endl;
					current_ack_no = last_successful_ack_no;
					n_times_acked = 0;
				}
				if (n_times_acked >= 20){
					cerr << "Emergency mode, setting current_ack_no from " << current_ack_no << " to " << second_last_successful_ack_no << endl;
					current_ack_no = second_last_successful_ack_no;
					n_times_acked = 0;
				}
			}
			current_ack_no %= MAX_SEQNO;
			current_seq_no=(current_seq_no+8)%MAX_SEQNO;
			while(handshake_ack(sockfd, p, current_seq_no, current_ack_no)<=0);
		}
	}
	char chunk[PACKETSIZE];
	//setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO, (char*)&expire_time, sizeof(struct timeval));
	
	outfile.close();
	//file finished sending
	//finack
	int finacks_sent = 0;
	while ((handshake_ack(sockfd,p,current_seq_no,current_ack_no,FIN|ACK)) != -1){
		//cerr << "sent our finack " << current_seq_no << " " << current_ack_no << endl;
		finacks_sent++;
		vector<uint8_t> ack_wire;
		char ack_buffer[PACKETSIZE];
		addr_len = sizeof p->ai_addr;
		if ((bytes_received = recvfrom(sockfd, ack_buffer, PACKETSIZE, 0, p->ai_addr, &addr_len)) != -1){
			ack_wire.insert(ack_wire.end(), &ack_buffer[0], &ack_buffer[bytes_received]);
			TCPMessage ack_message;
			ack_message = decode(ack_wire);
			//cerr << "got our ack " << ack_message.seqNo << " " << current_ack_no << endl;
			//check wire's ack and compare it with the syn.
			if (ack_message.seqNo == current_ack_no){
				current_seq_no = ack_message.ackNo;
				current_ack_no = (ack_message.seqNo+8)%MAX_SEQNO;
				return 0;
			}
		}
		if (finacks_sent > 25){
			cerr << "Server probably closed connection. Check resulting file to see if correct." << endl;
			return -1;
		}
	}
	
}

int main(int argc, char** argv){
	if (argc != 3){
		printf("Usage: ./client [IP] [PORT]");
		return 1;
	}
	expire_time.tv_sec = 0;
	expire_time.tv_usec = TIMEOUT_MS * 1000;
	char* url = argv[1]; char * endptr;
	int port = strtoimax(argv[2],&endptr,10);
	struct addrinfo hints, *results;
	memset(&hints,0,sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	int status = getaddrinfo(url, to_string(port).c_str(), &hints, &results);
    if (status) {
        fprintf(stderr, "%s\n", gai_strerror(status));
        return status;
    }
	struct addrinfo *p; int sockfd;
    for(p = results; p != NULL; p = p->ai_next) { //find a good ip to bind to
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) continue;
		//if (bind(sockfd, p->ai_addr, p->ai_addrlen == -1)){
		//	close(sockfd); continue;
		//}
        break;
    }
	if (p == NULL) {
		cerr << "Failed to connect";
		return -1;
    }
	int bytes_sent;
	/*char* abc = "test";
	if ((bytes_sent = sendto(sockfd, abc, strlen(abc), 0, p->ai_addr, p->ai_addrlen)) == -1){
		return -1;
	}*/
	
	 //start handshake
	 
	current_seq_no = 0; current_ack_no = 0;
	handshake(sockfd, p);
	receive_file(sockfd, p);
	freeaddrinfo(results);
}