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
// ... (other includes as necessary)
void error(const char *msg)
{
    perror(msg);
    exit(1);
}
SSL *ssl;
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
    // printf("%s\n", command.c_str());
    SSL_write(ssl, command.c_str(), command.size());
    char buffer[1024];
    SSL_read(ssl, buffer, sizeof(buffer));
    printf("%s\n", buffer);
}

int main()
{
    signal(SIGINT, sigint_handler);
    std::string host = "pop.gmail.com";
    // Step 1: Connect to the Gmail POP3 server
    std::cout << "Starting program" << std::endl;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    std::cout << "Socket created" << std::endl;
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
    std::cout << "SSL library initialized " << sslinit << std::endl;
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
    std::cout << "SSL context created" << std::endl;
    ssl = SSL_new(ctx);
    std::cout << "SSL created" << std::endl;
    int fd = SSL_set_fd(ssl, sock);
    std::cout << "SSL set fd " << fd << std::endl;
    try {
        int sslcon = SSL_connect(ssl);
        std::cout << "SSL connected " << sslcon << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "SSL_connect failed: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "SSL connected" << std::endl;
    // Step 3: POP3 Interaction
    send_command(ssl, "USER wian.koekemoer123@gmail.com\r\n");
    // send_command(ssl, "PASS ydavvvmekrorlvon\r\n");
    send_command(ssl, "PASS yfcelarqfqjwytli\r\n");
    // std::cout << "Commands sent" << std::endl;
    // Step 4: Parse Emails
    // This is a simplified version. You'll need to add code to parse the email headers and look for the BCC field.
    std::cout << "Reading emails" << std::endl;
    // send_command(ssl, "LIST\r\n");
    char buffer[1024];
    // getting the number of emails
    int bytes;
    int numReads = 0;
    while ((bytes = SSL_read(ssl, buffer, sizeof(buffer))) > 0) {
        ++numReads;
        // if (bytes < 0) {
        //     std::cerr << "Error reading from socket" << std::endl;
        //     return 1;
        // }
        std::cout << numReads << " | Bytes read: " << bytes << std::endl;
        int numEmails = 0;
        std::string emailList(buffer);
    }
    std::cout << "Finished reading emails" << std::endl;

    // Step 5: Send Warning Email
    // You'll need to add code to send an email through your postfix server if a BCC field is found.


    // step 6: Quit and close connection
    send_command(ssl, "QUIT\r\n");
    SSL_shutdown(ssl);
    close(sock);
    return 0;
}
