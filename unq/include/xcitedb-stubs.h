// Copyright (c) 2022 by Sela Mador-Haim

#include <string>
#include "rapidjson/document.h"
#include "json-utils.h"
#include <set>
#include <ostream>

typedef std::string string;
typedef rapidjson::Value JSONValue;

class WhatEver
{
};

typedef int MDB_txn;

namespace pugi {
    const int node_pcdata = 0;
    class xml_attribute
    {
    public:
        bool empty() {return false;}
        char * value() {return 0;}
    };

    class xml_node 
    {
    public:
        string name() const {return {};}
        xml_node first_child() {return xml_node();}
        xml_node child(const char*) const {return {};}
        std::set<xml_node> children() const {return {};}
        bool empty() const {return false;}
        const char* value() const {return 0;}
        int type() const {return 0;}

        void print(std::ostream& os) {}
        xml_attribute attribute(const char *) {return {};}
    };

    class xml_document: public xml_node
    {
    };
}

class DocStructure
{
public:
    bool isInFilter(const string& s1, const string& s2) {return true;}
};

class XMLDBSettings
{
};

class SimpleQuery
{
public:
    SimpleQuery(MDB_txn* x, MDB_txn* y) {}
    void beginQuery() {}
    bool readyForNext() {return true;}
    string getNextIdentifier() {return {};}
    
    string match_start;
};


class XMLReader
{
public:
    XMLReader(XMLDBSettings& settings, MDB_txn* txn) {}
    string getNodeFromIdentifier(const string& identifier) {return {};}
    MDB_txn* get_txn() {return 0;}
    MDB_txn* get_base_txn() {return 0;}
    bool getNodeXML(const string& s, pugi::xml_document& doc) {return true;}
    void addNamespace(pugi::xml_node& n, const string& s) {}
    void traverse(pugi::xml_node& n) {}
    DocStructure doc_conf() {return DocStructure();}
    bool no_children;
};

class JSONMetaReader
{
public:
    JSONMetaReader(XMLDBSettings& settings_, MDB_txn* txn_, const std::string& node) {}
    void changeNode(const std::string& node) {}
    std::string readMetaNode(const std::string& path) {return {};}
    JSONValue traverse(const string& path, rapidjson::Document& json) {return {};}
};

inline std::string getMainIdPart(const std::string& id, int parts = 0) {return {};}
