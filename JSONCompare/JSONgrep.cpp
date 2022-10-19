#include "TraverseJSON.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <regex>
#include <filesystem>

using namespace std;
using namespace rapidjson;

bool readJSON(Document& json, const string& filename)
{
    FILE* fp = fopen(filename.c_str(), "r");
    if (!fp) {
        cerr<<"Error opening file: "<<filename<<endl;
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
        return false;
    }
    fclose(fp);
    return true;

}

void help()
{
  cerr<<"Syntax: JSONGrep [flags] <search-exp> <json-file>"<<endl;
  cerr<<"   Supported flags:"<<endl;
  cerr<<"   -report <regex>: report only key names that match regex"<<endl;
  cerr<<"   -ignore <regex>: skip keys that match regex"<<endl;
  cerr<<"   -context <regex>: when mismatch is reported, print fields in the same amendment that match regex"<<endl;
  cerr<<"   -if <path>:<string>: report results only for amendments where the key in <path> contains the substring <string>"<<endl;
  cerr<<"   -r: recursively traverse all JSON files in a directory"<<endl;
  cerr<<"   -q: quiet mode. Hide filename and path."<<endl;
  exit(1);
}

void handleFile(const string& filename, TraverseJSON& traversal, const string& s)
{
    //cerr<<"Grep filename: "<<filename<<endl;
    Document json;
    if (readJSON(json, filename)) {
        traversal.searchJSON(Path(""), json, s);
    }
}

void handleDirectory(const string& path, TraverseJSON& traversal, const string& s)
{
    for (auto& entry: std::filesystem::recursive_directory_iterator(path)) {
        filesystem::path p = entry.path();
        //cerr<<"Filename = "<<p<<" extension = "<<p.extension()<<endl;
        if (!entry.is_directory() && p.extension()==".json") {
            traversal.filename = p;
            handleFile(p, traversal, s);
        }
    }
    
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
  string json_filename;
  string s;
  bool recursive = false;
  if (argc<3) {
      help();
  }
  
  int i;
  for (i=1; i<argc; i++) {
    string arg = argv[i];
    if (arg=="-report") {
		traversal.opt_report_str = argv[++i];
        traversal.opt_report = regex(traversal.opt_report_str);
    } else if (arg=="-ignore") {
        traversal.opt_ignore = regex(argv[++i]);
    } else if (arg=="-context") {
        traversal.show_context = true;
        traversal.opt_context = regex(argv[++i]);
    } else if (arg=="-if") {
        traversal.opt_cond = argv[++i];
    } else if (arg=="-hide-path") {
        traversal.opt_hide_path = true;
    } else if (arg=="-q") {
        traversal.opt_hide_path = true;
        traversal.opt_hide_filename = true;
    } else if (arg=="-r") {
        recursive = true;
    } else if (arg[0]=='-') {
        cerr<<"Error: unknown option '"<<arg<<"'\n\n";
        help();
    } else {
        break;
    }
  }
  
  if (argc-i==2) {
      s= argv[i];
      json_filename = argv[i+1];
  } else if (argc-i==1) {
      json_filename = argv[i];
  } else {
      help();
  }
  
  if (recursive) {
    handleDirectory(json_filename, traversal, s);
  } else {
    handleFile(json_filename, traversal, s);
  }
  cout<<"\n  Total number of matches: "<<traversal.count_diffs[MISMATCH_T::FOUND_VALUE]<<endl;

  return 0;

}
