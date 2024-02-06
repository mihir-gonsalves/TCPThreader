#include "TCPRequestChannel.h"

using namespace std;


TCPRequestChannel::TCPRequestChannel (const std::string _ip_address, const std::string _port_no) {
    // if server
    if (_ip_address == "") {
        // guide 2:
        // set up variables
        struct sockaddr_in server;
        int server_sock, bind_stat, listen_stat;

        // socket - make socket - socket(int domain, int type, int protocol)
        // AF_INET - IPv4
        // SOCK_STREAM - TCP
        // Normally only a single protocol exists to support a particular socket type 
        // within a given protocol family, in which case protocol can be specified as 0.
        server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock < 0) {
            perror("Socket creation failed");
            exit(1);
        }
        
        // provide nesecarry machine info for sockaddr_in
        this->sockfd = server_sock;
        memset(&server, 0, sizeof(server));
        // address family, IPv4
        // IPv4 address, use current IPv4 address (INADDR_ANY)
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        // connection port
        // convert short from host byte order to network byte order
        server.sin_port = htons(atoi(_port_no.c_str()));

        // bind - assign address to socket - bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
        bind_stat = bind(server_sock, (struct sockaddr *) &server, sizeof(server));
        if (bind_stat < 0) {
            perror("Binding failed");
            exit(1);
        }

        // listen - listen  for client - listen(int sockfd, int backlog)
        listen_stat = listen(server_sock, 1024);
        if (listen_stat < 0) {
            perror("Listening failed");
            exit(1);
        }

        // accept - accept connection // from client - accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
        // written in a separate method
    }
    
    // if client
    else {
        // set up variables
        struct sockaddr_in server_info;
        int client_sock, connect_stat;

        // socket - make socket - socket(int domain, int type, int protocol)
        client_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (client_sock < 0) {
            perror("Socket creation failed");
            exit(1);
        }

        // generate server's info based on parameters
        this->sockfd = client_sock;
        // address family, IPv4
        // connection port
        memset(&server_info, 0, sizeof(server_info));
        server_info.sin_family = AF_INET;
        // convert short from host byte order to network byte order
        server_info.sin_port = htons((short)atoi(_port_no.c_str()));
        // convert IP address c-string to binary representation for sin_addr
        inet_aton(_ip_address.c_str(), &server_info.sin_addr);

        // connect - connect to server - connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
        connect_stat = connect(client_sock, (struct sockaddr *) &server_info, sizeof(server_info));
        if (connect_stat < 0) {
            perror("Connection failed");
            exit(1);
        }
    }
}

TCPRequestChannel::TCPRequestChannel (int _sockfd) {
    // assign an existing socket to object's socket file descriptor
    this->sockfd = _sockfd;
}

TCPRequestChannel::~TCPRequestChannel () {
    // close socket - close(this->sockfd)
    close(this->sockfd);
}

int TCPRequestChannel::accept_conn () {
    struct sockaddr_in client_address;
    socklen_t cli_addr_length = sizeof(client_address);
    // accept - accept connection
    // socket file descriptor for accepted connection
    // accept connection - accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
    int newsockfd = accept(sockfd, (struct sockaddr *) &client_address, &cli_addr_length);
    // return socket file descriptor
    return newsockfd;
}

// read/write or recv/send
int TCPRequestChannel::cread (void* msgbuf, int msgsize) {
    ssize_t no_bytes = 0; // number of bytes to read
    // read from socket - read(int sockfd, void *buf, size_t count)
    no_bytes = read(sockfd, msgbuf, msgsize);
    // return number of bytes read
    return no_bytes;
}

int TCPRequestChannel::cwrite (void* msgbuf, int msgsize) {
    ssize_t no_bytes = 0; // number of bytes to write
    // write to socket - write(int sockfd, const void *buf, size_t count)
    no_bytes = write(sockfd, msgbuf, msgsize); 
    // return number of bytes written
    return no_bytes;
}