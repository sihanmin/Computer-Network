#include "http_parser.h"
using namespace std;


int check_char(char a)
{
    if (a == ' ' || a == '\n' || a == '\r' || a == ':')
        return 1;
    else
        return 0;
}

string cut_string(const string& http_request, int& i, char sp)
{
    string temp = "";
    for (; i < http_request.size() && http_request[i] != sp; i++)
    {
        temp += http_request[i];
    }
    
    while (i < http_request.size() && check_char(http_request[i]) != 0)
        i++;
    return temp;
}

map<string, string> parse_request(const string& http_request)
{
    map<string, string> result;
    string temp = "";
    string temp_value = "";
    int i = 0, j = 0;
    
    temp.assign(cut_string(http_request, i, ' '));
    result["Method"] = temp;
    temp.assign(cut_string(http_request, i, ' '));
    result["Request-URL"] = temp;
    temp.assign(cut_string(http_request, i, '\r'));
    result["HTTP-Version"] = temp;
    
    while(i < http_request.size())
    {
        temp.assign(cut_string(http_request, i, ':'));
        temp_value.assign(cut_string(http_request, i, '\r'));
        result[temp] = temp_value;
        
    }
    
    return result;
}


