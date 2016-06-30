//HttpMessage.cpp
#include "HttpMessage.h"
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <string>
using namespace std;
std::string HttpMessage::get_version(){
	return m_version;
}
void HttpMessage::set_version(std::string v){
	m_version = v;
}
void HttpMessage::set_header(std::string key, std::string value){
	m_header.insert(std::pair<std::string, std::string>(key, value)); //does this work?
}


std::string HttpMessage::get_header(std::string key){
	auto it = m_header.find(key);
	if (it != m_header.end()) return it->second;
	else return ""; //does this work?
}


std::vector<char> HttpMessage::get_payload(){
	return m_payload;
}

void HttpMessage::decode_header(std::vector<char> line){
	string header_body;
	for (int i=0; i < line.size(); i++)
		header_body += line[i];
	istringstream prse(header_body);
	string header;
	string::size_type index;

	while (getline(prse, header) && header != "\r"){
		index = header.find(':', 0);
		if (index != -1) //not end of string so no out of bounds
			set_header(boost::algorithm::trim_copy(header.substr(0, index)), boost::algorithm::trim_copy(header.substr(index + 1)));
	}
}