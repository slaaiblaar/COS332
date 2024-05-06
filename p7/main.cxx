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
// ... (other includes as necessary)
SSL *ssl = nullptr;
int sock;
void sigint_handler(int sig)
{
    std::cout << "Caught signal " << sig << std::endl;
    std::cout << "Shutting down SSL connection" << std::endl;
    SSL_shutdown(ssl);
    std::cout << "Closing socket" << std::endl;
    close(sock);
    exit(sig);
}
void error(const char *msg)
{
    perror(msg);
    ssl == nullptr ? SSL_shutdown(ssl) : 0;
    close(sock);
    exit(1);
}
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
        printf("IPv%d address: %s (%s)\n", res->ai_family == PF_INET6 ? 6 : 4,
               addrstr, res->ai_canonname);
        // if ai family is IPV4
        if (res->ai_family != PF_INET6)
        {
            ip_address = addrstr;
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
    // printf("=====Reading message=====\n");
    ss->str("");
    int bytes;
    while (totalBytes < numOctets)
    {
        char *buffer = new char[4096];
        std::fill_n(buffer, 4096, '\0');
        bytes = SSL_read(ssl, buffer, 1023);
        buffer[bytes] = '\0';
        
        (*ss) << std::string(buffer).substr(0, bytes);
        totalBytes += bytes;
        // printf("%s", buffer);
        delete [] buffer;
    }
    // printf("Number of bytes read: %d\n", totalBytes);    
}
bool postfix_command(int postfixSock, const std::string &command, std::string expectedResponse)
{
    // printf("Sending command: %s", command.c_str());
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
    // printf("Response: %s", buffer);
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
    sa.sin_port = htons(25);  // Port number for SMTP
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
    printf("\t%s", buffer);
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
    // Step 1: Connect to the Gmail POP3 server
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

    // Step 2: SSL/TLS Handshake
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
    // Step 3: POP3 Interaction
    char buffer[4096];
    int bytes;
    std::fill_n(buffer, 1024, 0);
    bytes = SSL_read(ssl, buffer, sizeof(buffer));
    if (!check_ok(buffer, bytes)) {
        error("Error connecting");
    }
    // printf("=======buffer CONN=======\n%s\n=======NNOC reffub=======\n", buffer);
    send_command(ssl, "USER wian.koekemoer123@gmail.com\r\n");
    std::fill_n(buffer, 1024, 0);
    bytes = SSL_read(ssl, buffer, sizeof(buffer));
    if (!check_ok(buffer, bytes)) {
        error("Error sending USER command");
    }
    // printf("=======buffer USER=======\n%s\n=======reffub=======\n", buffer);
    #ifndef PASSWORD
    #define PASSWORD  "\"googleapppasswrd\""
    #endif
    send_command(ssl, std::string("PASS ") + PASSWORD + "\r\n");
    std::fill_n(buffer, 1024, 0);
    bytes = SSL_read(ssl, buffer, sizeof(buffer));
    if (!check_ok(buffer, bytes)) {
        error("Error sending PASS command");
    }
    // printf("=======buffer PASS=======\n%s\n=======reffub=======\n", buffer);
    // std::cout << "Commands sent" << std::endl;
    // Step 4: Parse Emails
    // This is a simplified version. You'll need to add code to parse the email headers and look for the BCC field.
    std::cout << "Reading emails" << std::endl;
    // send_command(ssl, "LIST\r\n");
    send_command(ssl, "STAT\r\n");
    // getting the number of emails
    int numReads = 0;
    std::fill_n(buffer, 1024, 0);
    bytes = SSL_read(ssl, buffer, sizeof(buffer));
    if (!check_ok(buffer, bytes)) {
        error("Error sending STAT command");
    }
    printf("=======STAT Response=======\n%s===========================\n", buffer);
    // std::fill_n(buffer, 1024, 0);
    // get number of emails in maildrop
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
    std::cout << "Number of emails: " << numEmails << std::endl;
    for (int i = 1; i <= numEmails; i++) {
        std::string command = "LIST " + std::to_string(i) + "\r\n";
        send_command(ssl, command);
        std::fill_n(buffer, 1024, 0);
        bytes = SSL_read(ssl, buffer, sizeof(buffer));
        if (!check_ok(buffer, bytes)) {
            break;
        }
        printf("=========Email %d==========\n", i);
        // get size of message
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
        // get first line from buffer
        std::string tempBuffer(buffer);
        std::stringstream issFirstLine(tempBuffer);
        // printf("=======buffer RETR %d=======\n%s\n=======RTER reffub=======\n", i, issFirstLine.str().c_str());
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
        std::stringstream messageCopy(message.str());
        read_message(&message, size, totalBytes);
        messageCopy << message.str();
        // messageStr += message.str();
        messageStr.append(message.str());
        // find end of head
        int headEnd = messageStr.find("\r\n\r\n");
        // std::cout << "Head end: " << headEnd << std::endl;
        std::string head = messageStr.substr(0, headEnd);
        // regex pattern for a string that matches "bcc: wian.koekemoer123@gmail.com" with any number of whitespaces in between
        std::regex bccPattern("\\bbcc:\\s+wian\\.koekemoer123@gmail\\.com\\b", std::regex_constants::icase);
        std::sregex_iterator it(head.begin(), head.end() + headEnd, bccPattern);
        std::sregex_iterator itEnd;
        if (it == itEnd) {
            continue;
        }
        int subjectStart = head.find("Subject: ");
        int subjectEnd = head.find("\r\n", subjectStart);
        std::string originalSubject = head.substr(subjectStart + 9, subjectEnd - subjectStart - 9);
        // if subject already has [BCC Alert] in it, skip
        if (originalSubject.find("[BCC Alert]") != std::string::npos) {
            continue;
        }
        std::cout << "\tFound BCC field in email " << i << std::endl;\
        // open file to write email to
        // std::ofstream emailFile("email" + std::to_string(i) + ".txt");
        // emailFile << messageStr;
        // emailFile.close();
        // find subject
        std::string subject = "[BCC Alert] " + originalSubject;
        std::cout << "\tSubject: " << subject << std::endl;
        // send email
        send_mail(subject);
        
    }
    std::cout << "Finished reading emails" << std::endl;
    
    // send_command(ssl, "RSET\r\n");
    send_command(ssl, "QUIT\r\n");
    SSL_shutdown(ssl);
    close(sock);
    return 0;
}
