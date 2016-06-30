#include "HttpResponse.h"
using namespace std;

int HttpResponse::decode(vector<char> line){
	vector<string> first_line;
	int r_check = 0, n_check = 0;
	string add_to_end;
	for (int i = 0; i < line.size() && !(r_check & n_check); i++){
		switch (line[i]){
		case ' ':
			first_line.push_back(add_to_end);
			add_to_end.clear();
			r_check = n_check = 0;
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



	//Error Checking
	for (int i = 3; i < first_line.size(); i++){
		first_line[2] += first_line[i];
	}
	if (first_line.size() < 3){
		fprintf(stderr, "Error: Missing arguments.\n");
		return -1;
	}
	if (first_line[0].substr(0, 4) != "HTTP"){
		fprintf(stderr, "Error: Improper format of HTTP field.\n");
		return -1;
	}
	if (first_line[1].size() != 3){
		fprintf(stderr, "Error: Improper exit status size.\n");
		return -1;
	}
	for (int i = 0; i < first_line[1].size(); i++)
		if (!isdigit(first_line[1][i])){
			fprintf(stderr, "Error: Improper exit status format.\n");
			return -1;
		}
	//End Error Checking



	//Set up data fields
	set_status(atoi(first_line[1].c_str()));
	set_version(first_line[0].substr(5, 3));
	
	switch (get_status()){
	case 200:
		if (set_description(first_line[2]) != "OK"){
			fprintf(stderr, "Error: Improper exit status description\n");
			return -1;
		}
		break;
	case 400:
		if (set_description(first_line[2]) != "Bad request"){
			fprintf(stderr, "Error: Improper exit status description\n");
			return -1;
		}
		break;
	case 404:
		if (set_description(first_line[2]) != "Not found"){
			fprintf(stderr, "Error: Improper exit status description\n");
			return -1;
		}
		break;
	}
	HttpMessage::decode_header(line);

	//set up payload
	int iterator=0, pos;
	for (pos = 0; pos < line.size() && iterator != 4; pos++){
		switch (iterator% 2){
		case 0:
			iterator += ((line[pos] == '\r') ? 1 : -iterator);
			break;
		case 1:
			iterator += ((line[pos] == '\n') ? 1 : -iterator);
			break;
		}
	}
	vector<char> payload;
	for (pos = pos + 1; pos < line.size(); pos++){
		payload.push_back(line[pos]);
	}
	set_payload(payload);
	/*string m_content_length;
	if ((m_content_length=get_header("Content-Length")) == ""){
		fprintf(stderr, "Error: Missing content length header field.\n");
		return -1;
	}

	int content_length = atoi(m_content_length.c_str());
	
	//error checking payload
	int payload_init_pos = line.size() - content_length;
	if (payload_init_pos < 4){
		fprintf(stderr, "Error: Incomplete message.\n");
		return -1;
	}
	if (line[payload_init_pos - 1] != '\n' || line[payload_init_pos - 2] != '\r' || line[payload_init_pos - 3] != '\n' || line[payload_init_pos - 4] != '\r'){
		fprintf(stderr, "Error: Incomplete carriage returns after headers.\n");
		return -1;
	}
	vector<char> payload;
	payload.resize(content_length);
	for (int i = 0; i < content_length; i++){
		payload.push_back(line[i + payload_init_pos]);
	}
	set_payload(payload);*/
	//End set up data fields
	
	return 0;
}

vector<char> HttpResponse::encode_first_line(){
	string first_line;
	vector<char> v_first_line;
	
	first_line += ("HTTP/" + get_version() + ' ' + to_string(get_status())+ ' ' + get_description() + "\r\n");

	for (int i = 0; i < first_line.size(); i++)
		v_first_line.push_back(first_line[i]);

	return v_first_line;
}

vector<char> HttpResponse::encode(){
	string headers;
	vector<char> message=encode_first_line();
	for (auto& x : get_map()) {
		headers += (x.first + ": " + x.second + "\r\n");
	}
	headers += "\r\n";
	for (int i = 0; i < headers.size(); i++)
		message.push_back(headers[i]);
	vector<char> payload = get_payload();
	message.resize(message.size() + payload.size());
	for (int i = 0; i < payload.size(); i++){
		message.push_back(payload[i]);
	}
	return message;
}