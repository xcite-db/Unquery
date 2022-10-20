#include "TraverseJSON.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/error/en.h"
#include <fstream>
#include <iostream>
#include <filesystem>

using namespace std;
using namespace rapidjson;

bool readJSON(Document& json, const string& filename)
{
    FILE* fp = fopen(filename.c_str(), "r");
    if (!fp) {
        return false;
    }

    char readBuffer[65536];
    FileReadStream jsonfile(fp, readBuffer, sizeof(readBuffer));
    json.ParseStream(jsonfile);
    if (json.HasParseError()) {
        fprintf(stderr, "Schema file '%s' is not a valid JSON\n", filename.c_str());
        fprintf(stderr, "Error(offset %u): %s\n",
            static_cast<unsigned>(json.GetErrorOffset()),
            GetParseError_En(json.GetParseError()));
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    fclose(fp);
    return true;
}

void handleFile(const string& expected_filename, const string& actual_filename, TraverseJSON& traversal)
{
  //cerr<<"Comapre expected="<<expected_filename<<" to actual: "<<actual_filename<<endl;
  Document expected;
  Document actual;
  if (!readJSON(expected, expected_filename)) {
      cout<<"Expected file: "<<expected_filename<<" not found.\n";
      traversal.count_diffs[MISMATCH_T::NO_EXPECTED_FILE]++;
      return;
  }
  if (!readJSON(actual, actual_filename)) {
      cout<<"Actual file: "<<actual_filename<<" not found.\n";
      traversal.count_diffs[MISMATCH_T::NO_ACTUAL_FILE]++;
      return;
  }
  if (!expected.IsObject() || !actual.IsObject()) {
    traversal.compareGenericJson(expected, actual);
    return;
  }
  Value::MemberIterator expected_it = expected.FindMember("_embedded");
  if (expected_it == expected.MemberEnd()) {
    expected_it = expected.FindMember("bills");
  }
  Value::MemberIterator actual_it = actual.FindMember("_embedded");
  if (actual_it == actual.MemberEnd()) {
    actual_it = actual.FindMember("bills");
  }
  if (actual_it==actual.MemberEnd() && expected_it==expected.MemberEnd()) {
    traversal.compareBill(expected, actual);
    return;
  } 
  if (actual_it==actual.MemberEnd() || expected_it==expected.MemberEnd()) {
    traversal.compareGenericJson(expected, actual);
    return;
  }
  auto expected_bills = expected_it->value.GetArray();
  auto actual_bills = actual_it->value.GetArray();
  if (expected_bills.Size()!=actual_bills.Size()) {
    cerr<<"Error: different number of bills\n";
    return;
  }
  for (int i=0; i<expected_bills.Size(); i++) {
    traversal.compareBill(expected_bills[i], actual_bills[i]);
  }

}

void handleDirectory(const string& expected_path, const string& actual_path, TraverseJSON& traversal)
{
    for (auto& entry: std::filesystem::recursive_directory_iterator(actual_path)) {
        filesystem::path p = entry.path();
        //cerr<<"Filename = "<<p<<" extension = "<<p.extension()<<endl;
        if (!entry.is_directory() && p.extension()==".json") {
            string actual = p;
            string base = actual.substr(actual_path.size());
            string expected = expected_path+base;
            traversal.filename = base;
            handleFile(expected, actual, traversal);
        }
    }
    
}


void help()
{
  cerr<<"Syntax: JSONCompare [flags] <expected> <actual>"<<endl;
  cerr<<"   Supported flags:"<<endl;
  cerr<<"   -report <regex>: report only key names that match regex"<<endl;
  cerr<<"   -ignore <regex>: skip keys that match regex"<<endl;
  cerr<<"   -context <regex>: when mismatch is reported, print fields in the same amendment that match regex"<<endl;
  cerr<<"   -r: recursively traverse directories, and compare files with matching names"<<endl;
  exit(1);
}

int main(int argc, char* argv[])
{
/*
  if (argc!=3) {
    cerr<<"argc="<<argc<<" Syntax: JSONCompare <expected> <actual>"<<endl;
    return 1;
  }
  */
  TraverseJSON traversal;
  string expected_filename;
  string actual_filename;
  bool recursive = false;
  if (argc<3) {
      help();
  }
  
  for (int i=1; i<argc; i++) {
    string arg = argv[i];
    if (arg=="-report") {
		traversal.opt_report_str = argv[++i];
        traversal.opt_report = regex(traversal.opt_report_str);
    } else if (arg=="-ignore") {
        traversal.opt_ignore = regex(argv[++i]);
    } else if (arg=="-context") {
        traversal.show_context = true;
        traversal.opt_context = regex(argv[++i]);
    } else if (arg=="-r") {
        recursive = true;
    } else {
        if (argc-i!=2 || argv[i][0]=='-' || argv[i+1][0]=='-') {
            help();
        } else {
            expected_filename = argv[i];
            actual_filename = argv[i+1];
            break;
        }
    }
  }
  if (recursive) {
      handleDirectory(expected_filename, actual_filename, traversal);
  } else {
    traversal.filename = actual_filename;
    handleFile(expected_filename, actual_filename, traversal);
  }
  traversal.report_summary();

  return 0;

}
