#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "rapidjson/document.h"

typedef rapidjson::Value JSONValue;
typedef std::shared_ptr<JSONValue> JSONValueP;

std::string escape_field_name(const std::string& s);
std::string unescape_field_name(const std::string& s);


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
};

std::string valToString(const JSONValueP& val);

double valToDouble(const JSONValueP& val);

int64_t valToInt(const JSONValueP& val);

bool valToBool(const JSONValueP& val);

JSONValueP readCSV(std::istream& is, const std::string& delim, bool with_header);

#endif // JSON_UTILS_H
