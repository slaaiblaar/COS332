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
#include <cmath>
#include <utility>
// lets the process kill itself gracefully
void sigint_handler(int signal)
{
    exit(signal);
}

class LDAPSearchEntry
{
public:
    std::string objectName = "INVALID";
    std::unordered_map<std::string, std::vector<std::string>> attributes;
    std::vector<std::string> children;
    int messageID;
    void print() {
        std::cout << "Object Name: " << objectName << std::endl;
        std::cout << "\tMessage ID: " << messageID << std::endl;
        std::cout << "\tAttributes: \n";
        for (std::pair<std::string, std::vector<std::string>> attribute : attributes)
        {
            std::cout << "\t\t" << attribute.first << ": ";
            for (std::string value : attribute.second)
            {
                std::cout << value << ", ";
            }
            std::cout << std::endl;
        }
        if (children.size() > 0)
        {
            std::cout << "\tChildren: \n";
            for (std::string child : children)
            {
                std::cout << "\t\t" << child << std::endl;
            }
        }
    }
};
class LDAPResponse
{
private:
    std::vector<std::vector<unsigned char>> messages;
    int messageID;
    std::vector<LDAPSearchEntry> entries;
    // bool resDone = false;
public:
    int getExpectedLength(std::vector<unsigned char> message)
    {
        if (message.size() < 2)
            return -1;
        int messageLength = static_cast<int>((unsigned char)message[1]);
        int bytesLength = 0;
        if (messageLength >= 0x80)
        {
            bytesLength = messageLength - 0x80;
            if (message.size() < 2 + bytesLength)
            {
                return -1;
            }
            std::stringstream lenHex;
            lenHex << std::hex << std::setfill('0');
            for (int x = 0; x < message.size() && x < bytesLength; ++x)
            {
                lenHex << std::setw(2) << static_cast<int>((unsigned char)message[2 + x]);
            }
            messageLength = std::stoi(lenHex.str(), nullptr, 16);
        }

        return 2 + messageLength + bytesLength;
    }
    void getLengthBytes(unsigned char *buffer, int length, int &messageLength, int &lengthBytes, int startPos)
    {
        messageLength = static_cast<int>((unsigned char)buffer[startPos + 1]);
        lengthBytes = 0;
        if (messageLength >= 0x80)
        {
            lengthBytes = messageLength - 0x80;
            std::stringstream lenHex;
            lenHex << std::hex << std::setfill('0');
            for (int x = 0; x < lengthBytes; ++x)
            {
                lenHex << std::setw(2) << static_cast<int>((unsigned char)buffer[startPos + 2 + x]);
            }
            messageLength = std::stoi(lenHex.str(), nullptr, 16);
        }
    }
    LDAPSearchEntry parseSearchEntry(unsigned char *buffer, int length)
    {
        LDAPSearchEntry entry;
        int messageLength = static_cast<int>((unsigned char)buffer[1]);
        int lengthBytes = 0;
        getLengthBytes(buffer, length, messageLength, lengthBytes, 0);
        entry.messageID = static_cast<int>(buffer[2 + lengthBytes + 2]);
        const int entryStart = 2 + lengthBytes + 2 + 1; // after ID
        if (buffer[entryStart] != 0x64)
        {
            // std::cout << "Invalid entry identifier: 0x" << (std::stringstream() << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[entryStart])).str() << std::endl;
            if (buffer[entryStart] == 0x65)
            {
                entry.objectName = "searchResDone";
                return entry;
            }
            return entry;
        }
        messageLength = static_cast<int>((unsigned char)buffer[1 + entryStart]);
        lengthBytes = 0;
        getLengthBytes(buffer, length, messageLength, lengthBytes, entryStart);
        const int objectNameStart = entryStart + 2 + lengthBytes;
        messageLength = static_cast<int>((unsigned char)buffer[objectNameStart + 1]);
        lengthBytes = 0;
        getLengthBytes(buffer, length, messageLength, lengthBytes, objectNameStart);
        std::stringstream objectName;
        for (int x = 0; x < messageLength; ++x)
        {
            objectName << buffer[objectNameStart + 2 + lengthBytes + x];
        }
        entry.objectName = objectName.str();
        // std::cout << "Object name: " << entry.objectName << std::endl;

        int attributesStart = objectNameStart + 2 + lengthBytes + messageLength;
        // std::cout << "Attributes start: " << attributesStart << std::endl;
        int attributesLength = static_cast<int>((unsigned char)buffer[attributesStart + 1]);
        lengthBytes = 0;
        getLengthBytes(buffer, length, attributesLength, lengthBytes, attributesStart);
        int partialAttributeListStart;
        int partialAttributeListLength;
        int attributeDescriptionPos;
        int attributeDescriptionLength;
        partialAttributeListStart = attributesStart + 2 + lengthBytes;
        partialAttributeListLength = static_cast<int>((unsigned char)buffer[partialAttributeListStart + 1]);
        lengthBytes = 0;
        getLengthBytes(buffer, length, partialAttributeListLength, lengthBytes, partialAttributeListStart);
        do
        {
            // std::cout << "\tPartial attribute list start: " << partialAttributeListStart << std::endl;
            // std::cout << "\tPartial attribute list length: " << static_cast<int>(partialAttributeListLength) << std::endl;
            attributeDescriptionPos = partialAttributeListStart + 2 + lengthBytes;
            // std::cout << "\t\tAttribute description pos: " << attributeDescriptionPos << std::endl;
            attributeDescriptionLength = static_cast<int>((unsigned char)buffer[attributeDescriptionPos + 1]);
            lengthBytes = 0;
            getLengthBytes(buffer, length, attributeDescriptionLength, lengthBytes, attributeDescriptionPos);
            // std::cout << "\t\tAttribute description length: " << static_cast<int>(attributeDescriptionLength) << std::endl;
            std::string attributeDescription;
            for (int x = 0; x < attributeDescriptionLength; ++x)
            {
                attributeDescription += buffer[attributeDescriptionPos + 2 + lengthBytes + x];
            }
            // std::cout << "\t\t\tAttribute description: " << attributeDescription << std::endl;
            int attributeValuesStart = attributeDescriptionPos + 2 + lengthBytes + attributeDescriptionLength;
            // std::cout << "\t\tAttribute values start: " << attributeValuesStart << std::endl;
            int attributeValuesLength = static_cast<int>((unsigned char)buffer[attributeValuesStart + 1]);
            lengthBytes = 0;
            getLengthBytes(buffer, length, attributeValuesLength, lengthBytes, attributeValuesStart);
            // std::cout << "\t\tAttribute values length: " << static_cast<int>(attributeValuesLength) << std::endl;
            std::vector<std::string> attributeValues;
            int attributeValueStart = attributeValuesStart + 2 + lengthBytes;
            int attributeValueLength = static_cast<int>((unsigned char)buffer[attributeValueStart + 1]);
            lengthBytes = 0;
            getLengthBytes(buffer, length, attributeValueLength, lengthBytes, attributeValueStart);
            do
            {
                // std::cout << "\t\tAttribute value start: " << attributeValueStart << std::endl;
                // std::cout << "\t\tAttribute value length: " << static_cast<int>(attributeValueLength) << std::endl;
                std::string attributeValue;
                for (int x = 0; x < attributeValueLength; ++x)
                {
                    attributeValue += buffer[attributeValueStart + 2 + lengthBytes + x];
                }
                // std::cout << "\t\t\tAttribute value: " << attributeValue << std::endl;
                if (entry.attributes.find(attributeDescription) == entry.attributes.end())
                {
                    entry.attributes[attributeDescription] = std::vector<std::string>();
                }
                entry.attributes[attributeDescription].push_back(attributeValue);
                attributeValueStart += 2 + lengthBytes + attributeValueLength;
                getLengthBytes(buffer, length, attributeValueLength, lengthBytes, attributeValueStart);
            } while (attributeValueStart < attributeValuesStart + 2 + lengthBytes + attributeValuesLength);

            // reset lengthBytes
            getLengthBytes(buffer, length, partialAttributeListLength, lengthBytes, partialAttributeListStart);
            // std::cout << "\tCurr partialAttributeList start: " << partialAttributeListStart << std::endl;
            // std::cout << "\tCurr partialAttributeList length: " << partialAttributeListLength << std::endl;
            // std::cout << "\tCurr partialAttributeList length bytes: " << lengthBytes << std::endl;
            // get next partialAttributeList position
            partialAttributeListStart += 2 + lengthBytes + partialAttributeListLength;
            getLengthBytes(buffer, length, partialAttributeListLength, lengthBytes, partialAttributeListStart);
            // std::cout << "\tNext partialAttributeList start: " << partialAttributeListStart << std::endl;
            // std::cout << "\tNext partialAttributeList length: " << partialAttributeListLength << std::endl;
            // std::cout << "\tNext partialAttributeList length bytes: " << lengthBytes << std::endl;
            // while next partialAttributeList exists
        } while (partialAttributeListStart < attributesStart + 2 + lengthBytes + attributesLength);
        return entry;
    }
    void parseSearchResponse(unsigned char *buffer, int length)
    {
        // check if previous message is complete and add a new message if necessary
        // std::cout << "Parse start\n";
        std::vector<unsigned char> *lastMessage;
        int lastMessageExpectedLength = 0;
        // std::cout << "Num messages: " << messages.size();
        int bIndex = 0;
        while (bIndex < length && !resDone())
        {
            // std::cout << "while start";
            if (messages.size() == 0 || lastMessage->size() == getExpectedLength((messages[messages.size() - 1])) || !resDone())
            {
                // std::cout << "\tCreating new message vector\n";
                messages.push_back(std::vector<unsigned char>());
                // std::cout << "\tNum messages: " << messages.size();

                lastMessage = &(messages[messages.size() - 1]);

                // std::cout << "\tPushing first : ";
                while (bIndex < length && getExpectedLength(*lastMessage) == -1)
                {
                    // std::cout << (std::stringstream() << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(buffer[bIndex])).str() << " ";
                    lastMessage->push_back(buffer[bIndex++]);
                }
                // std::cout << std::endl;
                // lastMessage = &(messages[messages.size() - 1]);
                lastMessageExpectedLength = getExpectedLength(*lastMessage);
            }
            while (bIndex < length && lastMessage->size() < lastMessageExpectedLength)
            {
                lastMessage->push_back(buffer[bIndex++]);
            }
        }
    }
    bool resDone()
    {
        for (std::vector<unsigned char> message : messages)
        {
            if (getExpectedLength(message) == -1 || message.size() != getExpectedLength(message))
            {
                continue;
            }
            int messageLength = static_cast<int>((unsigned char)message[1]);
            int bytesLength = 0;
            if (messageLength >= 0x80)
            {
                bytesLength = messageLength - 0x80;
                if (message.size() < 2 + bytesLength)
                {
                    return -1;
                }
                std::stringstream lenHex;
                lenHex << std::hex << std::setfill('0');
                for (int x = 0; x < message.size() && x < bytesLength; ++x)
                {
                    lenHex << std::setw(2) << static_cast<int>((unsigned char)message[2 + x]);
                }
                messageLength = std::stoi(lenHex.str(), nullptr, 16);
            }
            int protocolOp = static_cast<int>(message[2 + bytesLength + 3]);
            // std::cout << "ProtocolOp: 0x" << (std::stringstream() << std::hex << std::setfill('0') << std::setw(2) << protocolOp).str() << std::endl;
            if (protocolOp == 0x65)
            { // if protocolOp is searchResDone
                return true;
            }
            LDAPSearchEntry entry = parseSearchEntry(message.data(), message.size());
            // std::cout << "Object Name: " << entry.objectName << std::endl;
        }
        return false;
    }
    void printMessages()
    {
        for (int mi = 0; mi < messages.size(); ++mi)
        {
            std::vector<unsigned char> m = messages[mi];
            std::cout << "Message " << std::to_string(mi) << std::endl;
            std::stringstream ss;
            ss << std::hex << std::setfill('0');
            for (int x = 0; x < m.size(); ++x)
            {
                if (x != 0 && x % 8 == 0)
                    ss << " ";
                if (x != 0 && x % 16 == 0)
                    ss << "\n";
                ss << std::setw(2) << static_cast<int>(m[x]) << " ";
            }
            std::cout << ss.str() << std::endl;
            LDAPSearchEntry entry = parseSearchEntry(m.data(), m.size());
            std::cout << "Object Name: " << entry.objectName << std::endl;
            std::cout << "Message ID: " << entry.messageID << std::endl;
            std::cout << "Attributes: \n";
            for (std::pair<std::string, std::vector<std::string>> attribute : entry.attributes)
            {
                std::cout << "\t" << attribute.first << ": ";
                for (std::string value : attribute.second)
                {
                    std::cout << value << ", ";
                }
                std::cout << std::endl;
            }
        }
    }
    void constructEntries()
    {
        for (std::vector<unsigned char> m : messages)
        {
            LDAPSearchEntry entry = parseSearchEntry(m.data(), m.size());
            entries.push_back(entry);
        }
    }
    // parameter is a string of words separated by beriods ('.')
    std::vector<LDAPSearchEntry> getEntry(std::string objectNameComponent)
    {
        std::vector<LDAPSearchEntry> matchingEntries;
        std::vector<std::string> objectNameComponents;
        std::vector<std::string> searchComponents;
        std::string temp = "";
        for (char c : objectNameComponent)
        {
            if (c == '.')
            {
                searchComponents.push_back(temp);
                temp = "";
            }
            else
            {
                temp += c;
            }
        }
        searchComponents.push_back(temp);
        for (LDAPSearchEntry entry : entries)
        {
            objectNameComponents.clear();
            temp = "";
            for (char c : entry.objectName)
            {
                if (c == ',')
                {
                    objectNameComponents.push_back(temp);
                    temp = "";
                }
                else if (c == '=') 
                {
                    temp = "";
                }
                else
                {
                    temp += c;
                }
            }
            objectNameComponents.push_back(temp);
            bool match = true;
            for (int i = 0; i < searchComponents.size() && i < objectNameComponents.size(); ++i)
            {
                if (objectNameComponents[i] != searchComponents[i])
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                std::vector<LDAPSearchEntry> children = findChildren(entry);
                for (LDAPSearchEntry child : children)
                {
                    std::string childName = "";
                    std::string temp = "";
                    for (char c : child.objectName)
                    {
                        if (c == ',')
                        {
                            if (childName.length() != 0)
                                childName += ".";
                            childName += temp;
                            temp = "";
                        }
                        else if (c == '=')
                        {
                            temp = "";
                        }
                        else
                        {
                            temp += c;
                        }
                    }
                    if (childName.length() != 0)
                        childName += ".";
                    childName += temp;
                    entry.children.push_back(childName);
                }
                matchingEntries.push_back(entry);
            }
        }
        return matchingEntries;
    }
    std::vector<LDAPSearchEntry> findChildren(LDAPSearchEntry entry)
    {
        std::vector<LDAPSearchEntry> children;
        std::string parentName = entry.objectName;
        // std::cout << "Parent: " << parentName << std::endl;
        for (LDAPSearchEntry e : entries)
        {
            if (e.objectName.find(parentName) != std::string::npos && e.objectName != parentName)
            {
                // std::cout << parentName << " contained in " << e.objectName << std::endl;
                // if only one extra comma and entry.objectName is at end of e.objectName,
                // then e is a child of entry
                if (e.objectName.find(",") + 1 == e.objectName.find(parentName) 
                &&  e.objectName.find(parentName) + parentName.length() == e.objectName.length())
                {
                    children.push_back(e);
                }
            }
        }
        return children;
    }
};
class LDAPRequestBuilder
{
public:
    LDAPRequestBuilder &setMessageID(int messageID)
    {
        this->messageID = messageID;
        return *this;
    }

