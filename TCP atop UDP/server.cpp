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
#include "TCPMessage.h"
#include <sys/time.h>
#include <cstdint>
#include <map>
#include <algorithm>

#define PACKETSIZE 1032
#define TIMEOUT_MS 500
#define MAX_SEQNO 30720
#define MAX_WINDOW_SIZE 15360
using namespace std;
const int FIN = 1, SYN = 2, ACK = 4;
socklen_t addr_len;

uint16_t serverWin;
timeval expire_time;
uint16_t current_seq_no;
uint16_t current_ack_no;
char* filename;
int sshthresh = 30720;
int win_used = 0;
map<int,int> map_ack_to_pos;
map<int,int> map_pos_to_ack;
map<int,int> map_ack_to_payload_size;
int filepos = 0;

int handshake(int sockfd, addrinfo *p, sockaddr_in &client){
	char syn_buffer[PACKETSIZE];
	//setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO, (char*)&expire_time, sizeof(struct timeval));
	int bytes_received, bytes_sent;
	addr_len = sizeof p->ai_addr;
	bytes_received = recvfrom(sockfd, syn_buffer, PACKETSIZE, 0, p->ai_addr, &addr_len);
	if(bytes_received==-1){
		fprintf(stderr, "Error: invalid recvfrom instruction\n");
		return -1;
	}
	vector<uint8_t> recv_message;
	recv_message.insert(recv_message.end(), &syn_buffer[0], &syn_buffer[bytes_received]);
	//for (auto i = recv_message.begin(); i != recv_message.end(); ++i)
    //std::cerr << (int) *i << " ";
	TCPMessage syn_req = decode(recv_message);
	//cerr << syn_req.seqNo << " " << syn_req.ackNo << " " << syn_req.flags << endl;
	//don't need to check ack and sequence numbers. 
	if(!((syn_req.flags & ACK)||(syn_req.flags^ACK))){
		fprintf(stderr, "Error: invalid handshake protocol\n");
	}
	TCPMessage synack_message, ack_message;
	synack_message.seqNo=syn_req.ackNo;
	synack_message.ackNo=syn_req.seqNo+8;
	synack_message.rcvWin=serverWin;
	synack_message.flags=SYN|ACK;
	//bytes_received has information in it.
	vector<uint8_t> synack_wire= encode(synack_message);
	setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO, (char*)&expire_time, sizeof(struct timeval));
	string synack_string(synack_wire.begin(), synack_wire.end());
	while ((bytes_sent = sendto(sockfd, synack_string.c_str(), synack_string.size(), 0, p->ai_addr, p->ai_addrlen)) != -1){
		cout << "Sending data packet " << current_seq_no << " " << serverWin << " " << sshthresh << " SYN" << endl;
		vector<uint8_t> ack_wire;
		char ack_buffer[PACKETSIZE];
		addr_len = sizeof p->ai_addr;
		if ((bytes_received = recvfrom(sockfd, ack_buffer, PACKETSIZE, 0, p->ai_addr, &addr_len)) != -1){
			ack_wire.insert(ack_wire.end(), &ack_buffer[0], &ack_buffer[bytes_received]);
			ack_message = decode(ack_wire);
			//check wire's ack and compare it with the syn.
			if (ack_message.seqNo == synack_message.ackNo){
				current_seq_no = ack_message.ackNo;
				current_ack_no = ack_message.seqNo+8;
				return 0;
			}
		}	
	}
}

int send_flag_msg(int sockfd, addrinfo *p, vector<uint8_t> payload, uint16_t flags = FIN){
	TCPMessage ack_message;
	ack_message.seqNo = current_seq_no;
	ack_message.ackNo = current_ack_no;
	ack_message.rcvWin = serverWin;
	ack_message.flags = flags;
	ack_message.payload = payload;
	vector<uint8_t> ack_wire = encode(ack_message);
	string ack_string(ack_wire.begin(),ack_wire.end());
	//cerr << "Sending: " << ack_string << endl;
	return sendto(sockfd, ack_string.c_str(), ack_string.size(), 0, p->ai_addr, p->ai_addrlen);
}

