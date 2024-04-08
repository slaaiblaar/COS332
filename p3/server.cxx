#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <thread>
#include <arpa/inet.h>
#include <csignal>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <time.h>
#include <sstream>
// #include <system_error>
void error(const char *msg)
{
    perror(msg);
    exit(1);
}
int socketFD;
bool terminate = false;
std::thread *threads[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
sockaddr_in *addresses[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
int fileDescriptors[] = {-1, -1, -1, -1, -1};
bool executing[5];
// lets the process kill itself gracefully
void sigint_handler(int signal)
{
    // if (signal != SIGUSR1 && signal != SIGINT)
    // {
    // }
    // std::cout << "Signal received: " << signal << std::endl;
    terminate = true;
    for (int i = 0; i < 5; ++i)
    {
        // close(addresses[i]);
        if (threads[i] != nullptr)
        {
            try
            {
                std::cout << "Joining thread " << i << std::endl;
                threads[i]->join();
            }
            catch (...)
            {
            }
            try
            {
                std::cout << "Deleting thread " << i << std::endl;
                delete threads[i];
            }
            catch (...)
            {
            }
            threads[i] = nullptr;
        }
        // shouldn't happen, but w/e I guess
        if (addresses[i] != nullptr)
        {
            try
            {
                std::cout << "Deleting address " << i << std::endl;
                delete addresses[i];
            }
            catch (...)
            {
            }
            addresses[i] = nullptr;
        }
        // shouldn't happen, but w/e I guess
        if (fileDescriptors[i] != -1)
        {
            try
            {
                std::cout << "Closing FD " << i << ": " << fileDescriptors[i] << std::endl;
                close(fileDescriptors[i]);
            }
            catch (...)
            {
            }
            fileDescriptors[i] = -1;
        }
    }
    std::cout << "Closing main FD " << std::endl;
    int mainCloseStatus = close(socketFD);

    std::cout << "Closed with response " << mainCloseStatus << std::endl;
    exit(signal);
}

int readInput(int fd, char *input, int len)
{
    int index = 0;
    int bytesRead;
    int totalRead = 0;
    do
    {
        bytesRead = read(fd, input + index, 1);
        if (bytesRead < 0)
            return bytesRead;
        totalRead += bytesRead;
        // std::cout << index << ": " << (input[index] + 0) << " ";
        write(fd, input + index, bytesRead);
        if (index > 1 && input[index - 1] == 13 /*CR*/ && input[index] == 10)
            break;
        ++index;
    } while (index < len);

    // std::cout << (input[index] + 0) << std::endl;
    return totalRead;
}
bool decode(char c)
{
    bool prevCR = false;
    switch (c)
    {
    case 32:
        std::cout << "[SP]";
        break;
    case 0:
        std::cout << "[NULL]";
        break;
    case 1:
        std::cout << "[SOH]";
        break;
    case 2:
        std::cout << "[STX]";
        break;
    case 3:
        std::cout << "[ETX]";
        break;
    case 4:
        std::cout << "[EOT]";
        break;
    case 5:
        std::cout << "[ENQ]";
        break;
    case 6:
        std::cout << "[ACK]";
        break;
    case 7:
        std::cout << "[BEL]";
        break;
    case 8:
        std::cout << "[BS]";
        break;
    case 9:
        std::cout << "[HT]";
        break;
    case 10:
        std::cout << "[LF]";
        break;
    case 11:
        std::cout << "[VT]";
        break;
    case 12:
        std::cout << "[FF]";
        break;
    case 13:
        prevCR = true;
        break;
    case 14:
        std::cout << "[SO]";
        break;
    case 15:
        std::cout << "[SI]";
        break;
    case 16:
        std::cout << "[DLE]";
        break;
    case 17:
        std::cout << "[DC1]";
        break;
    case 18:
        std::cout << "[DC2]";
        break;
    case 19:
        std::cout << "[DC3]";
        break;
    case 20:
        std::cout << "[DC4]";
        break;
    case 21:
        std::cout << "[NAK]";
        break;
    case 22:
        std::cout << "[SYN]";
        break;
    case 23:
        std::cout << "[ETB]";
        break;
    case 24:
        std::cout << "[CAN]";
        break;
    case 25:
        std::cout << "[EM]";
        break;
    case 26:
        std::cout << "[SUB]";
        break;
    case 27:
        std::cout << "[ESC]";
        break;
    case 28:
        std::cout << "[FS]";
        break;
    case 29:
        std::cout << "[GS]";
        break;
    case 30:
        std::cout << "[RS]";
        break;
    case 31:
        std::cout << "[US]";
        break;
    default:
        std::cout << c;
        break;
    }
    return prevCR;
}
void connectionHandler(int acceptedFD, sockaddr_in *acceptedAddress, bool *terminate, int threadNum, bool *executing)
{
    srand(time(NULL));
    char strAddr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(acceptedAddress->sin_addr), strAddr, INET_ADDRSTRLEN);
    // std::cout << strAddr << ":" << acceptedAddress->sin_port << " connected\n";
    char input[256];
    bzero(input, 256);
    int readState, writeState;
    bool prevCR = false;
    bool doubleCRLF[2] = {false, false};
    std::string method;
    std::string path;
    std::string protocolVersion;
    std::string host;
    std::stringstream request;
    int responseCode = 200;
    std::string param("");
    std::string res;
    const std::string CRLF = "" + ((char)13) + ((char)10);
    while (!*terminate)
    {
        res = "HTTP/1.1 ";
        param = "";
        request.str("");
        // reading request
        while (!*terminate)
        {
            bzero(input, 256);
            readState = read(acceptedFD, input, 255);
            if (readState <= 0)
            {
                break;
            }
            // append to request
            request << input;
            // std::cout << request.str() << std::endl;
            std::cout << "[" << threadNum << "][" << strAddr << ":" << acceptedAddress->sin_port << "][" << readState << "]: ";
            for (int index = 0; index < readState; ++index)
            {
                // if potential CRLF
                if (prevCR)
                {
                    prevCR = false;
                    if (input[index] == 10)
                    {
                        doubleCRLF[doubleCRLF[0]] = true;
                        std::cout << "[CRLF" << doubleCRLF[0] << "," << doubleCRLF[1] << "]\n";
                        // if end of request headers
                        if (doubleCRLF[1])
                        {
                            std::cout << "Double CRLF\n";
                            break;
                        }
                        continue;
                    }
                    std::cout << "[CR]";
                }
                prevCR = decode(input[index]);
                // if no possibility of CRLF
                // only write if doubleCRLF[0] is true
                // no need to write if doubleCRLF[1], because this code will not execute if that is the case
                if (!prevCR && doubleCRLF[0])
                    doubleCRLF[0] = false;
            }
            if (doubleCRLF[1])
            {
                std::cout << "Double CRLF\n";
                break;
            }
        }
        if (readState == 0)
            break;
        std::cout << "Headers:\n"
                  << request.str() << "EOF\n";
        // interpret headers
        request >> method;
        if (method != "GET")
        {
            responseCode = 405; // Method not allowed
            break;
        }

        request >> path;

        if (path[0] != '/')
        {
            responseCode = 400;
            // res += "400 Bad Request" + CRLF;
            res += "400 Bad Request\n";
            break;
        }
        if (path.length() > 1)
        {
            // if the requested resource is not just '/' and it has more than just a get param
            if (path[1] != '?')
            {
                responseCode = 404;
                // res += "404 Not Found" + CRLF;
                res += "404 Not Found\n";
                break;
            }
            else if (path != "/?prev" && path != "/?next")
            {
                responseCode = 400;
                // res += "400 Bad Request" + CRLF;
                res += "400 Bad Request\n";
                break;
            }
            else
                param = path.substr(2, 4);
        }
        std::cout << responseCode << " " << param << std::endl;
        break;
    }
    if (responseCode != 200)
    {
        res += CRLF;
    //     std::string res = "HTTP/1.1 " + responseCode;
    }
    else {
        // res += "200 OK" + CRLF + CRLF;
        res += "200 OK\n";
    }
        // writeState = write(acceptedFD, res.c_str(), res.length());

        writeState = write(acceptedFD, res.c_str(), res.length());
        // res += "Content-Type:text/html; charset=UTF-8" + CRLF;
        // std::string body = "<!DOCTYPE html><html><head><title>COS332 P1</title></head><body>Error: Could not read numbers from file \"fibnums.txt\"</body></html>" + CRLF;
        // res += "Content-Length: " + body.length() + CRLF;
        // res += body + CRLF;
        res = "Content-Type:text/html; charset=UTF-8\n";
        writeState = write(acceptedFD, res.c_str(), res.length());
        std::string body = "<!DOCTYPE html><html><head><title>COS332 P3</title></head><body>Testing</body></html>\n";
        res = "Content-Length: " + std::to_string(body.length()) + "\n\n";
        // res += "\n\n";
        writeState = write(acceptedFD, res.c_str(), res.length());
        res = body + "\n";
        writeState = write(acceptedFD, res.c_str(), res.length());
        std::cout << "----------response----------\n" << res << "-----------------------------\n" ;
        // writeState = write(acceptedFD, res.c_str(), res.length());
        std::cout << writeState << std::endl;
    // writeState = write(acceptedFD, "bye\n", 4);
    delete acceptedAddress;
    addresses[threadNum] = nullptr;
    close(acceptedFD);
    fileDescriptors[threadNum] = -1;
    std::cout << "Connection closed\n";
    executing[threadNum] = false;
    // if (*terminate)
    // {
    //     kill(getpid(), SIGUSR1);
    // }
    // std::terminate();
}
int main(int argc, char *argv[])
{
    // constructQuestions();

    // so the server doesn't suicide when a client disconnects forcefully
    signal(SIGPIPE, SIG_IGN);
    // so the server doesn't suicide with an open port
    signal(SIGINT, sigint_handler);
    int acceptedFD;
    char input[2];
    socklen_t acceptedSockLen;
    struct sockaddr_in serverAddress;
    // needs to be a pointer to pass to thread without overwriting in main while
    struct sockaddr_in *acceptedAddress;
    // clear garbage data from struct
    bzero((char *)&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(80);
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    // std::cout << socketFD << std::endl;
    if (socketFD < 0)
        error("Could not open socket");
    if (bind(socketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        close(socketFD);
        error("Could not bind socket to port 55555");
    }
    if (listen(socketFD, 10) < 0)
    {
        close(socketFD);
        error("Socket could not listen for connections test");
    }
    bool exit = false;
    int readState = 0;

    while (!terminate)
    {
        // std::cout << "while start\n";
        acceptedAddress = new struct sockaddr_in;
        acceptedSockLen = sizeof(*acceptedAddress);
        // clear garbage data
        bzero(input, 2);
        acceptedFD = accept(socketFD, (struct sockaddr *)acceptedAddress, &acceptedSockLen);
        // std::cout << "accepted\n";
        if (acceptedFD < 0)
        {
            // std::cout << "Invalid accepted socket\n";
            continue;
        }
        if (terminate)
        {
            close(acceptedFD);
            kill(getpid(), SIGUSR1);
        }
        int threadNum = 0;
        while (threads[threadNum] != nullptr && executing[threadNum] && threadNum < 5)
        {
            ++threadNum;
        }
        if (threadNum >= 5)
        {
            continue;
        }
        if (executing[threadNum])
        {
            continue;
        }
        // std::cout << "New thread: " << threadNum << std::endl;
        if (threads[threadNum] != nullptr && !executing[threadNum])
        {
            try
            {
                threads[threadNum]->join();
            }
            catch (...)
            {
            }
            delete threads[threadNum];
            threads[threadNum] = nullptr;
        }
        executing[threadNum] = true;
        addresses[threadNum] = acceptedAddress;
        threads[threadNum] = new std::thread(connectionHandler, acceptedFD, acceptedAddress, &terminate, threadNum, executing);
        threads[threadNum]->detach();
        // std::cout << "Thread: " << threadNum << " detached\n";
        // if (terminate)
        // {
        //     break;
        // }
    }
    for (int i = 0; i < 5; ++i)
    {
        if (threads[i] != nullptr)
        {
            try
            {
                threads[i]->join();
            }
            catch (...)
            {
            }
            delete threads[i];
            threads[i] = nullptr;
        }
    }
    // std::cout << "Terminating server\n";
    close(socketFD);
    return 0;
}