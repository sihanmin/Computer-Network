//
//  request_parser.hpp
//  proj1
//
//  Created by Mint MSH on 1/21/18.
//  Copyright © 2018 Mint MSH. All rights reserved.
//

#ifndef request_parser_hpp
#define request_parser_hpp

#include <stdio.h>
#include <string>
#include <iostream>
#include <vector>
#include <map>
using namespace std;

int check_char(char a);
string cut_string(const string& http_request, int& i, char sp);
map<string, string> parse_request(const string& http_request);

#endif /* request_parser_hpp */


