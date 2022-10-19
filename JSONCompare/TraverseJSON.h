#ifndef TRAVERSEJSON_H
#define TRAVERSEJSON_H

#include "rapidjson/document.h"
#include <string>
#include <regex>
#include <map>

static std::string kTypeNames[] = { "Null", "False", "True", "Object", "Array", "String", "Number" };

enum class MISMATCH_T {
    DIFFERENT_VALUE,
    FIELD_NOT_EXPECTED,
    FIELD_NOT_ACTUAL,
    AMENDMENT_NOT_EXPECTED,
    AMENDMENT_NOT_ACTUAL,
    FOUND_VALUE,
    NO_EXPECTED_FILE,
    NO_ACTUAL_FILE
};

class Path {
public:
    Path(const std::string& path, rapidjson::Value* obj = NULL, std::string key = std::string())
     : _path(path), _obj(obj), _key(key) {}
    const std::string& get() const {
        return _path;
    }
    const std::string& getKey() const {
        return _key;
    }
    rapidjson::Value* getObject() const {
        return _obj;
    }
    Path addKey(std::string key) const {
        return Path(_path+"/"+key, _obj, key);
    }
    Path addIndex(int i) const {
        return Path(_path+"["+std::to_string(i)+"]", _obj);
    }
    Path addObjectWithID(rapidjson::Value* v) const  {
        auto it = v->FindMember("id");
        if (it==v->MemberEnd() || !it->value.IsString()) {
            return Path(_path+"[id=#notfound]", v);
        }
        std::string id = it->value.GetString();
        return Path(_path+"[id="+id+"]", v);
    }

    std::string _path;
    std::string _key;
    rapidjson::Value* _obj;
};

class TraverseJSON
{
public:
  void searchJSON(const Path& path, rapidjson::Value& value, const std::string& search_str);
  void compareBill(rapidjson::Value& expected, rapidjson::Value& actual);
  void compareValues(const Path& path, rapidjson::Value& expected, rapidjson::Value& actual);
  void compareAmendments(const Path& path, rapidjson::Value& expected, rapidjson::Value& actual);
  void compareGenericJson(rapidjson::Value& expected, rapidjson::Value& actual);
  void report(MISMATCH_T type, const Path& path, const std::string& key, const std::string& msg);
  void report_summary();
  bool testCond(rapidjson::Value& value, const std::string& cond);
  
  
  bool opt_any = false;
  std::regex opt_report = std::regex(".*");
  std::string opt_report_str;
  std::regex opt_ignore;
  std::regex opt_context;
  std::string opt_cond;
  bool opt_hide_path = false;
  bool opt_hide_filename = false;
  bool show_context = false;
  std::map<MISMATCH_T, int> count_diffs;
  std::string filename;
};


#endif //TRAVERSEJSON_H
