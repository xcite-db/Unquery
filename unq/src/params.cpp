// Copyright (c) 2022 by Sela Mador-Haim

#include "params.h"

using namespace std;

namespace xcite {

Params::Params(int argc, char* argv[])
{
    for (int i =0; i<argc; i++) {
        _args.push_back(argv[i]);
    }
}

string Params::nextArg()
{
    return _args[i++];
}

bool Params::isOpt()
{
    return _args[i][0]=='-';
}

} // namespace xcite
