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
#include <string>
#include <memory>

using namespace std;
using namespace rapidjson;
using namespace xcite;

typedef std::shared_ptr<rapidjson::Document> json_documentP;

void print_help_message(int exit_code)
{
    cerr<<"(c) 2022 Sela Mador-Haim All rights Reserved.\n\n";
    cerr<<"unQLite Version: v"<<get_version()<<endl<<endl;
    cerr<<"Command-line syntax:\n";
    cerr<<"  unQLite [-c <query-string>] [-f <query-file>] <json-file-list>\n\n";
    exit(exit_code);
}

int main(int argc, char* argv[])
{
    Params args(argc, argv);
    string query_file;
    string query_txt;

    while (!args.isEnd() && args.isOpt()) {
        string arg = args.nextArg();
        if (arg=="-f") {
            query_file = args.nextArg();
        } else if (arg=="-c") {
            query_txt = args.nextArg();
        } else if (arg=="-h") {
            print_help_message(0);
        }
    }
    if (query_file.empty()==query_txt.empty()) {
        cerr<<"Error: must specify either -f or -c (but not both).\n\n";
        print_help_message(1);
    }

    Document json_query;
    if (!query_file.empty()) {
        FILE* fp = fopen(query_file.c_str(), "r");
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
        JSONValueP json_val(new Document);
        Document& json_doc = *static_cast<Document*>(json_val.get());
        while (!args.isEnd()) {
            string filename = args.nextArg();
            //cerr<<"Filename: "<<filename<<endl;
            FILE* fp = fopen(filename.c_str(), "r");
            char readBuffer[65536];
            FileReadStream jsonfile(fp, readBuffer, sizeof(readBuffer));
            json_doc.ParseStream<kParseCommentsFlag>(jsonfile);
            fclose(fp);
            if (json_doc.HasParseError()) {
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