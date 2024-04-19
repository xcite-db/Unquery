// Copyright (c) 2022 by Sela Mador-Haim

#include "params.h"
#include "shared/version.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/stringbuffer.h"
#include <rapidjson/writer.h>
#include "rapidjson/error/en.h"
#include "TemplateParser.h"
#include "TemplateQuery.h"
#include "rapidjson/prettywriter.h"
#include <iostream>
#include <fstream>
#include <string>
#include <memory>

using namespace std;
using namespace rapidjson;
using namespace xcite;

typedef std::shared_ptr<rapidjson::Document> json_documentP;

void process_json_file(TQDataP& tq, TQContext& ctx, Params& args, bool use_stdin)
{
    string filename;
    //cerr<<"Filename: "<<filename<<endl;
    FILE* fp;
    if (use_stdin) {
        fp = stdin;
        filename = "stdin";
    } else {
        filename = args.nextArg();
        fp = fopen(filename.c_str(), "r");
        if (!fp) {
            cerr<<"Error. Could not open JSON file: "<<filename<<endl;
            exit(1);
        }

    }
    char readBuffer[65536];
    FileReadStream jsonfile(fp, readBuffer, sizeof(readBuffer));
    while (jsonfile.Peek()!=EOF) {
        JSONValueP json_val(new Document);
        Document& json_doc = *static_cast<Document*>(json_val.get());
        json_doc.ParseStream<kParseCommentsFlag|kParseStopWhenDoneFlag>(jsonfile);
        if (json_doc.HasParseError()) {
            if (json_doc.GetParseError()==kParseErrorDocumentEmpty) {
                break;
            }
            cerr<<"Not a valid JSON\n";
            cerr<<"Error(offset "<<static_cast<unsigned>(json_doc.GetErrorOffset())<<"): "<<GetParseError_En(json_doc.GetParseError())<<endl;
            exit(EXIT_FAILURE);
        }
        try {
            ctx.reset({}, {});
            ctx.startLocalJSON(json_val);
            ctx.pushFilename(filename);
            tq->processData(ctx);
            ctx.popFilename();
        } catch (QueryError& e) {
            cerr<<"In file: "<<filename<<", ";
            cerr<<e.message()<<endl;
            exit(1);
        }
    }
    fclose(fp);
}

void process_csv_file(
    TQDataP& tq, 
    TQContext& ctx, 
    Params& args, 
    const string& delim, 
    bool with_headers, 
    bool use_stdin)
{
    JSONValueP json;
    string file_name;
    if (use_stdin) {
        file_name = "stdin";
        json = readCSV(cin, delim, with_headers);
    } else {
        file_name = args.nextArg();
        ifstream is(file_name);
        if (is.fail()) {
            cerr<<"Error: failed opening CSV file: "<<file_name<<endl;
            exit(1);
        }
        json = readCSV(is, delim, with_headers);
        try {
            ctx.reset({}, {});
            ctx.startLocalJSON(json);
            ctx.pushFilename(file_name);
            tq->processData(ctx);
            ctx.popFilename();
        } catch (QueryError& e) {
            cerr<<"In file: "<<file_name<<", ";
            cerr<<e.message()<<endl;
            exit(1);
        }
    }
    
}

void print_help_message(int exit_code)
{
    cerr<<"(c) 2024 Sela Mador-Haim All rights Reserved.\n\n";
    cerr<<"unq Version: v"<<get_version()<<endl<<endl;
    cerr<<"Command-line syntax:\n";
    cerr<<"  unq [options] <json-file-list>\n\n";
    cerr<<"Options:\n";
    cerr<<"  -c <query-string>: query as string in the command line.\n";
    cerr<<"  -f <query-file>: a filename containing the query.\n";
    cerr<<"  -show-nulls (or -n): do not hide null values.\n";
    cerr<<"  -csv: input files as csv files, instead of json.\n";
    cerr<<"  -delim <delimiter>: a character (or string) used as a delimiter for csv files.\n";
    cerr<<"  -csv-no-headers: the csv file contains no headers in the first line.\n";
    cerr<<endl;
    exit(exit_code);
}

int main(int argc, char* argv[])
{
    Params args(argc, argv);
    string query_file;
    string query_txt;
    bool csv_opt = false;
    string delim = ",";
    bool csv_headers_opt = true;
    bool show_nulls_opt = false;

    while (!args.isEnd() && args.isOpt()) {
        string arg = args.nextArg();
        if (arg=="-f") {
            query_file = args.nextArg();
        } else if (arg=="-c") {
            query_txt = args.nextArg();
        } else if (arg=="-show-nulls" || arg=="-n") {
            show_nulls_opt = true;
        } else if (arg=="-csv") {
            csv_opt = true;
        } else if (arg=="-csv-no-headers") {
            csv_headers_opt = false;
        } else if (arg=="-delim") {
            delim = args.nextArg();
        } else if (arg=="-h") {
            print_help_message(0);
        } else {
            cerr<<"Error: unknown option "<<arg<<endl<<endl;
            print_help_message(1);
        }
    }
    if (query_file.empty()==query_txt.empty()) {
        cerr<<"Error: must specify either -f or -c (but not both).\n\n";
        print_help_message(1);
    }

    Document json_query;
    if (!query_file.empty()) {
        FILE* fp = fopen(query_file.c_str(), "r");
        if (!fp) {
            cerr<<"Error. Could not open query file: "<<query_file<<endl;
            exit(1);
        }
        char readBuffer[65536];
        FileReadStream jsonfile(fp, readBuffer, sizeof(readBuffer));
        json_query.ParseStream<kParseCommentsFlag>(jsonfile);
        fclose(fp);
        if (json_query.HasParseError()) {
            cerr<<"Not a valid JSON\n";
            cerr<<"Error(offset "<<static_cast<unsigned>(json_query.GetErrorOffset())<<"): "<<GetParseError_En(json_query.GetParseError())<<endl;
            exit(EXIT_FAILURE);
        }
    } else {
        json_query.Parse(query_txt.c_str());
        if (json_query.HasParseError()) {
            cerr<<"Not a valid JSON\n";
            cerr<<"Error(offset "<<static_cast<unsigned>(json_query.GetErrorOffset())<<"): "<<GetParseError_En(json_query.GetParseError())<<endl;
            exit(EXIT_FAILURE);
        }
    }

    try {
        TemplateQueryP t = JSONToTQ(json_query);
        TQDataP tq = t->makeData();
        TQContext ctx;
        ctx.opt_show_null = show_nulls_opt;
        bool use_stdin = args.isEnd();
        while (!args.isEnd() || use_stdin) {
            if (csv_opt) {
                process_csv_file(tq, ctx, args, delim, csv_headers_opt, use_stdin);
            } else {
                process_json_file(tq, ctx, args, use_stdin);
            }

            use_stdin = false;
//
        }
        ctx.in_get_JSON = true;
        JSONValue value = tq->getJSON(ctx);
        ctx.in_get_JSON = false;
        (JSONValue&)(*ctx.doc) = value;

        StringBuffer sb;
        PrettyWriter<StringBuffer> writer(sb);
        ctx.doc->Accept(writer);    // Accept() traverses the DOM and generates Handler events.
        cout<<sb.GetString();
    } catch (ParsingError& e) {
        cerr<<e.message()<<endl;
        exit(1);
    }

    return 0;
}