    LDAPRequestBuilder &appendDNComponent(const std::string &component)
    {
        if (!dn.empty())
        {
            dn += ",";
        }
        dn += component;
        return *this;
    }

    LDAPRequestBuilder &setScope(const std::string &scope)
    {
        if (scope == "baseObject")
        {
            this->scope = 0;
        }
        else if (scope == "singleLevel")
        {
            this->scope = 1;
        }
        else if (scope == "wholeSubtree")
        {
            this->scope = 2;
        }
        else
        {
            std::cout << "Invalid scope: " << scope << std::endl;
        }
        return *this;
    }

    std::string build() const
    {

        // // Authentication
        // std::stringstream ssAuth;
        // ssAuth << std::hex << std::setfill('0')
        //        << std::setw(2) << 0x80  // [CONTEXT 0] tag
        //        << std::setw(2) << 0x00;  // length
        std::string control = "a01b30190417322e31362e3834302e312e3131333733302e332e342e32";
        std::stringstream attributes;
        attributes << std::hex << std::setfill('0')
                   << std::setw(2) << 0x30                  // SEQUENCE tag
                   << std::setw(2) << 0x03                  // length
                   << std::setw(2) << 0x04                  // OCTET STRING tag
                   << std::setw(2) << 0x01                  // length
                   << std::setw(2) << static_cast<int>('*') // all
                                                            //    << std::setw(2) << 0x04
                                                            //    << std::setw(2) << 0x01
                                                            //    << std::setw(2) << 0x2b
            ;
        std::string filterPresent = "objectClass";
        std::stringstream filter;
        filter << std::hex << std::setfill('0')
               << std::setw(2) << 0xa0
               << std::setw(2) << 0x0d
               << std::setw(2) << 0x87
               << std::setw(2) << filterPresent.length();
        for (char c : filterPresent)
        {
            filter << std::setw(2) << static_cast<int>(c);
        }
        // std::cout << "Filter: " << filter.str() << std::endl;
        std::string typesOnly = "010100";
        std::string timeLimit = "020100";
        std::string sizeLimit = "020100";
        std::string derefAliases = "0a0100";
        std::stringstream ssScope;
        ssScope << std::hex << std::setfill('0')
                << std::setw(2) << 0x0a // ENUMERATED tag
                << std::setw(2) << 0x01 // length
                << std::setw(2) << scope;
        // DN
        std::stringstream ssDN;
        ssDN << std::hex << std::setfill('0')
             << std::setw(2) << 0x04         // OCTET STRING tag
             << std::setw(2) << dn.length(); // length
        for (char c : dn)
        {
            ssDN << std::setw(2) << static_cast<int>(c);
        }

        // BindRequest
        std::stringstream ssBR;
        int lengthBR = (ssDN.str().length() / 2) + (ssScope.str().length() / 2) + (derefAliases.length() / 2) + (sizeLimit.length() / 2) + (timeLimit.length() / 2) + (typesOnly.length() / 2) + (filter.str().length() / 2) + (attributes.str().length() / 2);
        ssBR << std::hex << std::setfill('0')
             << std::setw(2) << 0x63      // [APPLICATION 0] tag
             << std::setw(2) << lengthBR; // length
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
                  << std::setw(2) << 0x30                                                     // SEQUENCE tag
                  << std::setw(2) << ((ssBR.str().length() / 2) + 3 + (control.length() / 2)) // length
                  << std::setw(2) << 0x02                                                     // INTEGER tag
                  << std::setw(2) << 0x01                                                     // length
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
    {80, "other"}};
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
    // std::cout << "Length: " << std::to_string(length) << std::endl;
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i)
    {
        // std::cout << std::to_string(i) << std::endl;
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    // std::cout << "Hex: " << ss.str() << std::endl;
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
        // std::cout << "Bind response\n";
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
    // std::cout << "Response sequence length: " << std::to_string(responseSequenceLength) << "\n";
    // std::cout << "Response sequence: ";
    // std::stringstream ss;
    // ss << std::hex;
    // for (int x = 7; x < 7 + responseSequenceLength; x++)
    // {
    //     ss << std::setw(2) << std::setfill('0') << (int)data[x] << " ";
    // }
    // std::cout << ss.str() << "\n";
    if (responseSequenceLength < 7)
    {
        std::cout << "Invalid response, response sequence not found\n";
        return false;
    }
    int resultCode = static_cast<int>(data[9]);
    // std::cout << "Result code: " << std::to_string(resultCode) << " (" << resultCodes[resultCode] << ")\n";
    return (resultCode == 0);
}

