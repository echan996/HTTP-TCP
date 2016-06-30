#include <stdlib.h>
#include <string>
#include <map>
#include <vector>
#include <map>
//#include <boost/algorithm/string.hpp>

//typedef HttpVersion std::string;
#ifndef HTTPMESSAGE
#define HTTPMESSAGE
class HttpMessage{
	
	public:
	HttpMessage(){};
	~HttpMessage(){};

	virtual int decode(std::vector<char> line) = 0; //maybe change name later
	void decode_header(std::vector<char> line);

	virtual std::vector<char> encode_first_line() = 0;
	virtual std::vector<char> encode() = 0;
	
	void set_version(std::string v);
	std::string get_version(); //inlined

	void set_header(std::string key, std::string value); //inlined
	std::string get_header(std::string key); //inlined

	void set_payload(std::vector<char> blob){
		m_payload = blob;
	}; //inlined
	std::vector<char> get_payload(); //inlined

	

	std::map<std::string, std::string> get_map(){
		return m_header;
	};
	
	
	private:
	std::string m_version = "1.0";
	std::map<std::string,std::string> m_header;
	std::vector<char> m_payload;
};
#endif