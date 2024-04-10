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
#include <unordered_map>
#include <iomanip>
#include <ctime>
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

std::string currentDate()
{
    // Get current time
    std::time_t currentTime = std::time(nullptr);
    std::tm *localTime = std::gmtime(&currentTime); // UTC time

    // Define the months and days of the week
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    // Output the formatted date
    std::stringstream budgetStringBuilder;
    budgetStringBuilder
        << days[localTime->tm_wday] << ", "
        << std::setw(2) << std::setfill('0') << localTime->tm_mday << " "
        << months[localTime->tm_mon] << " "
        << 1900 + localTime->tm_year << " "
        << std::setw(2) << std::setfill('0') << localTime->tm_hour << ":"
        << std::setw(2) << std::setfill('0') << localTime->tm_min << ":"
        << std::setw(2) << std::setfill('0') << localTime->tm_sec << " GMT";
    return budgetStringBuilder.str();
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
    int responseCode;
    std::string param("");
    std::string res;
    const std::string CRLF({13, 10});
    std::string body;
    std::unordered_map<std::string, std::string> head;
    int f1, f2, f3;
    bool notInitialVals = false;
    while (!*terminate)
    {
        res = "HTTP/1.1 ";
        param = "";
        request.str("");
        doubleCRLF[0] = doubleCRLF[1] = false;
        responseCode = 0;
        // reading request
        // std::cout << "[" << threadNum << "][" << strAddr << ":" << acceptedAddress->sin_port << "][" << readState << "]: Waiting...\n";
        while (!*terminate && (readState = read(acceptedFD, input, 255)) > 0)
        {
            // std::cout << "[" << readState << "]: " << input << std::endl;
            // if (readState < 255) std::cout <<"\n last char: " << (0 + input[readState-1]);
            request << input;
            // if final piece of data
            if (readState < 255 || (input[readState - 1] == 10 && input[readState - 2] == 13 && input[readState - 3] == 10 && input[readState - 4] == 13))
                break;
            bzero(input, 256);
        }
        body = "";
        if (request.str() == "")
            break;
        responseCode = 200;
        // std::cout << "Headers:\n"
        //           << request.str() << "EOF\n";
        std::string temp;
        std::getline(request, temp);
        int pos;
        std::stringstream(temp) >> method >> path >> protocolVersion;
        // std::cout << "While getline: \n";
        std::string header;
        while (std::getline(request, temp))
        {
            // std::cout << temp.length() << std::endl;
            if (temp.length() > 1)
            {
                temp.pop_back();
                pos = temp.find(':');
                if (pos == std::string::npos)
                    continue;
                header = temp.substr(0, pos++);
                if (temp.find(' ') == pos)
                    pos += 1;
                head[header] = temp.substr(pos);
            }
        }
        // std::cout << "Hash Map: \n";
        // for (const auto &pair : head)
        // {
        //     std::cout << pair.first << ": " << pair.second << std::endl;
        // }
        // Validate method
        if (method != "GET" && method != "HEAD" && method != "OPTIONS")
        {
            responseCode = 405; // Method not allowed
            res += "405 Method Not Allowed\n";
            break;
        }
        // validate path
        if (path[0] != '/')
        {
            if (method != "OPTIONS" || path != "*")
            {
                responseCode = 400;
                res += "400 Bad Request\n";
                body = "<!DOCTYPE html><html><head><title>COS332 P3</title></head><body>Bad Request</body></html>\n";
                break;
            }
        }
        if (path.length() > 1)
        {
            // if the requested resource is not just '/' and it has more than just a get param
            if (path[1] != '?')
            {
                responseCode = 404;
                res += "404 Not Found\n";
                body = "<!DOCTYPE html><html><head><title>COS332 P3</title></head><body>404 \"" + path + "\" Not Found</body></html>\n";
                break;
            }
            if (path != "/?prev" && path != "/?next") // if invalid param
            {
                responseCode = 400;
                res += "400 Bad Request\n";
                body = "<!DOCTYPE html><html><head><title>COS332 P3</title></head><body>Invalid Parameter</body></html>\n";
                break;
            }
            responseCode = 200;
            res += "200 OK\n";
            param = path.substr(2, 4);
            break;
        }
        else
        {
            responseCode = 200;
            res += "200 OK\n";
        }

        break;
    }

    if (responseCode != 0)
    {
        if (responseCode == 200)
        {
            f1 = 0;
            f2 = f3 = 1;
            if (path != "/")
            {
                auto cookie = head.find("Cookie");
                if (cookie != head.end())
                {
                    std::string temp;
                    int cookieLen = cookie->second.length();
                    int valPos = cookie->second.find('=') + 1;
                    std::string cookieVal = cookie->second.substr(valPos, cookieLen - valPos);
                    std::stringstream nums(cookieVal);
                    int iNums[3] = {0, 1, 1};
                    int counter = 0;
                    while (std::getline(nums, temp, ','))
                    {
                        iNums[counter++] = std::stoi(temp);
                    }
                    f1 = iNums[0];
                    f2 = iNums[1];
                    f3 = iNums[2];
                }

                notInitialVals = (f1 != 0 || f2 != 1 || f3 != 1);
                if (path == "/?next")
                {
                    f3 = (f1 = f2) + (f2 = f3);
                }
                else if (path == "/?prev" && notInitialVals)
                {
                    f1 = (f3 = f2) - (f2 = f1);
                }
                notInitialVals = (f1 != 0 || f2 != 1 || f3 != 1);
            }
            std::stringstream budgetStringBuilder;
            budgetStringBuilder << "<!DOCTYPE html>"
                                << "<html>"
                                << "<head><title>COS332 P1</title></head>"
                                << "<body>"
                                << "<a href=\"/?prev\"" << (!notInitialVals ? " style=\"pointer-events: none\"" : "") << ">Previous</a>"
                                << "(" << std::to_string(f1) << ", " << std::to_string(f2) << ", " << std::to_string(f3) << ")"
                                << "<a href=\"/?next\">Next</a>"
                                << "</body>"
                                << "</html>";
            body = budgetStringBuilder.str();
        }
        // Response
        std::stringstream response;
        if (method == "OPTIONS")
        {

            response << res;
            if (responseCode == 200)
                response << "Allow: GET, HEAD, OPTIONS" << CRLF;
            response << "Content-Length: 0" << CRLF;
        }
        else
        {
            response << res;
            response << "Content-Type:text/html; charset=UTF-8" << CRLF;
            response << "Set-Cookie: seq=" << std::to_string(f1) << ", " << std::to_string(f2) << ", " << std::to_string(f3) << CRLF;
            response << "Content-Length: " << std::to_string(body.length()) << CRLF;
        }
        response << "Cache-Control:max-age=0" << CRLF;
        response << "Server: Toaster 9000" << CRLF;
        response << "Date: " << currentDate() << CRLF;
        // BODY
        if (method == "GET")
            response << CRLF << body << CRLF;
        response << CRLF;
        // std::cout << "----------response----------\n"
        //           << res << "-----------------------------\n";
        res = response.str();
        writeState = write(acceptedFD, res.c_str(), res.length());
    }
    delete acceptedAddress;
    addresses[threadNum] = nullptr;
    close(acceptedFD);
    fileDescriptors[threadNum] = -1;
    executing[threadNum] = false;
    // if (*terminate)
    // {
    //     kill(getpid(), SIGUSR1);
    // }
    // std::terminate();
    // std::cout << "Connection closed\n";
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