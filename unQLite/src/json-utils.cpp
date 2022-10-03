#include "json-utils.h"
#include <algorithm>
#include <iostream>

using namespace std;

string escape_field_name(const string& s)
{
    string res;
    res.reserve(s.size());
    for (char c: s) {
        switch(c) {
            case '@':
                res+="@a"; break;
            case '.':
                res+="@d"; break;
            case ',':
                res+="@c"; break;
            case '{':
                res+="@l"; break;
            case '}':
                res+="@r"; break;
            case '[':
                res+="@L"; break;
            case ']':
                res+="@R"; break;
            case '/':
                res+="@s"; break;
            default:
                res.push_back(c);
        }
    }
    return res;
}

string unescape_field_name(const string& s)
{
    string res;
    size_t size = s.size();
    res.reserve(size);
    for (int i=0; i<size;++i) {
        char c = s[i];
        if (c=='@' && i<size-1) {
            ++i;
            switch(s[i]) {
                case 'a': c='@'; break;
                case 'd': c='.'; break;
                case 'c': c=','; break;
                case 'l': c='{'; break;
                case 'r': c='}'; break;
                case 'L': c='['; break;
                case 'R': c=']'; break;
                case 's': c='/'; break;
            }
        }
        res.push_back(c);
    }
    return res;
}


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
    fields.push_back(str);
}

void ObjectFieldSet::add(const ObjectFieldSet& o)
{
    for (const string& fld: o.fields) {
        add(fld);
    }
}

bool ObjectFieldSet::remove(const string& str)
{

    auto it = find(fields.begin(), fields.end(), str);
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

string valToString(const JSONValueP& val)
{
    if (val->IsString()) {
        return val->GetString();
    } else if (val->IsDouble()) {
        return to_string(val->GetDouble());
    } else if (val->IsInt64()) {
        return to_string(val->GetInt64());
    } else if (val->IsBool()) {
        return val->GetBool()?"true":"false";
    }

    return {};
}

double valToDouble(const JSONValueP& val)
{
    if (val->IsDouble()) {
        return val->GetDouble();
    } else if (val->IsInt64()) {
        return val->GetInt64();
    }
    return {};
}

int64_t valToInt(const JSONValueP& val)
{
    if (val->IsInt64()) {
        return val->GetInt64();
    } else if (val->IsDouble()) {
        return val->GetDouble();
    }
    return {};
}

bool valToBool(const JSONValueP& val)
{
    if (val->IsBool()) {
        return val->GetBool();
    } else if (val->IsInt64()) {
        return val->GetInt64()!=0;
    }
    return false;
}