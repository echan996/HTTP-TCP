#include <vector>
#include <cstdint>
#ifndef TCPMESSAGE
#define TCPMESSAGE

//When constructing a TCPMessage, set all the fields and call encode. On the receiving side, 
//simply call decode to extract all relevant data parameters.

struct TCPMessage{
	uint16_t seqNo;
	uint16_t ackNo;
	uint16_t rcvWin;
	uint16_t flags;
	std::vector<uint8_t> payload;
};

std::vector<uint8_t> encode(TCPMessage header);
TCPMessage decode(std::vector<uint8_t> header);
#endif
