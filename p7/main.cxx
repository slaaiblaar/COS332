#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <netdb.h>
#include <csignal>
#include <sstream>
#include <fstream>
#include <regex>

SSL *ssl = nullptr;
int sock;
// Close the SSL connection and socket when the program is interrupted
void sigint_handler(int sig)
{
    std::cout << "Caught signal " << sig << std::endl;
    std::cout << "Shutting down SSL connection" << std::endl;
    SSL_shutdown(ssl);
    std::cout << "Closing socket" << std::endl;
    close(sock);
    exit(sig);
}
// Close the SSL connection and socket when this program throws an error
void error(const char *msg)
{
    perror(msg);
    ssl == nullptr ? SSL_shutdown(ssl) : 0;
    close(sock);
    exit(1);
}
// Look up address of a host domain
// Source: https://gist.github.com/jirihnidek/bf7a2363e480491da72301b228b35d5d
std::string lookup_host(const char *host)
{
    struct addrinfo hints, *res, *result;
    int errcode;
    char addrstr[100];
    void *ptr;
    std::string ip_address = "";

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_CANONNAME;

    errcode = getaddrinfo(host, NULL, &hints, &result);
    if (errcode != 0)
    {
        perror("getaddrinfo");
        return "";
    }

    res = result;

    printf("Host: %s\n", host);
    while (res)
    {
        inet_ntop(res->ai_family, res->ai_addr->sa_data, addrstr, 100);

        switch (res->ai_family)
        {
        case AF_INET:
            ptr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
            break;
        case AF_INET6:
            ptr = &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
            break;
        }
        inet_ntop(res->ai_family, ptr, addrstr, 100);
        // if ai family is IPV4
        if (res->ai_family != PF_INET6)
        {
            printf("IPv%d address: %s (%s)\n", res->ai_family == PF_INET6 ? 6 : 4,
                addrstr, res->ai_canonname);
            ip_address = addrstr;
            break;
        }
        res = res->ai_next;
    }

    freeaddrinfo(result);

    return ip_address;
}
// Function to send a command to the server and print the response
void send_command(SSL *ssl, const std::string &command)
{
    SSL_write(ssl, command.c_str(), command.size());
}
bool check_ok(const char *buffer, int len)
{
    if (len < 3)
    {
        return false;
    }
    return buffer[0] == '+' && buffer[1] == 'O' && buffer[2] == 'K';
}

void read_message(std::stringstream *ss, const int numOctets, int totalBytes = 0)
{
    ss->str("");
    int bytes;
    // read all data in 1023 byte chunks
    while (totalBytes < numOctets)
    {
        char *buffer = new char[1024];
        std::fill_n(buffer, 1024, '\0');
        bytes = SSL_read(ssl, buffer, 1023);
        buffer[bytes] = '\0';
        
        (*ss) << std::string(buffer).substr(0, bytes);
        totalBytes += bytes;
        delete [] buffer;
    }
}

