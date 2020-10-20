/* A simple server in the internet domain using TCP
 The port number is passed as an argument
 This version runs forever, forking off a separate
 process for each connection
 */
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>  /* signal name macros, and the kill() prototype */
#include <errno.h>
#include <string>
#include <string.h>
#include <vector>
#include "http_parser.h"
#include <string>
#include <map>
#include <fcntl.h>  // open
#include <sys/stat.h>  // fstat
#include <sys/types.h>
#include <time.h>  // time


#define PORTNO 8024
#define BUFFER_SIZE 4096
#define IS_BAD_REQUEST 3
#define DEBUG 1
//request names

using namespace std;
//commonly used response message
string http_version = "HTTP/1.1";
string bad_request = " 400 Bad Request";
string not_found = " 404 Not Found";
string ok = " 200 OK";
string connection = "Connection: keep-alive\r\n";
//string connection = "Connection: close\r\n";
string server = "Server: Simple Webserver\r\n";

//note: case insensitive
const map<string, string> file_type = {
    {".htm", "text/html"},
    {".html", "text/html"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {"default", "application/octet-stream"}
};

//adopt the code from http://bits.mdminhazulhaque.io/cpp/find-and-replace-all-occurrences-in-cpp-string.html
void find_and_replace(string& source, string const& find, string const& replace){
    for(string::size_type i = 0; (i = source.find(find, i)) != string::npos;){
        source.replace(i, find.length(), replace);
        i += replace.length();
    }
}

void error(string msg)
{
    //perror(msg);
    cout << msg;
    exit(1);
}

void debug_print(string msg){
    if(DEBUG){
        cout << msg << endl;
    }
}


//status
string http_respond(string s, int status, int newsockfd, string path = ""){
    debug_print("Path is " + path);
    char time_buffer[30] = {0};
    time_t t;
    time(&t);
    strftime(time_buffer, 30, "%a, %d %b %Y %H:%M:%S %Z", localtime(&t));
    string date = string(time_buffer);
    date = "Date: "+ date+  "\r\n";
    
    string last_modified = "Last-Modified: ";
    string content_length = "Content-Length: ";
    string content_type = "Content-Type: ";
    if(path == "./"){
        path = "./404.html"; //NOTE: auto transfer to 404.html
    }
    if(status == 404){
        path = "./404.html"; //display 404 not found page
        status = 200;
    }
    //get last modified time
    if(status == 200){
        struct stat file_info;
        int f = open(path.c_str(), O_RDONLY);
        if (fstat(f, &file_info) < 0) {
            status = 404;
            s = http_version + not_found;
        }
            strftime(time_buffer, 30, "%a, %d %b %Y %H:%M:%S %Z", localtime(&file_info.st_mtime));
            last_modified = last_modified + string(time_buffer) + "\r\n";
            content_length = content_length + to_string(file_info.st_size) + "\r\n";
            //find .
            std::size_t dot = path.rfind(".");
            if (dot!=std::string::npos){
                string extension = path.substr(dot, path.length()-1);
                for(int i = 0; extension[i] != '\0'; i++){
                    extension[i] = tolower(extension[i]);
                }
                debug_print("File extension is " + extension);
                if(file_type.find(extension) != file_type.end()){
                    content_type = content_type + file_type.at(extension) + "\r\n";
                }else{
                    content_type = content_type + file_type.at("default") + "\r\n";
                }//end else content type
            }//end find . in extension
            string response = s + "\r\n" + connection + date + server+ last_modified+content_length + content_type + "\r\n";
            debug_print("Response message:\n" +response);
            write(newsockfd, response.c_str(), response.length());
            /*
             ifstream file(path);
             
             
             */
            FILE *file;
            file = fopen(path.c_str(), "rb");
            fseek(file, 0, SEEK_END);
            int fsize = ftell(file);
            fseek(file, 0, SEEK_SET);
            
            debug_print("File size :" + to_string(fsize) + "\n");
            char* buffer = new char[fsize];
            fread (buffer, fsize, 1, file);
            int ret = write(newsockfd, buffer, fsize);
            cout << "total data transfered: " << ret << endl;
            cout << endl;
            //fclose(file);
            //debug_print("File Buffer dump: " + string(buffer));
            return "";
        }//end 200 with file info found
    string response = s + "\r\n" + connection + date + server +"\r\n";
    debug_print("Response message is \n"+ response);
    return response;
}

//check for http request format
string process_request(map<string, string> request,int newsockfd){
    
    if(request.find("HTTP-Version") == request.end()){
        cout << "Cannot parse HTTP Version";
        return http_respond(http_version + bad_request, 400, newsockfd);
    }else{
        http_version = request.at("HTTP-Version");
    }
    
    if(request.find("Method") == request.end()){
        cout << "Cannot parse method";
        return http_respond(http_version + bad_request, 400, newsockfd);
    }else{
        if(request.at("Method") == "GET"){
            string path = "." + request.at("Request-URL");
            //handling space
            find_and_replace(path, "%20", " ");
            std::ifstream infile(path);
            if(infile.good()){
                infile.close();
                return http_respond(http_version+ok, 200, newsockfd, path);
            }else{
                return http_respond(http_version+not_found, 404,newsockfd, path);
            }
        }else{
            return http_respond(http_version + bad_request, 400, newsockfd);
        }//end else GET
    }//end
}//end process request


int main(int argc, char *argv[])
{
    int sockfd, portno;
    
    struct sockaddr_in serv_addr, cli_addr;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);  // create socket
    if (sockfd < 0)
        error("ERROR opening socket");
    memset((char *) &serv_addr, 0, sizeof(serv_addr));   // reset memory
    
    // fill in address info
    if(argc == 2){
        portno = stoi(argv[1]);
    }else{
        portno = PORTNO;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    if (::bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    
    cout << "server listening..." << endl << endl;
    listen(sockfd, 5);  // 5 simultaneous connection at most
    while(true){
        socklen_t clilen;
        int newsockfd;
        //accept connections
        //cout << "now accepting" << endl;
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        
        if (newsockfd < 0){
            error("ERROR on accept");
            cout << strerror(errno) <<"w";
        }
        
        int n;
        string request;
        while(request.length() < 4 || request.substr(request.length() - 4)!= "\r\n\r\n")
        {
            char buffer[BUFFER_SIZE];
            memset(buffer, 0, BUFFER_SIZE);  // reset memory
            n = read(newsockfd, buffer, BUFFER_SIZE);
            if (n < 0) error("ERROR reading from socket");
            request += buffer;
            cout << buffer;
        }
        //cout << request;
        map<string, string> r = parse_request(request);
        string ret = process_request(r, newsockfd);
        //reply to client
        if(ret != ""){
            n = write(newsockfd, ret.c_str() , ret.length());
        }
        if (n < 0) error("ERROR writing to socket");
        close(newsockfd); // close connection
        //cout << "newsockfd closed" << endl;
        
    }
    close(sockfd);
    
    return 0;
}




