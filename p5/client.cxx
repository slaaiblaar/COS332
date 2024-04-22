#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <unordered_map>
// lets the process kill itself gracefully
void sigint_handler(int signal)
{
    exit(signal);
}
class LDAPRequestBuilder {
public:
    LDAPRequestBuilder& setMessageID(int messageID) {
        this->messageID = messageID;
        return *this;
    }

    LDAPRequestBuilder& appendDNComponent(const std::string& component) {
        if (!dn.empty()) {
            dn += ",";
        }
        dn += component;
        return *this;
    }

    LDAPRequestBuilder& setScope(const std::string& scope) {
        if (scope == "baseObject") {
            this->scope = 0;
        } else if (scope == "singleLevel") {
            this->scope = 1;
        } else if (scope == "wholeSubtree") {
            this->scope = 2;
        } else {
            std::cout << "Invalid scope: " << scope << std::endl;
        }
        return *this;
    }

    std::string build() const {

        // // Authentication
        // std::stringstream ssAuth;
        // ssAuth << std::hex << std::setfill('0')
        //        << std::setw(2) << 0x80  // [CONTEXT 0] tag
        //        << std::setw(2) << 0x00;  // length
        std::string control = "a01b30190417322e31362e3834302e312e3131333733302e332e342e32";
        std::stringstream attributes;
        attributes << std::hex << std::setfill('0')
                   << std::setw(2) << 0x30  // SEQUENCE tag
                   << std::setw(2) << 0x06  // length
                   << std::setw(2) << 0x04  // OCTET STRING tag
                   << std::setw(2) << 0x01  // length
                   << std::setw(2) << static_cast<int>('*') // all
                   << std::setw(2) << 0x04
                   << std::setw(2) << 0x01
                   << std::setw(2) << 0x2b;
        std::string filterPresent = "objectClass";
        std::stringstream filter;
        filter << std::hex << std::setfill('0')
               << std::setw(2) << 0xa0
               << std::setw(2) << 0x0d 
               << std::setw(2) << 0x87
               << std::setw(2) << filterPresent.length();
        for (char c : filterPresent) {
            filter << std::setw(2) << static_cast<int>(c);
        }
        std::cout << "Filter: " << filter.str() << std::endl;
        std::string typesOnly = "010100";
        std::string timeLimit = "020100";
        std::string sizeLimit = "020100";
        std::string derefAliases = "0a0100";
        std::stringstream ssScope;
        ssScope << std::hex << std::setfill('0')
                << std::setw(2) << 0x0a  // ENUMERATED tag
                << std::setw(2) << 0x01  // length
                << std::setw(2) << scope;
        // DN
        std::stringstream ssDN;
        ssDN << std::hex << std::setfill('0')
             << std::setw(2) << 0x04  // OCTET STRING tag
             << std::setw(2) << dn.length();  // length
        for (char c : dn) {
            ssDN << std::setw(2) << static_cast<int>(c);
        }

        // BindRequest
        std::stringstream ssBR;
        int lengthBR = (ssDN.str().length()/2) 
                        + (ssScope.str().length()/2) 
                        + (3*3)
                        + (filter.str().length()/2)
                        + (attributes.str().length()/2);
        ssBR << std::hex << std::setfill('0')
             << std::setw(2) << 0x63  // [APPLICATION 0] tag
             << std::setw(2) << lengthBR;  // length
        ssBR << ssDN.str()
             << ssScope.str()
             << derefAliases
             << sizeLimit
             << timeLimit
             << typesOnly
             << filter.str()
             << attributes.str(); // Manage DSA IT

        std::stringstream ssMessage;
        ssMessage << std::hex << std::setfill('0')
                  << std::setw(2) << 0x30  // SEQUENCE tag
                  << std::setw(2) << ((ssBR.str().length()/2) + 3 + (control.length() / 2))  // length
                  << std::setw(2) << 0x02  // INTEGER tag
                  << std::setw(2) << 0x01  // length
                  << std::setw(2) << messageID;
        ssMessage << ssBR.str() << control;
        return ssMessage.str();
    }

private:
    int messageID;
    std::string dn;
    int scope;
};
std::unordered_map<int, std::string> resultCodes = {
    {0, "success"},
    {1, "operationsError"},
    {2, "protocolError"},
    {3, "timeLimitExceeded"},
    {4, "sizeLimitExceeded"},
    {5, "compareFalse"},
    {6, "compareTrue"},
    {7, "authMethodNotSupported"},
    {8, "strongerAuthRequired"},
    // -- 9 reserved --
    {10, "referral"},
    {11, "adminLimitExceeded"},
    {12, "unavailableCriticalExtension"},
    {13, "confidentialityRequired"},
    {14, "saslBindInProgress"},
    {16, "noSuchAttribute"},
    {17, "undefinedAttributeType"},
    {18, "inappropriateMatching"},
    {19, "constraintViolation"},
    {20, "attributeOrValueExists"},
    {21, "invalidAttributeSyntax"},
    //  -- 22-31 unused --
    {32, "noSuchObject"},
    {33, "aliasProblem"},
    {34, "invalidDNSyntax"},
    // -- 35 reserved for undefined isLeaf --
    {36, "aliasDereferencingProblem"},
    // -- 37-47 unused --
    {48, "inappropriateAuthentication"},
    {49, "invalidCredentials"},
    {50, "insufficientAccessRights"},
    {51, "busy"},
    {52, "unavailable"},
    {53, "unwillingToPerform"},
    {54, "loopDetect"},
    // -- 55-63 unused --
    {64, "namingViolation"},
    {65, "objectClassViolation"},
    {66, "notAllowedOnNonLeaf"},
    {67, "notAllowedOnRDN"},
    {68, "entryAlreadyExists"},
    {69, "objectClassModsProhibited"},
    // -- 70 reserved for CLDAP --
    {71, "affectsMultipleDSAs"},
    // -- 72-79 unused --
    {80, "other"}
};
std::vector<unsigned char> hexToBytes(const std::string &hex)
{
    std::vector<unsigned char> bytes;

    for (unsigned int i = 0; i < hex.length(); i += 2)
    {
        std::string byteString = hex.substr(i, 2);
        unsigned char byte = (unsigned char)strtol(byteString.c_str(), NULL, 16);
        bytes.push_back(byte);
    }

    return bytes;
}
std::string bytesToHex(const unsigned char *data, size_t length)
{
    std::cout << "Length: " << std::to_string(length) << std::endl;
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i)
    {
        std::cout << std::to_string(i) << std::endl;
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    std::cout << "Hex: " << ss.str() << std::endl;
    return ss.str();
}

