////

#include "HttpRequest.h"
#include <stdio.h>
#include <iostream>

using namespace std;

int HttpRequest::decode(vector<char> line){
	vector<string> first_line;
	int r_check = 0, n_check = 0;
	int i;
	////parse first line
	string add_to_end;
	for (i = 0; i < line.size() && !(r_check & n_check); i++){
		switch (line[i]){
		case ' ':
			r_check = n_check = 0;
			first_line.push_back(add_to_end);
			add_to_end.clear();
			break;
		case '\r':
			r_check = 1;
			break;
		case '\n':
			if (!r_check){
				r_check = 0;
				fprintf(stderr, "Error: improper format\n");
				return -1;
			}
			n_check = 1;
			first_line.push_back(add_to_end); //handles last line
			add_to_end.clear();
			break;
		default:
			add_to_end += line[i];
			r_check = n_check = 0;
			break;
		}
	}
	cerr << first_line.size() << endl;
	for (auto i = first_line.begin(); i != first_line.end(); ++i)
    cerr << *i << endl;
	if (first_line.size() != 3){
		fprintf(stderr, "Error: Invalid number of first line information\n");
		return -1;
	}
	set_version(first_line[2].substr(5, 3));
	set_method(first_line[0]);
	HttpMessage::decode_header(line);
	m_url_frag = first_line[1];
	set_url(get_header("Host") + first_line[1]); //set url. If "Host" header DNE, then get_header returns "". 
	return 0;
}

vector<char> HttpRequest::encode_first_line(){
	string first_line;
	first_line += (get_method() + " ");

	if (get_header("Host") == "")
		first_line += (get_url() + " ");
	else
		first_line += (get_header("Host") + m_url_frag + " ");
	first_line += ("HTTP/" + get_version() + "\r\n");

	std::vector<char> v_first_line(first_line.begin(), first_line.end());
	
	return v_first_line;
}

vector<char> HttpRequest::encode(){
	vector<char> message = HttpRequest::encode_first_line();
	string headers;
	for (auto& x :get_map()) {
		headers += (x.first + ": " + x.second + "\r\n");
	}
	headers += "\r\n";
	for (int i = 0; i < headers.size(); i++)
		message.push_back(headers[i]);
	
	return message;

}