// returns -1 if invalid message, returns index where message ends if normal message, otherwise returns 0 if searchResDone message received
int parseSearchResponse(unsigned char *buffer, int length)
{
    int messageLength = 0;
    if (length < 1)
    {
        std::cout << "message length < 1\n";
        return -1;
    }

    if (static_cast<int>((unsigned char)buffer[0]) != 0x30)
    {
        std::cout << "Invalid identifier: 0x" << (std::stringstream() << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[0])).str() << std::endl;
        return -1;
    }

    messageLength = static_cast<int>((unsigned char)buffer[1]);
    int lengthBytes = 0;
    // long form definite length
    if (messageLength >= 0x80)
    {
        lengthBytes = messageLength - 0x80;
        std::stringstream lenHex;
        lenHex << std::hex << std::setfill('0');
        for (int x = 0; x < lengthBytes; ++x)
        {
            lenHex << std::setw(2) << static_cast<int>((unsigned char)buffer[2 + x]);
        }
        messageLength = std::stoi(lenHex.str(), nullptr, 16);
    }
    // std::cout << "parseSearchResponse length: " << messageLength << std::endl;
    return messageLength + 2 + lengthBytes;
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
    // for (int x = 0; x < message.size(); x++)
    // {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)message[x] << " ";
    // }
    // std::cout << std::endl;
    // Send the message to the server
    if (send(sockfd, message.data(), message.size(), 0) < 0)
    {
        perror("Send failed");
        return 1;
    }
    // std::cout << "Sent\n";
    // Wait for the response
    char buffer[2048];
    int valread;
    memset(buffer, 0, sizeof(buffer));
    valread = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    // std::cout << std::endl;
    if (valread < 0)
    {
        perror("Receive failed");
        return 1;
    }
    buffer[valread] = '\0'; // Null-terminate the received data
    // std::cout << "Received: \n";
    // std::cout << std::string(buffer) << std::endl;
    std::string responseHex = bytesToHex((unsigned char *)buffer, valread);
    // std::cout << responseHex << std::endl;
    bool bindSuccess = parseBindResponse((unsigned char *)buffer, valread);

    if (bindSuccess)
    {
        // std::cout << "Bind successful\n";
    }
    else
    {
        std::cout << "Anonymous bind failed. Ensure the server is running on 127.0.0.1:389\n";
        close(sockfd);
        return 0;
    }

    // Get search term from user
    bool continueSearch;
    do
    {
        std::cout << "Enter organisation name: \n";
        std::string searchParam;
        std::cin >> searchParam;
        std::string searchQuery = "dc=za";
        // std::cout << "Searching for: " << searchQuery << std::endl;

        // Construct the LDAP search request
        LDAPRequestBuilder builder;
        builder.setMessageID(messageID)
            .appendDNComponent(searchQuery)
            .setScope("wholeSubtree");
        std::string searchRequest = builder.build();
        // std::cout << "Search request: " << searchRequest << std::endl;
        message = hexToBytes(searchRequest);
        // Send the message to the server
        if (send(sockfd, message.data(), message.size(), 0) < 0)
        {
            perror("Send failed");
            return 1;
        }
        memset(buffer, 0, sizeof(buffer));
        // std::cout << "Sent\n";
        // Wait for the response
        bool responseComplete = false;
        int messageStart = 0;
        LDAPResponse response;
        while (!responseComplete)
        {
            valread = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
            if (valread < 0)
            {
                perror("Receive failed");
                return 1;
            }
            buffer[valread] = '\0'; // Null-terminate the received data
            // std::cout << "Received: " << std::string(buffer) << std::endl;
            std::string responseHex = bytesToHex((unsigned char *)buffer, valread);
            // std::cout << "Received (Hex): \n";
            // std::cout << "\n0040          ";
            for (int x = 0; x < valread; ++x)
            {
                // if ((x + 2) % 16 == 0)
                //     std::cout << std::endl
                //               << "0" << std::setw(2) << std::setfill('0') << ((x + 2) / 16 + 4) << "0   ";
                // if (x % 50 == 0)
                //     std::cout << std::endl;
                // std::cout << " ";
                // std::cout << (std::stringstream() << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>((unsigned char)buffer[x]) << " ").str();
            }
            // std::cout << "\n--------eof----------\n";
            if (valread < 2)
            {
                // std::cout << "Invalid response\n";
                continue;
            }
            response.parseSearchResponse((unsigned char *)buffer, valread);
            // std::cout << "\n----------Parse Complete----------\n";
            if (response.resDone())
            {
                // std::cout << "Response Complete\n";
                responseComplete = true;
                continue;
            }

            memset(buffer, 0, sizeof(buffer));
        }
        // response.printMessages();
        // std::cout << "Constructing entries\n";
        response.constructEntries();
        // std::cout << "Geting matching entries\n";
        std::vector<LDAPSearchEntry> matchingEntries = response.getEntry(searchParam);
        if (matchingEntries.size() == 0)
        {
            std::cout << "No matching entries found\n";
        }
        else {
            std::cout << "Matching entries: \n";
            for (LDAPSearchEntry entry : matchingEntries)
            {
                entry.print();
            }
        }
        
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