int end_transmission(int sockfd, addrinfo *p, sockaddr_in &client){
	TCPMessage fin_message;
	fin_message.seqNo = current_seq_no;
	fin_message.ackNo = current_ack_no;
	fin_message.rcvWin = serverWin;
	fin_message.flags = FIN;
	vector<uint8_t> fin_wire = encode(fin_message);
	int bytes_sent, bytes_received;
	char finack_buffer[1024];
	//setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO, (char*)&expire_time, sizeof(struct timeval));
	//send FIN
	TCPMessage finack_message;
	string fin_string(fin_wire.begin(),fin_wire.end());
	while ((bytes_sent = sendto(sockfd, fin_string.c_str(), fin_string.size(), 0, p->ai_addr, p->ai_addrlen)) != -1){
			//receive FINACK. if no FINACK received, try again
		cout << "Sending data packet " << current_seq_no << " " << serverWin << " " << sshthresh << " FIN" << endl;
		vector<uint8_t> finack_wire;
		addr_len = sizeof p->ai_addr;
		//cerr << "sent fin: " << current_seq_no << " " << current_ack_no << endl;
		if ((bytes_received = recvfrom(sockfd, finack_buffer, 1024, 0, p->ai_addr, &addr_len)) != -1){
			finack_wire.insert(finack_wire.end(), &finack_buffer[0], &finack_buffer[bytes_received]);
			finack_message = decode(finack_wire);
			//cerr << "receiving finack: " << current_seq_no << " " << current_ack_no << ", " << finack_message.seqNo << " " << finack_message.ackNo << endl;
			if ((finack_message.flags == (FIN|ACK)) && (finack_message.seqNo == current_ack_no)){ //send ack and break from loop.
				//send an ack.
				current_seq_no = finack_message.ackNo;
				current_ack_no = (current_ack_no+8)%MAX_SEQNO;
				//cerr << "sending ack: " << current_seq_no << " " << current_ack_no << endl;
				vector<uint8_t> empty_vector;
				while (send_flag_msg(sockfd, p, empty_vector, ACK) <= 0) continue; //it'll break when bytes sent (but not necessarily received)
				//suppose we receive another SYNACK. then we try sending an ack again?
				return 0;
			}
		}
	}
}

void clearmap(int whichhalf){
	//0 is first half, 1 is second half.
	if(!whichhalf){
		cerr << "Clearing first half of map." << endl;
		auto it1 = map_ack_to_pos.begin();
		for (; it1->first < MAX_SEQNO/2 && it1 != map_ack_to_pos.end(); it1++){
			continue;
		}
		map_ack_to_pos.erase(map_ack_to_pos.begin(), it1);
	} else {
		cerr << "Clearing second half of map." << endl;
		auto it1 = map_ack_to_pos.begin();
		for (; it1->first < MAX_SEQNO/2 && it1 != map_ack_to_pos.end(); it1++){
			continue;
		}
		map_ack_to_pos.erase(it1, map_ack_to_pos.end());		
	}
}
int sendblock(int sockfd, addrinfo *p, vector<uint8_t> payload){
	while(send_flag_msg(sockfd, p, payload ,0)<=0) continue;
	if ((current_seq_no > MAX_SEQNO/2) && (current_seq_no + payload.size() + 8 > MAX_SEQNO)){//we wrap around! clear the first half of the map.
		clearmap(0);
	}
	if ((current_seq_no < MAX_SEQNO/2) && (current_seq_no + payload.size() + 8 > MAX_SEQNO/2)){
		clearmap(1);
	}
	current_seq_no = (current_seq_no+(payload.size()+8))%MAX_SEQNO;
	
	return 1;
}

int receive_ack(int sockfd, addrinfo *p, sockaddr_in &client, int i){ //return position of file within map
	char client_buffer[PACKETSIZE];
	vector<uint8_t> recv_message;
	int bytes_received = recvfrom(sockfd, client_buffer, PACKETSIZE, 0, p->ai_addr, &addr_len);
	TCPMessage client_response;
	if(bytes_received==-1){
		cerr << "Listened for ACKs but did not receive anything." << endl;
		//didn't receive anything or packet loss.
		sshthresh = max(serverWin/2, 1024);
		serverWin = 1024;
		return -1;
	} else {
		recv_message.insert(recv_message.end(), &client_buffer[0], &client_buffer[bytes_received]);
		client_response = decode(recv_message);
		cout << "Receiving ACK packet " << client_response.ackNo << endl;
		current_ack_no = client_response.seqNo;
		if (client_response.ackNo == current_seq_no){
			if (serverWin<sshthresh) serverWin*=2;
			//else serverWin+=1024;
		} else {//client didn't receive our portion... transmit it again
			sshthresh = max(serverWin/2, 1024);
			serverWin = 1024;
		}
		if (map_ack_to_pos.count(client_response.ackNo) != 0 && current_seq_no - client_response.ackNo <= MAX_SEQNO/2){
			//cerr << "It's in the map!" << endl;
			if (map_ack_to_pos[client_response.ackNo] + serverWin > i) { // + serverWin > i
				cerr << "map value: " << map_ack_to_pos[client_response.ackNo] << ", NoMoreserverWin:" << serverWin << endl;
				cerr << "Returning: " << i << endl;
				return i;
			} else {
				cerr << "Option 2" << endl;
				cerr << "Returning: " << map_ack_to_pos[client_response.ackNo] << endl;
				return map_ack_to_pos[client_response.ackNo];
			}
		} else {
			return i; //not in map, should send next data
		}
	}
}

