#include "TCPMessage.h"

std::vector<uint8_t> encode(TCPMessage header){
	std::vector<uint8_t> message;
	uint8_t value;
	
	//set seqNo
	value=(header.seqNo >> 8) & 0xff;
	message.push_back(value);
	value=(header.seqNo & 0xff);
	message.push_back(value);
	
	//set ackNo
	value=(header.ackNo >> 8) & 0xff;
	message.push_back(value);
	value=(header.ackNo & 0xff);
	message.push_back(value);
	
	//set window
	value=(header.rcvWin >> 8) & 0xff;
	message.push_back(value);
	value=(header.rcvWin & 0xff);
	message.push_back(value);
	
	//set flags
	value = (header.flags >> 8) & 0xff;
	message.push_back(value);
	value=(header.flags & 0xff);
	message.push_back(value);
	
	//set payload
	for(int i=0;i<header.payload.size();i++) message.push_back(header.payload[i]);	
	
	return message;
}

TCPMessage decode(std::vector<uint8_t> header){
	TCPMessage message;
	uint16_t value;
	
	value = (header[0] << 8)|(header[1]);
	message.seqNo = value;
	value = (header[2] << 8)|(header[3]);
	message.ackNo = value;
	value = (header[4] << 8)|(header[5]);
	message.rcvWin = value;
	value = (header[6] << 8)|(header[7]);
	message.flags=value;
	for(int i=8;i<header.size();i++) message.payload.push_back(header[i]);
	return message;
}
/*
int main(){
	TCPMessage message;
	message.seqNo = 1;
	message.ackNo=2;
	message.rcvWin=3;
	message.flags=0;
	message.payload.push_back(1);
	std::vector<uint8_t> vec = encode(message);
	TCPMessage newMes=decode(vec);
	assert(message.seqNo==newMes.seqNo);
	assert(message.ackNo==newMes.ackNo);
	assert(message.rcvWin==newMes.rcvWin);
	assert(message.flags==newMes.flags);

}
*/