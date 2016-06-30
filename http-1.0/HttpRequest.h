
//
#include <string>
#include "HttpMessage.h"
using namespace std;
class HttpRequest : public HttpMessage{
public:

	HttpRequest() :HttpMessage(){};
	~HttpRequest(){};

	void set_url(string url);
	void set_method(string method);

	string get_url();
	string get_method();

	int decode(vector<char> line);
	
	vector<char> encode_first_line();
	vector<char> encode();

	private:

	string m_url;
	string m_url_frag;
	string m_method; 
};

inline void HttpRequest::set_url(string url){
	m_url = url;
}

inline void HttpRequest::set_method(string method){
	m_method = method;
}

inline string HttpRequest::get_url(){
	return m_url;
}

inline string HttpRequest::get_method(){
	return m_method;
}