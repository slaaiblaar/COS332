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

std::vector<std::string> questions;
std::vector<std::vector<std::string>> answers;
std::vector<char> solutions;
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
                // std::cout << "Joining thread " << i << std::endl;
                threads[i]->join();
            }
            catch (...)
            {
            }
            try
            {
                // std::cout << "Deleting thread " << i << std::endl;
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
                // std::cout << "Deleting address " << i << std::endl;
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

                // std::cout << "Closing FD " << i << ": " << fileDescriptors[i] << std::endl;
                close(fileDescriptors[i]);
            }
            catch (...)
            {
            }
            fileDescriptors[i] = -1;
        }
    }
    // std::cout << "Closing main FD " << std::endl;
    int mainCloseStatus = close(socketFD);

    // std::cout << "Closed with response " << mainCloseStatus << std::endl;
    exit(signal);
}
void constructQuestions()
{
    std::ifstream questionFile("questions.txt");
    std::string line;
    char answerNumber = 0;
    char lineType;
    while (std::getline(questionFile, line))
    {
        lineType = line[0];
        line.erase(0, 1);
        if (lineType == '?')
        {
            questions.push_back(line);
            answers.push_back(std::vector<std::string>());
            answerNumber = 0;
            continue;
        }
        char prefix[] = {answerNumber + 65, ' ', '\0'};
        line.insert(0, prefix);
        answers[answers.size() - 1].push_back(line);
        if (lineType == '+')
        {
            solutions.push_back(answerNumber + 65);
        }
        ++answerNumber;
    }
    questionFile.close();
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
    while (!*terminate)
    {
        // writeState = write(acceptedFD, "Input \"c\" to continue and \"x\" to exit: ", 39);
        // if (writeState < 0)
        // {
        //     break;
        // }
        // readState = read(acceptedFD, input, 255);
        bzero(input, 256);
        readState = read(acceptedFD, input, 255);
        // std::cout << input << std::endl;
        std::cout << "[" << threadNum << "][" << strAddr << ":" << acceptedAddress->sin_port << "][" << readState << "]: ";
        for (int index = 0; index < readState; ++index)
        {
            switch (input[index]) {
                case 32: std::cout << "[SP]"; break;
                case 0: std::cout << "[NULL]"; break;
                case 1: std::cout << "[SOH]"; break;
                case 2: std::cout << "[STX]"; break;
                case 3: std::cout << "[ETX]"; break;
                case 4: std::cout << "[EOT]"; break;
                case 5: std::cout << "[ENQ]"; break;
                case 6: std::cout << "[ACK]"; break;
                case 7: std::cout << "[BEL]"; break;
                case 8: std::cout << "[BS]"; break;
                case 9: std::cout << "[HT]"; break;
                case 10: std::cout << "[LF]"; break;
                case 11: std::cout << "[VT]"; break;
                case 12: std::cout << "[FF]"; break;
                case 13: {
                    if (index + 1 >= readState) {
                        prevCR = true;
                    }
                    if (index + 1 < readState && input[index + 1] == 10)
                    {
                        std::cout << "[CRLF]\n";
                        ++index;
                        break;
                    }
                    std::cout << "[CR]";
                    break;
                }
                case 14: std::cout << "[SO]"; break;
                case 15: std::cout << "[SI]"; break;
                case 16: std::cout << "[DLE]"; break;
                case 17: std::cout << "[DC1]"; break;
                case 18: std::cout << "[DC2]"; break;
                case 19: std::cout << "[DC3]"; break;
                case 20: std::cout << "[DC4]"; break;
                case 21: std::cout << "[NAK]"; break;
                case 22: std::cout << "[SYN]"; break;
                case 23: std::cout << "[ETB]"; break;
                case 24: std::cout << "[CAN]"; break;
                case 25: std::cout << "[EM]"; break;
                case 26: std::cout << "[SUB]"; break;
                case 27: std::cout << "[ESC]"; break;
                case 28: std::cout << "[FS]"; break;
                case 29: std::cout << "[GS]"; break;
                case 30: std::cout << "[RS]"; break;
                case 31: std::cout << "[US]"; break;
                default: std::cout << input[index]; break;
            }
        }
        if (readState <= 0)
        {
            break;
        }
        // input[readState - 2] = '\0';
        // if (strcmp(input, std::string("die").c_str()) == 0)
        // {
        //     *terminate = true;
        //     kill(getpid(), SIGUSR1);
        //     // close(socketFD);
        //     break;
        // }
        // // replace CRLF
        // input[readState - 2] = '\0';
        // if (strcmp(input, std::string("x").c_str()) == 0)
        // {
        //     break;
        // }
        // // writeState = write(acceptedFD, "\033[2J", strlen("\033[2J"));
        // // if (strcmp(input, std::string("c").c_str()) != 0)
        // // {
        // //     continue;
        // // }
        // // randomly select question and construct formatted message
        // int qNum = std::rand() % questions.size();
        // char solution = solutions[qNum];
        // std::string question = "\033[2J\033[0;0H" + questions[qNum];
        // // std::cout << question << std::endl;
        // int a = 0;
        // char coordinates[] = {27, '[', '0', ';', '0', 'H', '\0'};
        // for (; a < answers[qNum].size(); ++a)
        // {
        //     coordinates[2] = 50 + a;
        //     coordinates[4] = '4';
        //     question += coordinates;
        //     question += answers[qNum][a];
        // }
        // coordinates[2] = 50 + a;
        // coordinates[4] = '0';
        // question += coordinates;
        // question += "Answer: ";

        // // std::cout << question << std::endl;

        // // writeState = write(acceptedFD, question.c_str(), strlen(question.c_str()));
        // readState = read(acceptedFD, input, 255);
        // // std::cout << input << std::endl;
        // // std::cout << "Bytes read: " << readState << std::endl;
        // // for (int index = 0; index < readState; ++index)
        // // {
        // //     std::cout << (0 + input[index]) << std::endl;
        // // }
        // if (readState < 0)
        // {
        //     break;
        // }
        // // replace CRLF
        // input[readState - 2] = '\0';
        // // std::cout << input << std::endl;
        // if (strcmp(input, std::string("die").c_str()) == 0)
        // {
        //     *terminate = true;
        //     kill(getpid(), SIGUSR1);
        //     // close(socketFD);
        //     break;
        // }
        // // std::cout << input << " | " << solution << std::endl;
        // // std::cout << strcmp(input, solution.c_str()) << std::endl;
        // // if (input[0] == solution || input[0] == solution + 32)
        // // {
        // //     writeState = write(acceptedFD, "Correct\n", 8);
        // // }
        // // else
        // // {
        // //     writeState = write(acceptedFD, "Incorrect\n", 10);
        // // }
    }
    writeState = write(acceptedFD, "bye\n", 4);
    delete acceptedAddress;
    addresses[threadNum] = nullptr;
    close(acceptedFD);
    fileDescriptors[threadNum] = -1;
    // std::cout << "Connection closed\n";
    executing[threadNum] = false;
    // if (*terminate)
    // {
    //     kill(getpid(), SIGUSR1);
    // }
    // std::terminate();
}
int main(int argc, char *argv[])
{
    constructQuestions();

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
    serverAddress.sin_port = htons(55555);
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
        error("Socket could not listen for connections");
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