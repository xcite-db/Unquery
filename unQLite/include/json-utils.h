#include <string>
#include <vector>
#include <map>

class ObjectFieldSet
{
public:
    ObjectFieldSet() {}
    ObjectFieldSet(const std::string& str);
    void add(const std::string& s);
    void add(const ObjectFieldSet& o);
    bool remove(const std::string& s);
    std::string toString() const;
    bool operator==(const ObjectFieldSet& o) const {
        return fields==o.fields;
    }
    auto begin() {
        return fields.begin();
    }
    auto end() {
        return fields.end();
    }
private:
    std::vector<std::string> fields;
    std::map<std::string,int> fields_map;
};
