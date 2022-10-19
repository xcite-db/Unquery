#include "TraverseJSON.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include <fstream>
#include <iostream>
#include <unordered_map>

using namespace std;
using namespace rapidjson;


void bad_format()
{
  cerr<<"Error: either expected or actual not in the correct format.\n";
  cerr<<"Use -any to do generic compare of JSON files.\n";
  exit(1);

}

void print_match(const Path& path, const Value& value, const regex& match)
{
    if (regex_search(path.get(), match)) {
        if (value.IsString()) {
            cout<<"  "<<path.get()<<": "<<value.GetString()<<endl;
        } else if (value.IsInt()) {
            cout<<"  "<<path.get()<<": "<<value.GetInt()<<endl;
        }
    }
    if (value.IsObject()) {
        for (auto& m: value.GetObject()) {
            string key = m.name.GetString();
            print_match(path.addKey(key), m.value, match);
        }
    } else if (value.IsArray()) {
        auto arr = value.GetArray();
        for (int i=0; i<arr.Size(); i++) {
            print_match(path.addIndex(i), arr[i], match);
        }
    }
}

void TraverseJSON::report(MISMATCH_T type, const Path& path, const string& key, const string& msg)
{
    if (path.get().find(opt_report_str)==string::npos && !regex_search(path.get(), opt_report)) {
        return;
    }
    count_diffs[type]++;
    if (!opt_hide_filename && !filename.empty()) {
        cout<<filename<<": ";
    }
    if (!opt_hide_path) {
        cout<<"In "<<path.get()<<": ";
    }
    cout<<msg<<endl;
    if (show_context && path.getObject()) {
        cout<<"Context:\n";
        print_match(Path("..."), *path.getObject(), opt_context);
    }
}

void TraverseJSON::report_summary()
{
    cout<<endl<<endl<<"Summary: "<<endl;
    cout<<"  Fields with different values: "<<count_diffs[MISMATCH_T::DIFFERENT_VALUE]<<endl;
    cout<<"  Fields in expected, but missing in actual: "<<count_diffs[MISMATCH_T::FIELD_NOT_ACTUAL]<<endl;
    cout<<"  Fields in actual, but not in expected: "<<count_diffs[MISMATCH_T::FIELD_NOT_EXPECTED]<<endl;
    cout<<"  Amentments in expected, but missing in actual: "<<count_diffs[MISMATCH_T::AMENDMENT_NOT_ACTUAL]<<endl;
    cout<<"  Amendments in actual, but not in expected: "<<count_diffs[MISMATCH_T::AMENDMENT_NOT_EXPECTED]<<endl;
    int expected_files = count_diffs[MISMATCH_T::NO_EXPECTED_FILE];
    if (expected_files) {
        cout<<"  Expected files missing: "<<expected_files<<endl;
    }
    int actual_files = count_diffs[MISMATCH_T::NO_ACTUAL_FILE];
    if (actual_files) {
        cout<<"  Actual files missing: "<<actual_files<<endl;
    }
    
}

void TraverseJSON::compareGenericJson(Value& expected, Value& actual)
{
    if (!opt_any) {
        bad_format();
    }
    compareValues(Path(""), expected, actual);
}

void TraverseJSON::compareBill(Value& expected, Value& actual)
{
    string exp_filename;
    auto it = expected.FindMember("filename");
    if (it!=expected.MemberEnd()) {
        exp_filename = it->value.GetString();
        //cout<<"Filename = "<<exp_filename<<endl;
    }

    compareValues(exp_filename, expected, actual);

}