int transfer(int sockfd, addrinfo *p, sockaddr_in &client){//Return 1 on success 0 on failure.
	char* file_block;
	ifstream file(filename, ios::in|ios::binary|ios::ate);
	streampos file_size;
	int bytes_received, bytes_sent;
	if(file.is_open()){
		file_size = file.tellg();
		file_block = new char[file_size];
		file.seekg(0,ios::beg);
		file.read(file_block,file_size);
		file.close();
	} else {
		cerr << "file not open!" << endl;
		return 0;
	}
	int client_window = 30720;
	char payload[1024];
	char message[PACKETSIZE];
	int pos = 0;
	//file_block contains the file, and file_size is the size. split into chunks and keep sending chunks.
	//after every chunk, receive an ack. if no ack, then do some congestion control?
	//if ack received and right ack #, send the next chunk. if incorrect ack #, send same chunk again.
	//we have current_ack_no and current_seq_no.
	//we are restricted by: client's window (that it gives back from an ack), server's rcvWin congestion window (dynamic through congestion control), and max file size.
	//congestion window: total amount of shit you can send without getting an ack.
	int retransmitting = 0;
	for(int a=0,i=0; ; a=0){
		
		int can_send = (win_used < serverWin && i < file_size);
		if(can_send){
			//add to map
			map_ack_to_pos[current_seq_no] = i;
			map_pos_to_ack[i] = current_seq_no;
			//send
			//cerr << "i: " << i << endl;
			for(; i < file_size && win_used < client_window && win_used < serverWin && a < 1024; i++, a++, win_used++){ 
				//cerr << win_used << endl;
				if (map_ack_to_payload_size.count(current_seq_no) > 0 && a >= map_ack_to_payload_size[current_seq_no]){
					cerr << "Found a payload with this seq # already..." << endl;
					break;
				}
				payload[a] = file_block[i];     //payload setup
			}
			vector<uint8_t> msg_payload;
			msg_payload.insert(msg_payload.end(), &payload[0], &payload[a]);
			string s_message(msg_payload.begin(), msg_payload.end());
			map_ack_to_payload_size[current_seq_no] = msg_payload.size();
			cerr << "Filepos: " << filepos << endl;
			//cerr << s_message.size() << endl;
			//cerr << "i: " << i << ", win_used: " << win_used << ", client_window: " << client_window << endl;
			cout << "Sending data packet " << current_seq_no << " " << serverWin << " " << sshthresh;
			if (retransmitting){
				cout << " Retransmission";
				retransmitting = 0;
			}
			cout << endl;
			sendblock(sockfd, p, msg_payload);
		}
		else if(filepos<file_size){
			int result = receive_ack(sockfd,p,client,i);
			cerr << "i: " << i << ", result: " << result << ", filepos: " << filepos << ", seqNo: " << current_seq_no << endl;
			if (result == -1){
				win_used = 0;
				i = filepos;
				current_seq_no = map_pos_to_ack[i];
				retransmitting = 1;
			} else if (result < filepos){
				/*cerr << result << " < " << filepos << endl;
				if (map_ack_to_pos.count(result) > 0){
					cerr << "It's in the map!" << endl;
					current_seq_no = map_ack_to_pos[result];
				}
				win_used = i - result;
				i = filepos = result;*/
			} else if (result == filepos){
				if (map_pos_to_ack.count(result) > 0)
				cerr << result << " has ack # " << map_pos_to_ack[result] << endl;
				win_used = i - result;
				i = filepos;
				if (map_pos_to_ack.count(result) > 0){
					current_seq_no = map_pos_to_ack[result];
				}
			} else {//problem: we can't automatically assume that we should start the next sequence number from the same place...
				cerr << filepos << " " << result << endl;
				filepos = result;
				win_used = i - result;
			}
		}
		else break;
	}	
	delete[] file_block;
	//cerr << "we got here" << endl;
} 

int main(int argc, char** argv){
	//default hostname: localhost. default port: 4000. default directory: . (current working directory)
	expire_time.tv_sec = 0;
	expire_time.tv_usec = TIMEOUT_MS * 1000;
	int port_num = 80; //to convert to a getaddrinfo, use std::string port_num_s = std::to_string(port_num); char const *port_num_char = port_num_s.c_str();
	char* endptr; //for strtoimax
	string directory = ".";
	if(argc != 1){
		port_num = strtoimax(argv[1],&endptr,10);
		filename = argv[2];
	} else {
		return -1; //error
	}
	struct addrinfo hints, *results;
	memset(&hints,0,sizeof hints);
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_DGRAM;
	hints.ai_flags=AI_PASSIVE;
	//cerr << to_string(port_num).c_str() << endl;
	int status = getaddrinfo(NULL, to_string(port_num).c_str(), &hints, &results);
    if (status) {
        fprintf(stderr, "%s\n", gai_strerror(status));
        return status;
    }
	struct addrinfo *p; int sockfd;
    for(p = results; p != NULL; p = p->ai_next) { //find a good ip to bind to
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) continue;
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        break;
    }
	if (p == NULL) {
		cerr << "Error: couldn't bind" << endl;
		return -1;
    }

	struct sockaddr_in client; //convention lets us omit struct declarations, but it is useful for reading
	memset(&client,0,sizeof client);
	socklen_t client_length = sizeof client;
	//start handshake
	serverWin = 1024;
	handshake(sockfd, p, client);
	transfer(sockfd, p, client);
	end_transmission(sockfd, p, client);
	freeaddrinfo(results);
}