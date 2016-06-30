
//
#include <string>
#include "HttpMessage.h"
#include <string>

using namespace std;
class HttpResponse : public HttpMessage{
public:
	HttpResponse(){};
	~HttpResponse(){};

	int decode(vector<char> line);

	vector<char> encode_first_line();
	vector<char> encode();


	string set_description(string m_status_description);
	string get_description();
	
	int set_status(const int status);
	int get_status();


private:
	string m_status_description;
	int m_status; //static constant int 
};

inline string HttpResponse::set_description(string status_description){
	return m_status_description = status_description;
}

inline int HttpResponse::set_status(const int status){
	return (m_status = status);
}

inline string HttpResponse::get_description(){
	return m_status_description;
}

inline int HttpResponse::get_status(){
	return m_status;
}