bool postfix_command(int postfixSock, const std::string &command, std::string expectedResponse)
{
    if (write(postfixSock, command.c_str(), command.size()) < 0)
    {
        perror("write failed");
        close(postfixSock);
        return false;
    }

    // Read the response
    char buffer[1024];
    std::fill_n(buffer, 1024, 0);
    int bytes = read(postfixSock, buffer, 1023);
    if (bytes < 0)
    {
        perror("read failed");
        close(postfixSock);
        return false;
    }

    // if response code is not as expected
    if (std::string(buffer).find(expectedResponse) == std::string::npos)
    {
        close(postfixSock);
        return false;
    }

    return true;
}
void send_mail(std::string subject)
{
    // Create a socket
    int postfixSock = socket(AF_INET, SOCK_STREAM, 0);
    if (postfixSock < 0) {
        perror("socket failed");
        close(postfixSock);
        close(postfixSock);
        return;
    }

    // Specify the server address and port
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(25);
    if (inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(postfixSock);
        return;
    }

    // Connect to the server
    if (connect(postfixSock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        perror("connect failed");
        close(postfixSock);
        return;
    }

    // Use the socket to send an email
    char buffer[1024];
    int bytes;
    std::fill_n(buffer, 1024, 0);
    bytes = read(postfixSock, buffer, sizeof(buffer));
    if (bytes < 0) {
        perror("read failed");
        close(postfixSock);
        return;
    }

    if (std::string(buffer).find("220") == std::string::npos) {
        close(postfixSock);
        return;
    }

    // Send the HELO command
    if(!postfix_command(postfixSock, "HELO localhost\r\n", "250")) {
        return;
    }

    // Send the MAIL FROM command
    if (!postfix_command(postfixSock, "MAIL FROM: <wian.koekemoer123@gmail.com>\r\n", "250")) {
        return;
    }

    // Send the RCPT TO command
    if (!postfix_command(postfixSock, "RCPT TO: <wian.koekemoer123@gmail.com>\r\n", "250")) {
        return;
    }

    // Send the DATA command
    if (!postfix_command(postfixSock, "DATA\r\n", "354")) {
        return;
    }

    // Send the email headers and body
    if (!postfix_command(postfixSock, "Subject: " + subject + "\r\n\r\nYou have been BCCd\r\n.\r\n", "250")) {
        return;
    }

    // Send the QUIT command
    if (!postfix_command(postfixSock, "QUIT\r\n", "221")) {
        return;
    }

    close(postfixSock);
}

int main()
{
    signal(SIGINT, sigint_handler);
    std::string host = "pop.gmail.com";
    // Connect to the Gmail POP3 server
    std::cout << "Starting program" << std::endl;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    std::cout << "Socket created: " << sock << std::endl;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_addr.s_addr = inet_addr(lookup_host(host.c_str()).c_str());
    sa.sin_family = AF_INET;
    sa.sin_port = htons(995);
    inet_pton(AF_INET, "pop.gmail.com", &sa.sin_addr);
    if (connect(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }
    std::cout << "Connected to server" << std::endl;

    // SSL/TLS Handshake
    int sslinit = SSL_library_init();
    std::cout << "SSL library initialized: " << sslinit << std::endl;
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
    std::cout << "SSL context created" << std::endl;
    ssl = SSL_new(ctx);
    std::cout << "SSL created" << std::endl;
    int fd = SSL_set_fd(ssl, sock);
    std::cout << "SSL fd: " << fd << std::endl;
    try {
        int sslcon = SSL_connect(ssl);
        std::cout << "SSL connected " << sslcon << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "SSL_connect failed: " << e.what() << std::endl;
        return 1;
    }
    // POP3 Interaction
    char buffer[1024];
    int bytes;
    std::fill_n(buffer, 1024, 0);
    bytes = SSL_read(ssl, buffer, sizeof(buffer));
    if (!check_ok(buffer, bytes)) {
        error("Error connecting");
    }

    send_command(ssl, "USER wian.koekemoer123@gmail.com\r\n");
    std::fill_n(buffer, 1024, 0);
    bytes = SSL_read(ssl, buffer, sizeof(buffer));
    if (!check_ok(buffer, bytes)) {
        error("Error sending USER command");
    }
    // totally legit password, set with -DPASSWORD="\"yourpassword\""
    #ifndef PASSWORD
    #define PASSWORD  "\"googleapppasswrd\""
    #endif
    send_command(ssl, std::string("PASS ") + PASSWORD + "\r\n");
    std::fill_n(buffer, 1024, 0);
    bytes = SSL_read(ssl, buffer, sizeof(buffer));
    if (!check_ok(buffer, bytes)) {
        error("Error sending PASS command");
    }

    // Read Emails
    std::cout << "Reading emails" << std::endl;
    send_command(ssl, "STAT\r\n");
    int numReads = 0;
    std::fill_n(buffer, 1024, 0);
    bytes = SSL_read(ssl, buffer, sizeof(buffer));
    if (!check_ok(buffer, bytes)) {
        error("Error sending STAT command");
    }
    printf("STAT Response: %s", buffer);
    // parse number of emails in maildrop
    std::string stat(buffer);
    std::istringstream iss(stat);
    std::string word;
    int numEmails = 0;
    while (iss >> word) {
        if (numReads == 1) {
            numEmails = std::stoi(word);
            break;
        }
        numReads++;
    }
    std::cout << "Number of new emails: " << numEmails << std::endl;
    for (int i = 1; i <= numEmails; i++) {
        // send LIST command
        std::string command = "LIST " + std::to_string(i) + "\r\n";
        send_command(ssl, command);
        std::fill_n(buffer, 1024, 0);
        bytes = SSL_read(ssl, buffer, sizeof(buffer));
        if (!check_ok(buffer, bytes)) {
            break;
        }
        printf("=========Email %d==========\n", i);
        // parse size of message
        iss.str(std::string(buffer));
        numReads = 0;
        int size = 0;
        while (iss >> word) {
            if (numReads == 2) {
                size = std::stoi(word);
                break;
            }
            numReads++;
        }
        std::cout << "\tNumber of octets: " << size << std::endl;
        command = "RETR " + std::to_string(i) + "\r\n";
        send_command(ssl, command);
        std::fill_n(buffer, 1024, 0);
        bytes = SSL_read(ssl, buffer, sizeof(buffer));
        if (!check_ok(buffer, bytes)) {
            break;
        }
        // get first line from buffer and exclude it from message
        std::string tempBuffer(buffer);
        std::stringstream issFirstLine(tempBuffer);
        int totalBytes = bytes;
        std::string temp;
        issFirstLine >> temp; // "+OK"
        totalBytes -= temp.length() + 1; // sp
        issFirstLine >> temp; // "message"
        totalBytes -= temp.length() + 1; // sp
        issFirstLine >> temp; // "follows"
        totalBytes -= temp.length() + 2; // cr lf

        // copy remaining data to message
        std::stringstream message(std::string(buffer + bytes - totalBytes));
        std::fill_n(buffer, 1024, 0);
        std::string messageStr = message.str();
        read_message(&message, size, totalBytes);
        messageStr.append(message.str());

        // find end of head
        int headEnd = messageStr.find("\r\n\r\n");
        std::string head = messageStr.substr(0, headEnd);

        // regex pattern for a string that matches "bcc: wian.koekemoer123@gmail.com"
        std::regex bccPattern("\\bbcc:\\s+wian\\.koekemoer123@gmail\\.com\\b", std::regex_constants::icase);
        std::sregex_iterator it(head.begin(), head.end() + headEnd, bccPattern);
        std::sregex_iterator itEnd;
        if (it == itEnd) {
            continue;
        }
        // find subject
        int subjectStart = head.find("Subject: ");
        int subjectEnd = head.find("\r\n", subjectStart);
        std::string originalSubject = head.substr(subjectStart + 9, subjectEnd - subjectStart - 9);
        // if subject already has [BCC Alert] in it, skip
        // notification emails sent by this program have the recipient as a BCC for some reason
        if (originalSubject.find("[BCC Alert]") != std::string::npos) {
            continue;
        }
        std::cout << "\tFound BCC field in email " << i << std::endl;
        std::string subject = "[BCC Alert] " + originalSubject;
        std::cout << "\tSubject: " << subject << std::endl;
        // send email
        send_mail(subject);
        
    }
    std::cout << "Finished reading emails" << std::endl;
    
    send_command(ssl, "QUIT\r\n");
    SSL_shutdown(ssl);
    close(sock);
    return 0;
}
