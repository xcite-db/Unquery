// Copyright (c) 2022 by Sela Mador-Haim

#ifndef PARAMS_H
#define PARAMS_H

#include <string>
#include <vector>

namespace xcite {

class Params {
public:
    Params(int argc, char* argv[]);
    std::string nextArg();
    bool isOpt();
    bool isEnd() {
        return i>=_args.size();
    }
    
    std::vector<std::string> _args;
    int i = 1;
};

} // namespace xcite

#endif //PARAMS_H