int messageID = 2;
bool parseBindResponse(const unsigned char *data, size_t length)
{
    if (length < 2)
    {
        std::cout << "Invalid response\n";
        return false;
    }
    int tag = static_cast<int>(data[0]);
    int messageLength = static_cast<int>(data[1]);
    if (length != messageLength + 2)
    {
        std::cout << "Invalid message length. received: " << std::to_string(length) << ", expected: " << std::to_string(messageLength + 2) << "\n";
        return false;
    }
    if (data[2] != 0x02)
    {
        std::cout << "Invalid response, message id not type integer (0x02). Received type: 0x" << (std::stringstream() << std::hex << data[2]).str() << "\n";
        return false;
    }
    int respMessageID = static_cast<int>(data[4]);
    if (1 != respMessageID)
    {
        std::cout << "Invalid response, message id does not match. Expected: " << std::to_string(messageID) << ", received: " << std::to_string(respMessageID) << "\n";
        return false;
    }
    if (length < 6)
    {
        std::cout << "Invalid response, message type not found\n";
        return false;
    }

    int messageType = static_cast<int>(data[5]);
    if (messageType == 0x61)
    {
        std::cout << "Bind response\n";
    }
    else
    {
        std::cout << "Response type not BindResponse: 0x" << (std::stringstream() << std::hex << messageType).str() << "\n";
        return false;
    }
    if (length < 7)
    {
        std::cout << "Invalid response, response sequence length not found\n";
        return false;
    }
    int responseSequenceLength = static_cast<int>(data[6]);
    if (length < 7 + responseSequenceLength)
    {
        std::cout << "Invalid response, response sequence not found. Expected length: " << std::to_string(7 + responseSequenceLength) << ", received length: " << std::to_string(length) << "\n";
        return false;
    }
    std::cout << "Response sequence length: " << std::to_string(responseSequenceLength) << "\n";
    std::cout << "Response sequence: ";
    std::stringstream ss;
    ss << std::hex;
    for (int x = 7; x < 7 + responseSequenceLength; x++)
    {
        ss << std::setw(2) << std::setfill('0') << (int)data[x] << " ";
    }
    std::cout << ss.str() << "\n";
    if (responseSequenceLength < 7)
    {
        std::cout << "Invalid response, response sequence not found\n";
        return false;
    }
    int resultCode = static_cast<int>(data[9]);
    std::cout << "Result code: " << std::to_string(resultCode) << " (" << resultCodes[resultCode] << ")\n";
    return (resultCode == 0);
}
int main()
{
    // so the server doesn't suicide when a client disconnects forcefully
    signal(SIGPIPE, SIG_IGN);
    // so the server doesn't suicide with an open port
    signal(SIGINT, sigint_handler);
    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Unable to create socket");
        return 1;
    }

    // Specify the server address and port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(389); // LDAP server port
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address");
        return 1;
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        return 1;
    }

    // Construct the LDAP bind request
    // This is a simplified example, you will need to construct a proper LDAP message
    std::string hexString = "300c020101600702010304008000";
    std::vector<unsigned char> message = hexToBytes(hexString);
    // int message[] = {0x30, 0x0e, 0x02, 0x01, 0x01, 0x60, 0x0c, 0x02, 0x01, 0x03, 0x04, 0x00, 0x80, 0x00};
    // const char* message = "LDAP bind request";
    for (int x = 0; x < message.size(); x++)
    {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)message[x] << " ";
    }
    std::cout << std::endl;
    // Send the message to the server
    if (send(sockfd, message.data(), message.size(), 0) < 0)
    {
        perror("Send failed");
        return 1;
    }
    std::cout << "Sent\n";
    // Wait for the response
    char buffer[1024];
    int valread;
    memset(&server_addr, 0, sizeof(server_addr));
    valread = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    // std::cout << std::endl;
    if (valread < 0)
    {
        perror("Receive failed");
        return 1;
    }
    buffer[valread] = '\0'; // Null-terminate the received data
    std::cout << "Received: \n";
    // std::cout << std::string(buffer) << std::endl;
    std::string responseHex = bytesToHex((unsigned char *)buffer, valread);
    std::cout << responseHex << std::endl;
    bool bindSuccess = parseBindResponse((unsigned char *)buffer, valread);

    if (bindSuccess)
    {
        std::cout << "Bind successful\n";
    }
    else
    {
        std::cout << "Anonymous bind failed. Ensure the server is running on 127.0.0.1:389\n";
        close(sockfd);
        return 0;
    }

    // Get search term from user
    bool continueSearch;
    do {
        std::cout << "Enter organisation name: \n";
        std::string searchParam;
        std::cin >> searchParam;
        std::string searchQuery = "dc=" + searchParam;
        std::cout << "Searching for: " << searchQuery << std::endl;

        // Construct the LDAP search request
        LDAPRequestBuilder builder;
        builder.setMessageID(messageID)
            .appendDNComponent(searchQuery)
            .setScope("baseObject");
        std::string searchRequest = builder.build();
        // searchRequest = "304f020102632d040564633d7a610a01000a0100020100020100010100a00d870b6f626a656374436c617373300604012a04012ba01b30190417322e31362e3834302e312e3131333733302e332e342e32";
        std::cout << "Search request: " << searchRequest << std::endl;
        message = hexToBytes(searchRequest);
        // Send the message to the server
        if (send(sockfd, message.data(), message.size(), 0) < 0)
        {
            perror("Send failed");
            return 1;
        }
        memset(&server_addr, 0, sizeof(server_addr));
        std::cout << "Sent\n";
        // Wait for the response
        valread = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (valread < 0)
        {
            perror("Receive failed");
            return 1;
        }
        buffer[valread] = '\0'; // Null-terminate the received data
        std::cout << "Received: " << std::string(buffer) << std::endl;
        memset(&server_addr, 0, sizeof(server_addr));
        ++messageID;
        std::cout << "Continue searching? (y/n)\n";
        char continueChar;
        std::cin >> continueChar;
        continueSearch = (continueChar == 'y');
    } while (continueSearch);

    // Close the socket
    close(sockfd);

    return 0;
}
