# unqery: a command line tool for querying and transforming JSON data

## About

The query language `Unquery` is a fast, powerful and flexible language for querying structured documents. It was originally developed as the query language for [XCiteDB](http://www.xcitedb.com), a fast and reliable noSQL database for structured documents, supporting XML, JSON, and other features such as temporal versioning and branches. 

This repository contains *unq*, a stand-alone command-line tool for querying, transforming and analyzing JSON files using `Unquery`. It reads either a single or multiple JSON files and an Unquery query, and produce a single JSON output based on the query.

## Building unq

To build the tool, you'll need cmake version 3.6.2 or higher. You can install it with the package manager on most Linux distros, or follow the directions [here](https://cmake.org/install/).

To build
```shell
cd unq
cmake .
make
```

The executable would be in bin/Release/unq.

## Running unq

To run `unq`, you'll need to provide the query, either as a file (with the option `-f`) or as a command-line option (with the option `-c`), and a list of files to query. For example, if you want to collect the value of the field `firstName` from all the json files in the current directory, the query you need is:
```
["firstName"]
```

You can store the above query in a file, say `query.unq`, and then run:
```
unq -f query.unq *.json
```

Or alternatively, you can write:
```
unq -c '["firstName"]' *.json
```

It also allows to query CSV files by converting them on-the-fly to JSON and then processing the query. Example for a command line for processing CSV:

```
unq -f query.unq -csv -delim ";" users.csv
```

## Frequently Asked Questions?

### Why do we need another json query language?

*There already over a dozen different quey languages for json. Why do we need yet another one?*

While there are a lot of query languages for json, Unquery is different. And, we believe, better. The approach we used for designing Unquery is based on two principles:
* Any Unquery query is a well-formed json.
* The structure of the query matches the structure of the result.

The advantages of Unquery are:
* Readability: because te structure of the query matches the structure of the result, it is easy to unserstand what the query does.
* Succint: complex queries can be writen using relatively shot queries.
* Simple: there are only few constructs the user need to learn to write complex queries.
* Powerfull: we do not compromise on the expressive power of the language. 
* Recursive structure: json is recursive, and allows unlimited nesting depth of objects and arrays. Similarly, Unquery is capable of querying nested arrays and objects with  ease.
* Guarantee to produce well-formed json.
* Fully embrace json: query json files with queries written in json, and results in json.
* Fast. The implementation is written in C++, and use RapidJSON, the fastest c++ json library, to parse json files.
* The same query language is used in XCiteDB, a powerfull database for XML and JSON.

### Are there any disadvantages?

*What are the disadvantages of Unquery, compared to other query languages?*

Unquery does have some disadvantages as well:
* The queries are written in json, and may come with some syntactic overhead. Mostly, lots of double quotes, which are required for strings.
* It can only produce well-formed json. So if you need non-json results, it wouldn't be a good match.
* The implmentation is in C++ (and not Javascript). So it cannot run on the client's side inside the browser.

### Is the implementation complete?

*It looks like unq is still being developped. Is it ready to be used?*

The implementation is fully working. All the features shown in the tutorial (and many more) are already implemented. It is still a beta version, and may still have bugs. The more you use it, the better it would become.

### What about documentation?

*Where is the reference manual?*

The full language manual is at here: [Language Reference](https://github.com/xcite-db/Unquery/blob/main/LANGUAGE-REFERENCE.adoc)

Also, check out the [tutorial](https://github.com/xcite-db/Unquery/blob/main/TUTORIAL.adoc) for more examples.

### Other programming languages

*Having a command line utility is cool. But I want to use it in a Python/NodeJS/C++ project. Do you plan to have language bindings for those languages?*

Yes.

### License

*Can I use it? Do I need to pay?*

unq is an open source project, disctributed under the terms Apache 2.0. So it is completely free.
