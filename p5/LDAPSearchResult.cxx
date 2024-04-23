#include <string>
#include <vector>
class LDAPPartialAttribute {
private:
    std::string type;
    std::string values;
};

class LDAPSearchResultEntry {
private:
    std::string dn;
    std::vector<LDAPPartialAttribute> attributes;
public:
    LDAPSearchResultEntry(const unsigned char *entry, size_t length) {}
};


class LDAPSearchResult {
private:
    std::vector<LDAPSearchResultEntry> entries;
public:
    LDAPSearchResult& addEntry(const unsigned char *entry, size_t length) {
        entries.push_back(LDAPSearchResultEntry(entry, length));
        return *this;
    }
};