#ifndef TEMPLATEPARSER_H
#define TEMPLATEPARSER_H

#include "TemplateQuery.h"

namespace xcite {

class ParsingError
{
public:
    ParsingError(string msg_): msg(msg_) {}

    virtual string message() {
        return "Error parsing Q! Query: "+msg;
    }

    string msg;
};

typedef std::pair<TemplateQueryP, TQConditionP> ValueAndCond;

typedef std::pair<TQKeyP, TemplateQueryP> KeyAndContext;

enum class ContextModMode {
    None,
    Start,
    OrStart,
    Or
};

class TParser
{
public:

    TParser(const std::string& str);

    bool eos() const;

    std::string nextToken(bool consume = true);
    void consume();
    void expect(const std::string& expected);
    bool lookAhead(const std::string& s);
    bool ifNext(const std::string& s);
    void throwError(const std::string& msg);

    TQKeyP key();
    TemplateQueryP context_mod(bool frame_flag = true, ContextModMode parse_mode = ContextModMode::None);
    ValueAndCond value();
    TQConditionP baseCondition(const TExpressionP& arg1 = {});
    TQConditionP condition(int prec = 0, const TExpressionP& exp = {});
    TExpressionP expression(int prec = 0);
    TExpressionP baseExpression();
    string pathId();
    string pathWithBrackets(const std::string& token);
    void pushPosition();
    void popPosition();
    void restorePosition(); 

private:
    std::string _str;
    size_t _pos;
    size_t _len;
    std::vector<size_t> _positions;

};

TemplateQueryP JSONToTQ(JSONValue& v);

} // namespace xcite

#endif //TEMPLATEPARSER_H
