#include <sstream>
#include <iomanip>

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

    std::string build() const {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');

        // Message ID
        ss << std::setw(2) << 0x02  // INTEGER tag
           << std::setw(2) << 0x01  // length
           << std::setw(2) << messageID;

        // BindRequest
        ss << std::setw(2) << 0x60  // [APPLICATION 0] tag
           << std::setw(2) << (dn.length() + 7)  // length
           << std::setw(2) << 0x02  // INTEGER tag
           << std::setw(2) << 0x01  // length
           << std::setw(2) << 0x03;  // LDAP version

        // DN
        ss << std::setw(2) << 0x04  // OCTET STRING tag
           << std::setw(2) << dn.length();  // length
        for (char c : dn) {
            ss << std::setw(2) << static_cast<int>(c);
        }

        // Authentication
        ss << std::setw(2) << 0x80  // [CONTEXT 0] tag
           << std::setw(2) << 0x00;  // length

        return ss.str();
    }

private:
    int messageID;
    std::string dn;
};
