// Copyright (c) 2022 by Sela Mador-Haim

#include "TemplateParser.h"
#include "utils.h"
#include <iostream>

using namespace std;

namespace xcite {

TParser::TParser(const string& str)
    : _str(str), _len(str.size()), _pos(0)
{
}

bool isalnum_(char c)
{
    return isalnum(c)|| c=='_';
}

bool isQuoted(const string& s)
{
    if (s.empty()) {
        return false;
    }
    return (s.front()=='\"' && s.back()=='\"') || 
           (s.front()=='\'' && s.back()=='\'') ||
           (s.front()=='`' && s.back()=='`');
}

bool isnumber(const string& s)
{
    int point = 0;
    bool digits = false;
    for (char c: s) {
        if (c=='.') {
            point++;
        } else if (isdigit(c)) {
            digits = true;
        } else {
            return false;
        }
    }
    return digits&&point<=1;
}

string stripQuotes_(const string& s)
{
    if (isQuoted(s)) {
        return s.substr(1, s.size()-2);
    }
    return s;
}

bool TParser::eos() const
{
    return _pos==_len;
}


string TParser::nextToken(bool consume)
{
    static const string single_char = "()[]{};,.#?!:$";

    size_t i = _pos;
    while (i<_len && isspace(_str[i])) {
        i++;
    }
    size_t start = i;
    char c = _str[i];
    if (isdigit(c)) {
        while (i<_len && (isdigit(c=_str[++i])||c=='.')) {}
        if (isalnum_(c)) {
            while (i<_len && isalnum_(_str[++i])) {}
        }
    } else if (isalnum_(c) || c=='$') {
        while (i<_len && isalnum_(_str[++i])) {}
    } else if (c=='\"') {
        while (++i<_len && (_str[i]!='\"'||(i>0 && _str[i]=='\"'&&_str[i-1]=='\\'))) {}
        i++;
    } else if (c=='\'') {
        while (++i<_len && _str[i]!='\'') {}
        i++;
    } else if (c=='`') {
        while (++i<_len && _str[i]!='`') {}
        i++;
    } else if (single_char.find(c)!=string::npos) {
        i++;
        if (c=='.' && _str[i]=='.') {
            i++;
        }
    } else {
        while (i<_len && !isspace(c) && !isalnum_(c) &&
               c!='\"' && c!='\'' && c!='`' && single_char.find(c)==string::npos) {
            c = _str[++i];
        }
    }
    string res = _str.substr(start, i-start);
    if (consume) {
        // Skip any trailing white spaces
        while (i<_len && isspace(_str[i])) {
            i++;
        }
        _pos = i;
    }
    //cerr<<" "<<res<<" ";
    return res;
}

void TParser::consume() {
    nextToken(true);
}

void TParser::expect(const string& expected)
{
    string token = nextToken();
    if (token!=expected) {
        throwError("Expected \""+expected+"\" but found: \""+token+"\"");
    }
}

bool TParser::lookAhead(const string& s)
{
    if (eos()) {
        return false;
    }
    //cout<<"Look ahead for:"<<s<<endl;
    string token = nextToken(false);
    return token == s;
}

bool TParser::ifNext(const string& s)
{
    if (lookAhead(s)) {
        consume();
        return true;
    }
    return false;
}

void TParser::pushPosition()
{
    _positions.push_back(_pos);
}

void TParser::popPosition()
{
    _positions.pop_back();
}

void TParser::restorePosition()
{
    _pos = _positions.back();
    popPosition();
}


void TParser::throwError(const string& msg)
{
    string err = _str.substr(0, _pos)+"/*error*/"+_str.substr(_pos);
    throw ParsingError("Error at: "+err+"\n"+msg);
}

TQKeyP TParser::key()
{
    TQKeyP res;
    string name;
    TExpressionP name_expr;
    string context;
    bool cond = false;
    bool pk = false;
    ContextMode mode = ContextMode::None;
    if (ifNext("$")) {
        expect("(");
        TExpressionP expr = expression();
        res = TQKeyP(new TQParamKey(expr));
        expect(")");
    } else if (nextToken(false)[0]=='$') {
        // If key start with $ sign, handle as expression
        TExpressionP expr = expression();
        res = TQKeyP(new TQParamKey(expr));
    } else if (ifNext("{")) {
        string next = nextToken();
        if (next=="}") {
            res = TQKeyP(new TQRegexKey(stripQuotes_("")));
        } else if (isQuoted(next)) {
            res = TQKeyP(new TQRegexKey(stripQuotes_(next)));
            expect("}");
        } else {
             throwError("Expected quotes (\")");
        }
    } else if (ifNext("#")) {
        string name = nextToken();
        if (name=="if") {
            res = TQKeyP(new TQDirectiveKey(KeyType::Cond));
        } else if (name=="func") {
            string token = nextToken();
            TQFuncDefinition* func = new TQFuncDefinition(KeyType::Func, token);
            if (ifNext("(")) {
                do {
                    string param = nextToken();
                    func->addParam(param);
                } while (ifNext(","));
                expect(")");
            }
            res = TQKeyP(func);
        } else if (name=="var") {
            string token = nextToken();
            res = TQKeyP(new TQDirectiveKey(KeyType::Variable, token));
        } else if (name=="assign") {
            string token = nextToken();
            res = TQKeyP(new TQDirectiveKey(KeyType::Assign, token));
        } else if (name == "exists") {
            res = TQKeyP(new TQDirectiveKey(KeyType::Exists));
        } else if (name == "notexists") {
            res = TQKeyP(new TQDirectiveKey(KeyType::Notexists));
        } else if (name == "return") {
            res = TQKeyP(new TQDirectiveKey(KeyType::Return));
        } else {
            throwError("Unknown directive \""+name+"\"");
        }
    } else {
        name = unescape_field_name(pathId());
        res = TQKeyP(new TQSimpleKey(name));
    }
    return res;
}


// TODO: Need to refactor this function and make it easier to understand. Especially with regards to ':' and '|' operators.
TemplateQueryP TParser::context_mod(bool frame_flag, ContextModMode parse_mode)
{
    TemplateQueryP res;
    bool or_start = (parse_mode==ContextModMode::Start);
    bool in_or = parse_mode==ContextModMode::OrStart || parse_mode==ContextModMode::Or;

    if (eos() || lookAhead("||")) {
        return res;
    }
    string next = nextToken(false);
    bool check = parse_mode == ContextModMode::OrStart;
    if (next==":" || next=="->") {
        check=true;
    } else if (next=="[" || next=="{" || next[0]=='.') {
        check = parse_mode!=ContextModMode::Start;
    }

    if (!check) {
        throwError("Unexpected token \""+next+"\" in key");
    }
    if (next==":") {
        if (in_or) {
            return res;
        }
        or_start = true;
        consume();
    }

    ContextMode mode = ContextMode::None;
    ArrowOp arrow = ArrowOp::None;
    string context;
    TExpressionP expr;
    bool new_frame = false;
    if (ifNext("{")) {
        mode = ContextMode::Regex;
        if(!ifNext("}")) {
            context = stripQuotes_(nextToken());
            expect("}");
        } else {
            context.clear();
        }
    } else if( ifNext("**")) {
        mode = ContextMode::AllPaths;
        context.clear();
    } else if (ifNext("$")) {
        expect("(");
        mode = ContextMode::Eval;
        expr = expression();
        if (!expr) {
            throwError("Error parsing [expression] in context modifier");
        }
        expect(")");
    } else if (nextToken(false)[0]=='$') {
        mode = ContextMode::Eval;
        expr = expression();
        if (!expr) {
            throwError("Error parsing [expression] in context modifier");
        }
    } else if (ifNext("->")) {
        mode = ContextMode::Arrow;
        if (ifNext("$")) {
            expect("(");
            expr = expression();
            expect(")");
        } else {
            string s = nextToken(false);
            if (s[0]=='$') {
                arrow = getArrowOp(s);
            }
            if (arrow==ArrowOp::None) {
                context = pathWithBrackets();
            } else if (arrow==ArrowOp::Other || arrow==ArrowOp::File) {
                expr = expression();
            } else {
                consume();
                if (ifNext("(")) {
                    context = pathWithBrackets();
                    expect(")");
                }
            }
        }
        new_frame = frame_flag;
    } else if (ifNext("[")) {
        if (ifNext("]")) {
            mode = ContextMode::Array;
        } else {
            string t = nextToken();
            if (isnumber(t)) {
                context = "["+t+"]";
                expect("]");
            } else {
                throwError("Expect a number");
            }
        }
    } else {
        context = pathWithBrackets(pathId());
        if (context.empty() && !lookAhead("[")) {
            mode = ContextMode::Reskey;
        }
        if (ifNext("[")) {
            mode = ContextMode::Array;
            expect("]");
        }
    }
    TQConditionP cond;
    if (ifNext("?")) {
        cond = condition();
    }
    res = context_mod(mode!=ContextMode::Arrow, in_or?(ContextModMode::Or):(ContextModMode::None));
    if (!res) {
        res = TemplateQueryP(new TQPlaceholder);
    }
    if (cond) {
        res = TemplateQueryP(new TQValueWithCond(res, cond));
    }
    if (expr) {
        res = TemplateQueryP(new TQContextMod(res, expr, mode, arrow, new_frame));
    } else {
        res = TemplateQueryP(new TQContextMod(res, context, mode, arrow, new_frame));
    }
    if (or_start && ifNext("||")) {
        /*     /--mod--\
             or         shared--value
               \--mod--/
        */
        TemplateQueryP ph(new TQPlaceholder);
        TemplateQueryP shared(new TQShared(ph));
        res = res->replace(shared);
        TQContextModOr* mod_or = new TQContextModOr(res);
        do {
            TemplateQueryP mod2 = context_mod(true, ContextModMode::OrStart);
            mod2 = mod2->replace(shared);
            mod_or->addVal(mod2);
        } while (ifNext("||"));
        res = TemplateQueryP(mod_or);
        // Look for more context modifiers after or expression
        TemplateQueryP res2 = context_mod(true, ContextModMode::None);
        if (res2) {
            res = res->replace(res2);
        }

    }
    return res;
}

ValueAndCond TParser::value()
{
    ValueAndCond res;
    if (eos()) {
        return res;
    }
    TExpressionP exp = expression();
    TQConditionP cond;

    OrderType order_type = OrderType::None;
    int order_num = 0;

    if (ifNext("?")) {
        cond = condition();
    }
    if (!eos() && !(nextToken(false)=="@")) {
        res.second = baseCondition(exp);
    }

    if (ifNext("@")) {
        string token = nextToken();
        if (token=="ascending") {
            order_type = OrderType::Ascend;
        } else if (token=="descending") {
            order_type = OrderType::Descend;
        } else if (token=="unique_ascending") {
            order_type = OrderType::UAscend;
        } else if (token=="unique_descending") {
            order_type = OrderType::UDescend;
        } else {
            throwError("Error parsing value. Unknown sort order: "+token);
        }
        if (ifNext("(")) {
            order_num = stoi(nextToken());
            expect(")");
        }
    }
    if (!eos()) {
        throwError("Could not parse text at the end");
    }
    res.first = TemplateQueryP(new TQValue(exp, order_type, order_num));
    if (cond) {
        res.first = TemplateQueryP(new TQValueWithCond(res.first, cond));
    }
    return res;
}

TQConditionP TParser::baseCondition(const TExpressionP& arg1)
{
    TExpressionP lhs = arg1;
    if (!lhs) {
        if (ifNext("(")) {
            TQConditionP res = condition();
            expect(")");
            return res;
        }
        lhs = expression();
    }
    /*
    string next;
    if (lhs->isBool() && (next=nextToken(false))!="=" && next!="!") {
        TExpressionP rhs = TExpressionP(new TExprBoolConst(true));
        return TQConditionP(new TQBoolTest(lhs, rhs, Operator::EQ));
    }
    */
    string op = nextToken();
    if (op=="!") {
        if (ifNext("=")) {
            op+="=";
        } else {
            return TQConditionP(new TQExistsTest(lhs));
        }
    } else if (op=="is_array") {
        return TQConditionP(new TQTypeTest(lhs, Operator::IS_ARRAY));
    } else if (op=="is_object") {
        return TQConditionP(new TQTypeTest(lhs, Operator::IS_OBJECT));
    } else if (op=="is_literal") {
        return TQConditionP(new TQTypeTest(lhs, Operator::IS_LITERAL));
    } else if (op=="is_string") {
        return TQConditionP(new TQTypeTest(lhs, Operator::IS_STRING));
    } else if (op=="is_int") {
        return TQConditionP(new TQTypeTest(lhs, Operator::IS_INT));
    } else if (op=="is_float") {
        return TQConditionP(new TQTypeTest(lhs, Operator::IS_FLOAT));
    } else if (op=="is_bool") {
        return TQConditionP(new TQTypeTest(lhs, Operator::IS_BOOL));
    }
    TExpressionP rhs = expression();
    if ((lhs->isAggregate(0)&&!rhs->isLiteral())||(rhs->isAggregate(0)&&!lhs->isLiteral())) {
        throwError("Aggregates can only be compared with literals");
    }
    if (lhs->isString() && rhs->isString()) {
        if (op=="=") {
            return TQConditionP(new TQStringTest(lhs, rhs, Operator::EQ));
        } else if (op=="!=") {
            return TQConditionP(new TQStringTest(lhs, rhs, Operator::NEQ));
        } else if (op=="contains") {
            return TQConditionP(new TQStringTest(lhs, rhs, Operator::CONTAINS));
        } else if (op=="starts_with") {
            return TQConditionP(new TQStringTest(lhs, rhs, Operator::STARTS));
        } else if (op=="ends_with") {
            return TQConditionP(new TQStringTest(lhs, rhs, Operator::ENDS));
        } else if (op=="matches") {
            return TQConditionP(new TQStringTest(lhs, rhs, Operator::MATCH));
        }
    } else if (lhs->isInt() && rhs->isInt() || lhs->isDouble() && rhs->isDouble()) {
        if (op=="=") {
            return TQConditionP(new TQNumberTest(lhs, rhs, Operator::EQ));
        } else if (op=="!=") {
            return TQConditionP(new TQNumberTest(lhs, rhs, Operator::NEQ));
        } else if (op==">") {
            return TQConditionP(new TQNumberTest(lhs, rhs, Operator::GT));
        } else if (op=="<") {
            return TQConditionP(new TQNumberTest(lhs, rhs, Operator::LT));
        } else if (op==">=") {
            return TQConditionP(new TQNumberTest(lhs, rhs, Operator::GE));
        } else if (op=="<=") {
            return TQConditionP(new TQNumberTest(lhs, rhs, Operator::LE));
        }
    } else if (lhs->isBool() && rhs->isBool()) {
        if (op=="=") {
            return TQConditionP(new TQBoolTest(lhs, rhs, Operator::EQ));
        } else if (op=="!=") {
            return TQConditionP(new TQBoolTest(lhs, rhs, Operator::NEQ));
        }
    }

    // In any other case, call operators between JSON expressions
    if (op=="=") {
        return TQConditionP(new TQJSONTest(lhs, rhs, Operator::EQ));
    } else if (op=="!=") {
        return TQConditionP(new TQJSONTest(lhs, rhs, Operator::NEQ));
    } else if (op=="in") {
        return TQConditionP(new TQJSONTest(lhs, rhs, Operator::IN));
    } else if (op=="not_in") {
        return TQConditionP(new TQJSONTest(lhs, rhs, Operator::NOTIN));
    }

    throwError("Error parsing condition");
   

    return {};

}

TQConditionP TParser::condition(int prec, const TExpressionP& exp)
{
    TQConditionP res;
    if (!exp && ifNext("(")) {
        // The parenthesis can contain either a condition or expression, so we try both
        TExpressionP exp1 = expression(0);
        if (ifNext(")")) {
            res = baseCondition(exp1);
        } else {
            res = condition(0,exp1);
            expect(")");
        }
    } else if (!exp && prec<=3 && ifNext("!")) {
        TQConditionP cond = condition(3);
        res = TQConditionP(new TQCondBool(cond, {}, Operator::NOT));
    } else {
        res = baseCondition(exp);
    }
    string next;
    while (!eos()) {
        Operator op;
        TQConditionP cond2;
        string next = nextToken(false);
        if (next=="&" && prec<=2) {
            op = Operator::AND;
            consume();
            cond2 = condition(2);
        } else if (next=="|" && prec<=1) {
            consume();
            cond2 = condition(1);
            op = Operator::OR;
        } else {
            break;
        }
        res = TQConditionP(new TQCondBool(res, cond2, op));
    }

    return res;
}

TExpressionP TParser::baseExpression()
{
    TExpressionP res;
    string token = nextToken();
    if (token.empty()) {
        throwError("Expected expression");
    }
    // This long else-if should be converted to switch-case
    if (token[0]=='`') {
        token = escape_field_name(stripQuotes_(token));
    }
    /*if (token=="(") {
        res = expression();
        expect(")");
    } else*/ if (token=="[") {
        TExpressionP dot(new TExprField("."));
        if (ifNext("]")) {
            res = TExpressionP(new TExprSubfield(dot, {}, true));
        } else {
            TExpressionP e= expression();
            expect("]");
            res = TExpressionP(new TExprSubfield(dot, e, true));
        }
    } else if (token=="$") {
        expect("(");
        TExpressionP arg = expression();
        expect(")");
        res = TExpressionP(new TExprField(arg));
    } else if (token=="$if") {
        expect("(");
        TQConditionP cond = condition();
        expect(",");
        TExpressionP th = expression();
        expect(",");
        TExpressionP el = expression();
        expect(")");
        res = TExpressionP(new TExprITE(cond, th, el));
    } else if (token=="$call") {
        expect("(");
        string name = nextToken();
        expect(")");
        res = TExpressionP(new TExprCall(name));
    } else if (token=="$var") {
        expect("(");
        string name = nextToken();
        expect(")");
        res = TExpressionP(new TExprVar(name));
    } else if (token=="$file") {
        expect("(");
        TExpressionP exp = expression();
        expect(")");
        res = TExpressionP(new TExprFile(exp));
    } else if (token=="$prev") {
        expect("(");
        TExpressionP def = expression();
        expect(")");
        res = TExpressionP(new TExprPrev(def));
    } else if (token=="$lower" || token=="$upper") {
        expect("(");
        TExpressionP exp = expression();
        expect(")");
        res = TExpressionP(new TExprChangeCase(exp, token=="$lower"));
    } else if (token=="$path") {
        res = TExpressionP(new TExprPath);
    } else if (token=="$index") {
        res = TExpressionP(new TExprIndex);
    } else if (token=="$key") {
        res = TExpressionP(new TExprKey);
    } else if (token=="$reskey") {
        res = TExpressionP(new TExprReskey);
    } else if (token=="$filename") {
        res = TExpressionP(new TExprFilename);
    } else if (token=="$env") {
        expect("(");
        TExpressionP exp = expression();
        expect(")");
        res = TExpressionP(new TExprEnv(exp));
    } else if (token=="$identifier") {
        int parts = 0;
        if (ifNext("(")) {
            parts = stoi(nextToken());
            expect(")");
        }
        res = TExpressionP(new TExprIdentifier(parts));
    } else if (token=="$xml") {
        res = TExpressionP(new TExprXML);
    } else if (token=="$xml_no_children") {
        res = TExpressionP(new TExprXML(true));
    } else if (token=="$node") {
        res = TExpressionP(new TExprNode);
    } else if (token=="$attr") {
        expect("(");
        string param = stripQuotes_(nextToken());
        expect(")");
        res = TExpressionP(new TExprAttr(param));
    } else if (token=="$child") {
        expect("(");
        string param = stripQuotes_(nextToken());
        expect(")");
        res = TExpressionP(new TExprChild(param));
    } else if (token=="$in_filter") {
        expect("(");
        string filter = stripQuotes_(nextToken());
        expect(")");
        res = TExpressionP(new TExprInFilter(filter));
    } else if (token=="$text") {
        expect("(");
        TExpressionP arg = expression();
        expect(")");
        res = TExpressionP(new TExprText(arg));
    } else if (token=="$size") {
        expect("(");
        TExpressionP arg = expression();
        expect(")");
        res = TExpressionP(new TExprSize(arg));
    } else if (token=="$length") {
        expect("(");
        TExpressionP arg = expression();
        expect(")");
        res = TExpressionP(new TExprLength(arg));
    } else if (token=="$string" || token=="$int" || token=="$float" || token=="$bool") {
        CastType ct;
        if (token=="$string") {
            ct = CastType::String;
        } else if (token=="$int") {
            ct = CastType::Int;
        } else if (token=="$float") {
            ct = CastType::Double;
        } else if (token=="$bool") {
            ct = CastType::Bool;
        }
        expect("(");
        TExpressionP arg = expression();
        expect(")");
        res = TExpressionP(new TExprTypeCast(arg, ct));
    } else if (token=="$count") {
        res = TExpressionP(new TExprCount);
    } else if (token=="$sum") {
        expect("(");
        TExpressionP arg = expression();
        expect(")");
        res = TExpressionP(new TExprSum(arg));
    } else if (token=="$avg") {
        expect("(");
        TExpressionP arg = expression();
        expect(")");
        res = TExpressionP(new TExprAvg(arg));
    } else if (token=="$min" || token=="$max") {
        expect("(");
        TExpressionP arg = expression();
        expect(")");
        res = TExpressionP(new TExprMinmax(token=="$max",arg));
    } else if (token=="$substr") {
        expect("(");
        TExpressionP arg = expression();
        expect(",");
        TExpressionP s = expression(0);
        TExpressionP l;
        if (ifNext(",")) {
            l = expression(0);
        }
        expect(")");
        res = TExpressionP(new TExprSubstr(arg, s, l));
    } else if ((token=="$find"||token=="$ifind")) {
        expect("(");
        TExpressionP arg = expression();
        expect(",");
        TExpressionP s = expression(0);
        expect(")");
        res = TExpressionP(new TExprFind(arg, s, token=="$find"));
    } else if (token=="$D") {
        string date = stripQuotes_(nextToken());
        time_t epoch = stringToTime(date);
        res = TExpressionP(new TExprIntConst(epoch));
    } else if (token[0]=='$') {
        string name = token.substr(1);
        TExprCall* call = new TExprCall(name);
        if (ifNext("(")) {
            do {
                TExpressionP exp = expression();
                call->addArg(exp);
            } while (ifNext(","));
            expect(")");
        }
        res = TExpressionP(call);
    } else if (token[0]=='\"' || token[0]=='\'') {
        res = TExpressionP( new TExprStringConst(stripQuotes_(token)));
    } else if (isnumber(token)) {
        if (token.find('.')!=string::npos) {
            res = TExpressionP(new TExprDoubleConst(stod(token)));
        } else {
            res = TExpressionP(new TExprIntConst(stoi(token)));
        }
    } else if (token=="true"||token=="false") {
        res = TExpressionP(new TExprBoolConst(token=="true"));
    } else if (token=="/") {
        TExpressionP arg = baseExpression();
        res = TExpressionP(new TExprChangepath(arg, Operator::ROOT));
    } else if (token==".." ) {
        TExpressionP arg;
        if (ifNext("/")) {
            arg = baseExpression();
        } else {
            arg = TExpressionP(new TExprField("."));
        }
        res = TExpressionP(new TExprChangepath(arg, Operator::UP));

    } else if (token=="<<") {
        TExpressionP arg = baseExpression();
        res = TExpressionP(new TExprChangepath(arg, Operator::PREVID));
    } else if (isalnum_(token[0])) {
        string path = pathWithBrackets(token); 
        res = TExpressionP(new TExprField(path));
    } else if (token==".") {
        res = TExpressionP(new TExprField(token));       
    } else if (token=="-") {
        string next = nextToken();
        res = TExpressionP(new TExprIntConst(-stoi(next)));
    } else {
        throwError("Expected expression");
    }
    return res;
}

TExpressionP TParser::expression(int prec)
{
    TExpressionP res;
    if (ifNext("(")) {
        res = expression(0);
        expect(")");
    } else {
        res = baseExpression();
    }
    while (!eos()) {
        string op = nextToken(false);
        if (op[0]=='`') {
            op = escape_field_name(stripQuotes_(op));
        }
        if (op=="+" && prec==0) {
            consume();
            TExpressionP arg2 = expression(1);
            res = TExpressionP(new TExprBinaryOp(res, Operator::PLUS, arg2));
        } else if (op=="-" && prec==0) {
            consume();
            TExpressionP arg2 = expression(1);
            res = TExpressionP(new TExprBinaryOp(res, Operator::MINUS, arg2));
        } else if (op=="*" && prec<=1) {
            consume();
            TExpressionP arg2 = expression(2);
            res = TExpressionP(new TExprBinaryOp(res, Operator::MULTIPLY, arg2));
        } else if (op=="/" && prec<=1) {
            consume();
            TExpressionP arg2 = expression(2);
            res = TExpressionP(new TExprBinaryOp(res, Operator::DIVISION, arg2));
        } else if (op=="mod" && prec<=1) {
            consume();
            TExpressionP arg2 = expression(2);
            res = TExpressionP(new TExprBinaryOp(res, Operator::MOD, arg2));
        } else if ((op=="[" || op==".") && prec<=3) {
            consume();
            TExpressionP e;
            if (!lookAhead("]")) {
                e = expression(0);
            }
            if (op=="[") {
                expect("]");
            }
            res = TExpressionP(new TExprSubfield(res, e, op=="["));
        } else if ((op[0]=='.'|| op[0]=='[') && prec<=3) {
            consume();
            string spath = pathWithBrackets(op);
            res = TExpressionP(new TExprSubfield(res, spath));
        } else {
            break;
        }
    }
    return res;
}

string TParser::pathId()
{
    string res;
    string token = nextToken(false);
    if (token.empty()) {
        return {};
    }
    if (isalnum_(token[0])) {
        consume();
        return token;
    } else if (token[0]=='`') {
        consume();
        return escape_field_name(stripQuotes_(token));      
    } else if (token[0]=='\"' || token[0]=='\'') {
        consume();
        return token;
    }
    throwError("Expected identifier or path");

    return {};
}

string TParser::pathWithBrackets(const std::string& path)
{
    string res = path;
    if (res.empty()) {
        res = pathId();
    }
    pushPosition();
    if (ifNext(".")) {
        string token = nextToken();
        if (!token.empty() && token[0]=='$') {
            restorePosition();
            return res;
        }
        if (token[0]=='`') {
            token = escape_field_name(stripQuotes_(token));
        }
        res = res+"."+token;
    }
    // If '[' followed by number, add to string. Otherwise, backtrace and return
    else if (ifNext("[")) {
        string token = nextToken();
        if (isnumber(token) && ifNext("]")) {
            res = res+"["+token+"]";
        } else {
            restorePosition();
            return res;
        }
    } else {
        popPosition();
        return res;
    }
    popPosition();
    return pathWithBrackets(res);
}


TemplateQueryP JSONToTQ(JSONValue& v)
{
    TemplateQueryP res;
    if (v.IsArray()) {
        TQArray* array = new TQArray;
        res = TemplateQueryP(array);
        for (auto& e: v.GetArray()) {
            TemplateQueryP val = JSONToTQ(e);
            array->add(val);
        }
    } else if (v.IsObject()) {
        TQObject* obj = new TQObject;
        res = TemplateQueryP(obj);
        for (auto& m: v.GetObject()) {
            string name =m.name.GetString();
            TParser parse_name(name);
            TQKeyP key = parse_name.key();
            TemplateQueryP context_mod = parse_name.context_mod(true, ContextModMode::Start);
            if (m.value.IsString()) {
                TParser parse_val(m.value.GetString());
                TQConditionP cond;
                TemplateQueryP condq;
                TemplateQueryP q;
                if (key->getKeyType()==KeyType::Cond) {
                    cond = parse_val.condition();
                    //obj->add(key, {}, cond);
                } else {
                    auto vc = parse_val.value();
                    q = vc.first;
                    cond = vc.second;
                    //obj->add(key, vc.first, vc.second);
                }
                if (cond) {
                    condq = TemplateQueryP(new TQCondWrapper(cond));
                    if (context_mod) {
                        condq = context_mod->replace(condq);
                    }
                }
                if (q && context_mod) {
                    q = context_mod->replace(q);
                }
                obj->add(key, q, condq);
            } else {
                TemplateQueryP val = JSONToTQ(m.value);
                if (context_mod) {
                    val = context_mod->replace(val);
                }
                obj->add(key, val);
            }

        }
    } else if (v.IsString()) {
        TParser parse(v.GetString());
        auto vc = parse.value();
        if (vc.second) {
            res = TemplateQueryP(new TQValueWithCond(vc.first, vc.second));
        } else {
            res = vc.first;
        }
    } else if (v.IsInt64()) {
        TExpressionP exp(new TExprIntConst(v.GetInt()));
        res = TemplateQueryP(new TQValue(exp));
    } else if (v.IsDouble()) {
        TExpressionP exp(new TExprDoubleConst(v.GetDouble()));
        res = TemplateQueryP(new TQValue(exp));
    } else if (v.IsBool()) {
        TExpressionP exp(new TExprBoolConst(v.GetBool()));
        res = TemplateQueryP(new TQValue(exp));
    }

    return res;
}


} // namespace xcite
