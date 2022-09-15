#include "json-utils.h"

using namespace std;

ObjectFieldSet::ObjectFieldSet(const string& str)
{
    if (str.empty() || str[0]!='{') {
        return;
    }
    size_t pos = 1;
    while (pos<str.size()-1) {
        size_t pos_e = str.find(",",pos);
        if (pos_e==string::npos) {
            pos_e = str.size()-1;
        }
        string fld = str.substr(pos, pos_e-pos);
        add(fld);
        pos = pos_e+1;
    }
}

void ObjectFieldSet::add(const string& str)
{
    fields.insert(str);
}

void ObjectFieldSet::add(const ObjectFieldSet& o)
{
    for (const string& fld: o.fields) {
        add(fld);
    }
}

bool ObjectFieldSet::remove(const string& str)
{
    auto it = fields.find(str);
    if (it==fields.end()) {
        return false;
    }
    fields.erase(it);
    return true;
}

string ObjectFieldSet::toString() const
{
    string res = "{";
    bool first = true;
    for (const string& fld: fields) {
        if (!first) {
            res+=",";
        }
        first = false;
        res+=fld;
    }
    res+="}";
    return res;
}
