# Unquery Lite

## About

The query language `~Q` (pronounced "unquery) is a fast, powerful and flexible language for querying structured documents. It was originally developed as the query language for [XCiteDB](http://www.xcitedb.com), a fast and reliable noSQL database for structured documents, supporting XML, JSON, and other features such as temporal versioning and branches. 

This repository contains *unquery-lite*, a stand-alone command-line tool for querying, transforming and analyzing JSON files using `~Q`. It reads either a single or multiple JSON files and a ~Q query, and produce a single JSON output based on the query.

## Building unquery-lite

To build the tool, you'll need cmake version 3.6.2 or higher. You can install it with the package manager on most Linux distros, or follow the directions [here](https://cmake.org/install/).

To build
```shell
cd unQLite
cmake .
make
```

The executable would be in bin/Release/unQLite.

## Running unQLite

To run `unQLite`, you'll need to provide the query, either as a file (with the option `-f`) or as a command-line option (with the option `-c`), and a list of files to query. For example, if you want to collect the value of the field `firstName` from all the json files in the current directory, the query you need is:
```
["firstName"]
```

You can store the above query in a file, say `query.unq`, and then run:
```
unQLite -f query.unq *.json
```

Or alternatively, you can write:
```
unQLite -c '["firstName"]' *.json
```