void TraverseJSON::compareValues(const Path& path, Value& expected, Value& actual)
{
    if (expected==actual) {
        return;
    }
    if (expected.GetType()!=actual.GetType()) {
        report(MISMATCH_T::DIFFERENT_VALUE, path, path.getKey(), "Different types. Expected:"+kTypeNames[expected.GetType()]+" Actual:"+kTypeNames[actual.GetType()]);
        return;
    }
    if (expected.IsString()) {
        string expval = expected.GetString();
        string actval = actual.GetString();
        if (expval!=actval) {
            report(MISMATCH_T::DIFFERENT_VALUE, path, path.getKey(), "String value differs.\n  Expected="+expval+"\n  Actual=  "+actval);
        }
    } else if (expected.IsInt()) {
        int expval = expected.GetInt();
        int actval = actual.GetInt();
        if (expval!=actval) {
            report(MISMATCH_T::DIFFERENT_VALUE, path, path.getKey(), "Integer value differs. Expected="+to_string(expval)+" Actual="+to_string(actval));
        }
    } else if (expected.IsObject()) {
        for (auto& m: expected.GetObject()) {
            string key = m.name.GetString();
            if (regex_match(key,opt_ignore)) {
                continue;
            }
            auto it = actual.FindMember(key.c_str());
            if (it==actual.MemberEnd()) {
                report(MISMATCH_T::FIELD_NOT_ACTUAL, path.addKey(key), key, "Field \""+key+"\" in expected not found in actual");
            } else if (key=="amendments") {
                compareAmendments(path, m.value, it->value);
            } else {
                compareValues(path.addKey(key), m.value, it->value);
            }
        }
        for (auto& m: actual.GetObject()) {
            string key = m.name.GetString();
            if (regex_match(key,opt_ignore)) {
                continue;
            }
            auto it = expected.FindMember(key.c_str());
            if (it==expected.MemberEnd()) {
                report(MISMATCH_T::FIELD_NOT_EXPECTED, path.addKey(key), key, "Field \""+key+"\" in actual not found in expected");
            }
        }

        //cout<<"Getting into field: "<<key<<endl;
    } else if (expected.IsArray()) {
        auto exp_array = expected.GetArray();
        auto act_array = actual.GetArray();
        if (exp_array.Size()!=act_array.Size()) {
            report(MISMATCH_T::DIFFERENT_VALUE, path, path.getKey(), "Expected array of size:"+to_string(exp_array.Size())+" actual size:"+to_string(act_array.Size()));
        } else {
            for (int i=0; i<exp_array.Size();i++) {
                compareValues(path.addIndex(i), exp_array[i], act_array[i]);
            }
        }
    }


}

void TraverseJSON::compareAmendments(const Path& path, Value& expected, Value& actual)
{
    auto exp_array = expected.GetArray();
    auto act_array = actual.GetArray();
    unordered_map<string, Value> expected_ids;
    unordered_map<string, Value> actual_ids;
    for (auto& m:exp_array) {
        auto it = m.FindMember("id");
        if (it==m.MemberEnd()) {
            continue;
        }
        string id = it->value.GetString();
        expected_ids[id] = m;
    }
    for (auto& m:act_array) {
        auto it = m.FindMember("id");
        if (it==m.MemberEnd()) {
            continue;
        }
        string id = it->value.GetString();
        auto exp_it = expected_ids.find(id);
        if (exp_it!=expected_ids.end()) {
            compareValues(path.addObjectWithID(&m), exp_it->second,m);
        } else {
            report(MISMATCH_T::AMENDMENT_NOT_EXPECTED, path, path.getKey(), "Amendment id:"+id+" in actual, but not in expected");
        }
        actual_ids[id] = m;
    }
    for (auto& m: expected_ids) {
        const string& id = m.first;
        if (actual_ids.find(id)==actual_ids.end()) {
            report(MISMATCH_T::AMENDMENT_NOT_ACTUAL, path, path.getKey(), "Amendment id:"+id+" in expected, but not in actual");
        }
    }

}

bool TraverseJSON::testCond(Value& value, const string& cond)
{
    size_t spos = 0;
    size_t epos = 0;
    Value* v = &value;
    while ((epos = cond.find_first_of("/:",spos))!=string::npos) {
        string key = cond.substr(spos, epos-spos);
        spos = epos+1;
        if (!v->IsObject()) {
            return false;
        }
        v = &v->FindMember(key.c_str())->value;
    }
    if (spos>0 &&cond[spos-1]==':' && v->IsString()) {
        string text = v->GetString();
        string val = cond.substr(spos);
        if (text.find(val)!=string::npos) {
            return true;
        }
    }
    return false;
}

void TraverseJSON::searchJSON(const Path& path, rapidjson::Value& value, const std::string& search_str)
{
    if (value.IsString()) {
        string txt = value.GetString();
        if (txt.find(search_str)!=string::npos) {
            report(MISMATCH_T::FOUND_VALUE, path, path.getKey(), txt);
        }
    } else if (value.IsObject()) {
        for (auto& m: value.GetObject()) {
            string key = m.name.GetString();
            if (regex_match(key,opt_ignore)) {
                continue;
            }
            searchJSON(path.addKey(key), m.value, search_str);
        }
    } else if (value.IsArray()) {
        auto array = value.GetArray();
        for (int i=0; i<array.Size();i++) {
            auto& m = array[i];
            if (m.IsObject()) {
                auto it = m.FindMember("filename");
                if (it!=m.MemberEnd()) {
                    string filename = it->value.GetString();
                    searchJSON(filename, m, search_str);
                    continue;
                }
            }
            if (path.getKey()=="amendments") {
                if (opt_cond.empty() || testCond(m, opt_cond)) {
                    searchJSON(path.addObjectWithID(&m), m, search_str);
                }
            } else {
                searchJSON(path.addIndex(i), m, search_str);
            }
        }
    }
}
