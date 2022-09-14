#ifndef TEMPLATEQUERY_H
#define TEMPLATEQUERY_H

#include "xcitedb-stubs.h"
//#include "JSONTraversal.h"
#include <memory>
#include <vector>
#include <iostream>
#include <regex>

namespace xcite {

typedef std::shared_ptr<rapidjson::Document> json_documentP;
typedef std::shared_ptr<pugi::xml_document> xml_documentP;
typedef std::shared_ptr<JSONMetaReader> JSONMetaReaderP;
typedef std::shared_ptr<XMLReader> XMLReaderP;
typedef std::shared_ptr<JSONValue> JSONValueP;

typedef std::vector<std::string> Strings;

class TQCondition;
typedef std::shared_ptr<TQCondition> TQConditionP;
class TExpression;
typedef std::shared_ptr<TExpression> TExpressionP;
class TQData;
typedef std::shared_ptr<TQData> TQDataP;
class TemplateQuery;
typedef std::shared_ptr<TemplateQuery> TemplateQueryP;

class DontDeleteJSONValue
{
public:
    void operator() (JSONValue*) const {}
};


class QueryError
{
public:
    QueryError(string msg_): msg(msg_) {}

    virtual string message() {
        return "Error executing query: "+msg;
    }

    string msg;
};


enum class OrderType {
    None, Ascend, Descend, UAscend, UDescend
};

struct Function {
    Function() {}
    Function(const TemplateQueryP& b, std::vector<string>& v)
        : body(b), params(v) {}
    TemplateQueryP body;
    std::vector<string> params;
};

class TQContext
{
public:
    TQContext();
    TQContext(XMLDBSettings& settings, MDB_txn* txn, const string& identifier, const string& index);
    void reset(const string& identifier, const string& index);

    const string& identifier() {
        if (identifiers.empty()) {
            return empty_string;
        }
        return identifiers.back();
    }
    const string& index() {
        if (indexes.empty()) {
            return empty_string;
        }
        return indexes.back();
    }
    const string& path() {
        if (paths.empty()) {
            return empty_string;
        }
        return paths.back();
    }

    const string& filename() {
        if (filenames.empty()) {
            return empty_string;
        }
        return filenames.back();
    }

    JSONValueP localJSON() {
        return localJSONs.back();
    }

    string fullPath(const string& subpath) {
        return fullPath(subpath, path());
    }
    string fullPath(const string& subpath, string currpath);

    TQData* data() {
        return curr_data.back();
    }

    const string& reskey() {
        if (reskeys.empty()) {
            return empty_string;
        }

        return reskeys.back();
    }
    void addFunc(const string& s, const TemplateQueryP& t, std::vector<string>& params) {
        funcs[s] = Function(t, params);
    }
    Function& getFunc(const string& name);

    JSONValueP addVar(const string& s, const JSONValueP& j);
    JSONValueP assignVar(const string& s, const JSONValueP& j);
    JSONValueP getVar(const string& s);
    void popVar(const string& s);

    void pushPath(const string& path);
    void adjustPath(const string& path);
    void addToPath(const string& added);
    void pushIdentifier(const string& identifier, const string& index = {});
    void startLocalJSON(const JSONValueP& json);
    void endLocalJSON();

    void pushData(TQData* data);
    void pushReskey(const string& key);
    void pushFilename(const string& name);

    void popPath();
    void popIdentifier();
    void popJSON();
    void popData(TQData* data);
    void popReskey();
    void popFilename();

    void beginFrame();
    void endFrame();
    void pushLastFrame();
    void popFrame();

    // Data access
    JSONValueP findLocalPath(const string& path) {
        return findLocalPath(path, localJSON());
    }
    JSONValueP findLocalPath(const string& path, JSONValueP val);
    bool isString(const string& key);
    bool isDouble(const string& key);
    bool isInt(const string& key);
    bool isBool(const string& key);
    bool exists(const string& key);
    JSONValueP getJSON(const string& key);
    string getString(const string& key);
    int64_t getInt(const string& key);
    double getDouble(const string& key);
    bool getBool(const string& key);
    int getArraySize(const string& key);
    ObjectFieldSet getMembers(const string& key);

public:
    XMLReaderP xml_reader;
    json_documentP doc;
    bool in_get_JSON = false;

private:
    typedef std::vector<std::string> StringQueue;
    typedef std::vector<int> IntQueue;
    typedef std::vector<JSONValueP> JSONQueue;
    const string empty_string = {};
    StringQueue paths;
    StringQueue indexes;
    StringQueue identifiers;
    StringQueue filenames;
    JSONQueue localJSONs;
    JSONQueue localDocs;
    // Keep track of result keys
    StringQueue reskeys;
    std::vector<TQData*> curr_data;

    IntQueue path_frames;
    IntQueue index_frames;
    IntQueue identifier_frames;
    IntQueue local_json_frames;

    std::map<string, Function> funcs;
    std::map<string, std::vector<JSONValueP> > variables;

    JSONMetaReaderP tr;

    bool in_local = false;
};

class TQData
{
public:
    virtual ~TQData() {}
    virtual bool processData(TQContext& ctx) = 0;
    virtual JSONValue getJSON(TQContext& ctx) = 0;
    virtual TemplateQuery* getTQ() = 0;

    virtual bool isXML(TQContext* ctx = NULL) {return false;}
    virtual bool isString(TQContext* ctx = NULL) {return false;}
    virtual bool isInt(TQContext* ctx = NULL) {return false;}
    virtual bool isDouble(TQContext* ctx = NULL) {return false;}
    virtual bool isBool(TQContext* ctx = NULL) {return false;}
    virtual bool isEmpty() {return false;}
    virtual bool isInnerValue() {return false;}

    virtual OrderType getOrderType() const {return OrderType::None;}
    virtual int getOrderNumber() const {return 0;}
    virtual bool isOrdered() const {return false;}
    virtual bool isAggregate(TQContext* ctx) const {return true;}
    virtual bool compare(const TQDataP& other) const {return false;}
    virtual bool equal(const TQDataP& other) const {return false;}
};

bool compare_data(const TQDataP& a, const TQDataP& b);
bool equal_data(const TQDataP& a, const TQDataP& b);

class TemplateQuery
{
public:
    virtual ~TemplateQuery() {}
    virtual TQDataP makeData() = 0;

    virtual bool isAggregate(TQContext* ctx) const {return false;}
    virtual OrderType getOrderType() const {return OrderType::None;}
    virtual int getOrderNumber() const {return 0;}
    virtual bool isOrdered() const {return false;}
    virtual bool isPlaceholder() const {return false;}
    virtual TemplateQueryP replace(const TemplateQueryP& val) {return TemplateQueryP(NULL);}
};

class TQPlaceholder: public TemplateQuery
{
    virtual TQDataP makeData() {return {}; }
    virtual bool isPlaceholder() const {return true;}
    virtual TemplateQueryP replace(const TemplateQueryP& val) {return val;}
};


enum class KeyType {Values, Cond, Func, Variable, Assign,Exists,Notexists};

class TQKey
{
public:
    virtual Strings getKeys(TQContext& ctx) = 0;
    virtual KeyType getKeyType() const {return KeyType::Values;}
    virtual string getName() const {return {};}
};

typedef std::shared_ptr<TQKey> TQKeyP;

class TQSimpleKey: public TQKey
{
public:
    TQSimpleKey(const std::string& k): key(k) {}
    virtual Strings getKeys(TQContext& ctx);
    virtual string getName() const {return key;}
private:
    std::string key;
};

class TQParamKey: public TQKey
{
public:
    TQParamKey(const TExpressionP& e): expr(e) {}
    virtual Strings getKeys(TQContext& ctx);
private:
    TExpressionP expr;
};

class TQRegexKey: public TQKey
{
public:
    TQRegexKey(const string& s): r(s), re(s) {}
    virtual Strings getKeys(TQContext& ctx);
private:
    string r;
    std::regex re;
};


class TQDirectiveKey: public TQKey
{
public:
    TQDirectiveKey(KeyType kt, const string& val = {}): ktype(kt), value(val) {}
    virtual Strings getKeys(TQContext& ctx) {return {};}
    virtual KeyType getKeyType() const {return ktype;}
    virtual string getName() const {return value;}

private:
    KeyType ktype;
    string value;
};

class TQFuncDefinition: public TQDirectiveKey
{
public:
    TQFuncDefinition(KeyType kt, const string& val = {}): TQDirectiveKey(kt, val) {}
    void addParam(const string& s);
    std::vector<string> params;
};


enum class ContextMode {None, Array, Regex, Arrow, Reskey, Eval, Var};

enum class ArrowOp {
    None, 
    Self, 
    Parent, 
    Ancestors, 
    AncestorsAndSelf, 
    Children, 
    Descendants, 
    DescendantsAndSelf, 
    All, 
    Var,
    File, 
    Other
};

ArrowOp getArrowOp(const string& str);

class TQInnerValue: public TemplateQuery
{
public:
    TQInnerValue(const TemplateQueryP& v): val(v) {}
    virtual OrderType getOrderType() const {return val->getOrderType();}
    virtual int getOrderNumber() const {return val->getOrderNumber();}
    virtual bool isOrdered() const {return val->isOrdered();}
    virtual bool isAggregate(TQContext* ctx) const {return val && val->isAggregate(ctx);}

    TemplateQueryP val;

};

class TQInnerValueData: public TQData
{
public:
    virtual OrderType getOrderType() const {return innerData->getOrderType();}
    virtual int getOrderNumber() const {return innerData->getOrderNumber();}
    virtual bool isOrdered() const {return innerData && innerData->isOrdered();}
    virtual bool isXML(TQContext* ctx) {return innerData && innerData->isXML(ctx);}
    virtual bool isString(TQContext* ctx) {return innerData && innerData->isString(ctx);}
    virtual bool isInt(TQContext* ctx) {return innerData && innerData->isInt(ctx);}
    virtual bool isDouble(TQContext* ctx) {return innerData && innerData->isDouble(ctx);}
    virtual bool isBool(TQContext* ctx) {return innerData && innerData->isBool(ctx);}

    virtual bool isEmpty() {return innerData->isEmpty();}
    virtual bool compare(const TQDataP& other) const;
    virtual bool equal(const TQDataP& other) const;
    virtual bool isInnerValue() {return true;}

    TQDataP innerData;
};

class TQContextMod: public TQInnerValue
{
public:
    TQContextMod(const TemplateQueryP& v, const string& c, ContextMode m, ArrowOp op, bool fr = false)
        : TQInnerValue(v), context(c), mode(m), arrow(op), new_frame(fr) {}
    TQContextMod(const TemplateQueryP& v, const TExpressionP& e, ContextMode m, ArrowOp op, bool fr = false)
        : TQInnerValue(v), expr(e), mode(m), arrow(op), new_frame(fr) {}

    virtual TQDataP makeData();
    virtual TemplateQueryP replace(const TemplateQueryP& val);

    std::string context;
    TExpressionP expr;
    ContextMode mode;
    ArrowOp arrow;
    bool new_frame;
};

class TQContextModData: public TQInnerValueData
{
public:
    TQContextModData(TQContextMod* mod): q(mod) {}
    virtual bool processData(TQContext& ctx);
    virtual JSONValue getJSON(TQContext& ctx);
    virtual TemplateQuery* getTQ() {return q;}


    bool contextMod(const TQDataP& data, TQContext& ctx);
    bool handleArrow(const TQDataP& data, TQContext& ctx);
    virtual bool isAggregate(TQContext* ctx) const;

    bool processIdentifier(const TQDataP& data, TQContext& ctx, string identifier) {
        ctx.pushIdentifier(identifier);
        ctx.pushPath("");
        bool res = data->processData(ctx);
        ctx.popPath();
        ctx.popIdentifier();
        return res;
    }
private:
    TQContextMod* q;
};

// An "or" operation between context modifiers
class TQContextModOr: public TemplateQuery
{
public:
    TQContextModOr(const TemplateQueryP& val = {}) {
        if (val) {
            vals.push_back(val);
        }
    }
    TQContextModOr(const std::vector<TemplateQueryP>& vs)
        : vals(vs) {}

    void addVal(const TemplateQueryP& val) {
        vals.push_back(val);
    }

    virtual TQDataP makeData();
    virtual TemplateQueryP replace(const TemplateQueryP& val);

    std::vector<TemplateQueryP> vals;
};

class TQContextModOrData: public TQData
{
public:
    TQContextModOrData(TQContextModOr* mod): q(mod) {}
    virtual bool processData(TQContext& ctx);
    virtual JSONValue getJSON(TQContext& ctx);
    virtual TemplateQuery* getTQ() {return q;}

    virtual bool isAggregate(TQContext* ctx) const;

private:
    TQContextModOr* q;
    std::vector<TQDataP> data;
};

// TQShared's job is to make sure data objects are shared after an or between context modifiers.
class TQShared: public TQInnerValue
{
public:
    TQShared(const TemplateQueryP& v, int id_ = -1)
        : TQInnerValue(v) 
    {
        if (id_==-1) {
            id = data_map.size();
            data_map.push_back({});
        } else {
            id = id_;
        }
    }

    virtual TQDataP makeData();
    virtual TemplateQueryP replace(const TemplateQueryP& val);
protected:
    friend class TQSharedData;
    // Each TQShared object gets an id that remain constant even when duplicating with 'replace'.
    int id;
    // Vector of ids. Each entry is a map from data object pointers (set in TQContextModOrData::processData)
    static std::vector<std::map<TQData*,TQDataP> > data_map;
};

class TQSharedData: public TQInnerValueData
{
public:
    TQSharedData(TQShared* shared): q(shared) {}
    virtual bool processData(TQContext& ctx);
    virtual JSONValue getJSON(TQContext& ctx);
    virtual TemplateQuery* getTQ() {return q;}

    TQShared* q;
};

class TQArray: public TemplateQuery
{
public:
    TQArray(const TemplateQueryP& v): val(v) {}
    virtual TQDataP makeData();

    TemplateQueryP val;
};

typedef std::shared_ptr<TQArray> TQArrayP;

class TQArrayData: public TQData
{
public:
    TQArrayData(TQArray* array): q(array) {}
    virtual bool processData(TQContext& ctx);
    virtual JSONValue getJSON(TQContext& ctx);
    virtual TemplateQuery* getTQ() {return q;}
    virtual bool isEmpty() {return array.empty();}

private:
    TQArray* q;
    std::vector<TQDataP> array;
};

class TQValueWithCond: public TQInnerValue
{
public:
    TQValueWithCond(const TemplateQueryP& v, const TQConditionP& c)
        : TQInnerValue(v), cond(c) {}
    virtual TQDataP makeData();
    virtual TemplateQueryP replace(const TemplateQueryP& v);
    virtual bool isAggregate(TQContext* ctx) const;

    TQConditionP cond;
};

class TQValueWithCondData: public TQInnerValueData
{
public:
    TQValueWithCondData(TQValueWithCond* tq): q(tq) {}
    virtual bool processData(TQContext& ctx);
    virtual JSONValue getJSON(TQContext& ctx);
    virtual TemplateQuery* getTQ() {return q;}

    virtual bool isAggregate(TQContext* ctx) const;

private:
    TQValueWithCond* q;
};

class DoNothingDeleter
{
public:
    void operator() (TQData*) const {}
};

// Wrapper for TQConditionP as a TemplateQuery object
class TQCondWrapper: public TemplateQuery, TQData
{
public:
    // Initialize this_p with DoNothingDeleter, so that is won't get deleted
    TQCondWrapper(const TQConditionP& c)
        : cond(c), this_p((TQData*)this, DoNothingDeleter()) {}
    virtual TQDataP makeData();
    virtual bool isAggregate(TQContext* ctx) const;
    virtual bool processData(TQContext& ctx);
    virtual JSONValue getJSON(TQContext& ctx) {return {};}
    virtual TemplateQuery* getTQ() {return this;}

    TQConditionP cond;
    TQDataP this_p;
};

class TQObject: public TemplateQuery
{
public:
    virtual TQDataP makeData();
    void add(const TQKeyP& key, const TemplateQueryP& value, const TemplateQueryP& cond = {});
    virtual bool isOrdered() const {return ordered;}

    friend class TQObjectData;
private:
    bool ordered = false;
    std::vector<TQDataP> conditions_data;
    std::vector<std::string> local_vars;
 
    std::vector<std::pair<TQKeyP, TemplateQueryP> > fields;
 };


class TQObjectData: public TQData
{
public:
    TQObjectData(TQObject* obj): q(obj) {}
    virtual bool processData(TQContext& ctx);
    virtual JSONValue getJSON(TQContext& ctx);
    virtual TemplateQuery* getTQ() {return q;}

    virtual bool isOrdered() const {return q->isOrdered();}
    virtual bool compare(const TQDataP& other) const;
    virtual bool equal(const TQDataP& other) const;
    virtual bool isEmpty() {return fields.empty();}

private:
    TQObject* q;
    std::map<std::string, TQDataP> fields;
    std::map<int, TQDataP> ordering;
};

class TExpression
{
public:
    virtual bool isJSON(TQContext* ctx = NULL) {return false;}
    virtual bool isXML(TQContext* ctx = NULL) {return false;}
    virtual bool isString(TQContext* ctx = NULL) {return false;}
    virtual bool isInt(TQContext* ctx = NULL) {return false;}
    virtual bool isDouble(TQContext* ctx = NULL) {return false;}
    virtual bool isBool(TQContext* ctx = NULL) {return false;}
    virtual bool exists(TQContext& ctx) {return false;}
    virtual bool isAggregate(TQContext* ctx) {return false;}
    virtual bool isField() {return false;}

    JSONValueP asJSON(TQContext& ctx);

    virtual JSONValueP getJSON(TQContext& ctx)
        {return {};}
    virtual string getString(TQContext& ctx)
        {return {};}
    virtual int64_t getInt(TQContext& ctx)
        {return {};}
    virtual double getDouble(TQContext& ctx)
        {return {};}
    virtual bool getBool(TQContext& ctx)
        {return {};}
    virtual pugi::xml_node getXML(TQContext& ctx, pugi::xml_document& doc)
        {return {};}
    virtual string getFieldPath(TQContext& ctx) {return {};}
};

class TQValue: public TemplateQuery
{
public:
    TQValue(const TExpressionP& e, OrderType ot=OrderType::None, int on = 0)
        : exp(e), ord_type(ot), ord_num(on) {}
    virtual TQDataP makeData();

    virtual OrderType getOrderType() const {return ord_type;}
    virtual int getOrderNumber() const {return ord_num;}
    virtual bool isOrdered() const {return ord_type!=OrderType::None;}
    virtual bool isAggregate(TQContext* ctx) const {return exp->isAggregate(ctx);}

    friend class TQValueData;
     
private:
    TExpressionP exp;
    OrderType ord_type;
    int ord_num;
};

class TQValueData: public TQData
{
public:
    TQValueData(TQValue* tqv): q(tqv) {}
    virtual bool processData(TQContext& ctx);
    virtual JSONValue getJSON(TQContext& ctx);
    virtual TemplateQuery* getTQ() {return q;}
    virtual bool isXML(TQContext* ctx) {return q->exp->isXML(ctx);}
    virtual bool isString(TQContext* ctx) {return q->exp->isString(ctx);}
    virtual bool isInt(TQContext* ctx) {return q->exp->isInt(ctx);}
    virtual bool isDouble(TQContext* ctx) {return q->exp->isDouble(ctx);}
    virtual bool isBool(TQContext* ctx) {return q->exp->isBool(ctx);}
    virtual bool isEmpty() {return val->IsNull();}

    virtual OrderType getOrderType() const {return q->getOrderType();}
    virtual int getOrderNumber() const {return q->getOrderNumber();}
    virtual bool isOrdered() const {return q->isOrdered();}
    virtual bool compare(const TQDataP& other) const;
    virtual bool equal(const TQDataP& other) const;

private:
    TQValue* q;
    JSONValueP val;
    bool updated = false;
};

typedef std::shared_ptr<TQValueData> TQValueDataP;

class TQCondition
{
public:
    virtual bool test(TQContext& ctx) = 0;
    virtual bool isAggregate(TQContext* ctx) {return false;}
};

enum class Operator {
    EQ,
    NEQ,
    CONTAINS,
    STARTS,
    ENDS,
    MATCH, 
    OR, 
    AND, 
    NOT, 
    GT, 
    LT, 
    GE, 
    LE,
    IN,
    NOTIN, 
    PLUS, 
    MINUS, 
    MULTIPLY, 
    DIVISION, 
    MOD, 
    SUBSTR,
    ROOT,
    UP,
    PREVID};

class TQCondBool: public TQCondition
{
public:
    TQCondBool(const TQConditionP& c1, const TQConditionP& c2, Operator o)
        : cond1(c1), cond2(c2), op(o) {}
    virtual bool test(TQContext& ctx);
    virtual bool isAggregate(TQContext* ctx) {return cond1->isAggregate(ctx)||cond2 && cond2->isAggregate(ctx);}
private:
    TQConditionP cond1;
    TQConditionP cond2;
    Operator op;
};

class TQStringTest: public TQCondition
{
public:
    TQStringTest(const TExpressionP& x1, const TExpressionP& y1, Operator o)
        : x(x1), y(y1), op(o) {}
    virtual bool test(TQContext& ctx);
    virtual bool isAggregate(TQContext* ctx) {return x->isAggregate(ctx)||y->isAggregate(ctx);}
    
private:
    TExpressionP x;
    TExpressionP y;
    Operator op;
};

class TQNumberTest: public TQCondition
{
public:
    TQNumberTest(const TExpressionP& x1, const TExpressionP& y1, Operator o)
        : x(x1), y(y1), op(o) {}
    virtual bool test(TQContext& ctx);
    template<typename T> bool test(T v1, T v2);
    virtual bool isAggregate(TQContext* ctx) {return x->isAggregate(ctx)||y->isAggregate(ctx);}
    
private:
    TExpressionP x;
    TExpressionP y;
    Operator op;
};

template<typename T> bool TQNumberTest::test(T v1, T v2)
{
    switch (op) {
        case Operator::EQ:
            return v1==v2;
        case Operator::NEQ:
            return v1!=v2;
        case Operator::GT:
            return v1>v2;
        case Operator::LT:
            return v1<v2;
        case Operator::GE:
            return v1>=v2;
        case Operator::LE:
            return v1<=v2;
        default:
            return false;
    }
}


class TQBoolTest: public TQCondition
{
public:
    TQBoolTest(const TExpressionP& x1, const TExpressionP& y1, Operator o)
        : x(x1), y(y1), op(o) {}
    virtual bool test(TQContext& ctx);
    virtual bool isAggregate(TQContext* ctx) {return x->isAggregate(ctx)||y->isAggregate(ctx);}
    
private:
    TExpressionP x;
    TExpressionP y;
    Operator op;
};

class TQJSONTest: public TQCondition
{
public:
    TQJSONTest(const TExpressionP& x1, const TExpressionP& y1, Operator o)
        : x(x1), y(y1), op(o) {}
    virtual bool test(TQContext& ctx);
    virtual bool isAggregate(TQContext* ctx) {return x->isAggregate(ctx)||y->isAggregate(ctx);}

    bool in_operator(const JSONValueP& v1, const JSONValueP& v2);
    
private:
    TExpressionP x;
    TExpressionP y;
    Operator op;
};

class TQExistsTest: public TQCondition
{
public:
    TQExistsTest(const TExpressionP& x1)
        : x(x1) {}
    virtual bool test(TQContext& ctx);
    
private:
    TExpressionP x;
};

class TExprField: public TExpression
{
public:
    TExprField(const std::string& f): field(f) {}
    TExprField(const TExpressionP& e): expr(e) {}
    std::string getFieldName(TQContext* ctx);

    virtual bool isJSON(TQContext* ctx) {return true;}
    virtual bool isString(TQContext* ctx);
    virtual bool isDouble(TQContext* ctx);
    virtual bool isInt(TQContext* ctx);
    virtual bool isBool(TQContext* ctx);
    virtual bool isField() {return true;}
    virtual bool exists(TQContext& ctx);
    virtual JSONValueP getJSON(TQContext& ctx);
    virtual string getString(TQContext& ctx);
    virtual int64_t getInt(TQContext& ctx);
    virtual double getDouble(TQContext& ctx);
    virtual bool getBool(TQContext& ctx);
    virtual string getFieldPath(TQContext& ctx) {
        return getFieldName(&ctx);
    }
private:
    
    std::string field;
    TExpressionP expr;
};

class TExprChangepath: public TExpression
{
public:
    TExprChangepath(const TExpressionP& e, Operator o): exp(e), op(o) {}

    virtual bool isJSON(TQContext* ctx) {return exp->isJSON();}
    virtual bool isString(TQContext* ctx) {return exp->isString();}
    virtual bool isDouble(TQContext* ctx) {return exp->isDouble();}
    virtual bool isInt(TQContext* ctx) {return exp->isInt();}
    virtual bool isBool(TQContext* ctx) {return exp->isBool();}
    virtual bool isField() {return op!=Operator::PREVID && exp->isField();}
    virtual bool exists(TQContext& ctx);
    virtual JSONValueP getJSON(TQContext& ctx);
    virtual string getString(TQContext& ctx);
    virtual int64_t getInt(TQContext& ctx);
    virtual double getDouble(TQContext& ctx);
    virtual bool getBool(TQContext& ctx);
    virtual string getFieldPath(TQContext& ctx);

private:
    void adjustPath(TQContext& ctx);
    void restorePath(TQContext& ctx);

    TExpressionP exp;
    // If toRoot is true, change to root, otherwise, change one level up
    Operator op;
};

class TExprITE: public TExpression
{
public:
    TExprITE(const TQConditionP& c, const TExpressionP& t, const TExpressionP& e)
        : cond(c), th(t), el(e) {}
    virtual bool isJSON(TQContext* ctx) {return true;}
    virtual JSONValueP getJSON(TQContext& ctx);
    virtual bool isAggregate(TQContext* ctx)
        {return th->isAggregate(ctx)||el->isAggregate(ctx);}

private:
    TQConditionP cond;
    TExpressionP th;
    TExpressionP el;
};

class TExprStringConst: public TExpression
{
public:
    TExprStringConst(const string& s): str(s) {}
    virtual bool isString(TQContext* ctx) {return true;}
    virtual string getString(TQContext& ctx)
        {return str;}
private:
    string str;
};

class TExprIntConst: public TExpression
{
public:
    TExprIntConst(int v): val(v) {}
    virtual bool isInt(TQContext* ctx) {return true;}
    virtual int64_t getInt(TQContext& ctx)
        {return val;}
    virtual double getDouble(TQContext& ctx)
        {return val;}
    virtual string getString(TQContext& ctx)
        {return std::to_string(val);}

private:
    int val;
};

class TExprDoubleConst: public TExpression
{
public:
    TExprDoubleConst(double v): val(v) {}
    virtual bool isDouble(TQContext* ctx) {return true;}
    virtual double getDouble(TQContext& ctx)
        {return val;}
    virtual int64_t getInt(TQContext& ctx)
        {return val;}
    virtual string getString(TQContext& ctx)
        {return std::to_string(val);}

private:
    double val;
};


class TExprBoolConst: public TExpression
{
public:
    TExprBoolConst(bool v): val(v) {}
    virtual bool isBool(TQContext* ctx) {return true;}
    virtual bool getBool(TQContext& ctx)
        {return val;}
private:
    bool val;
};

class TExprInFilter: public TExpression
{
public:
    TExprInFilter(const string& s): filter_name(s) {}
    virtual bool isBool(TQContext* ctx) {return true;}
    virtual bool getBool(TQContext& ctx);
private:
    string filter_name;
};


class TExprString: public TExpression
{
public:
    virtual bool isString(TQContext* ctx) {return true;}
};

class TExprPath: public TExprString
{
public:
    virtual string getString(TQContext& ctx);
};

class TExprIndex: public TExprString
{
public:
    virtual string getString(TQContext& ctx);
};


class TExprKey: public TExprString
{
public:
    virtual string getString(TQContext& ctx);
};

class TExprReskey: public TExprString
{
public:
    virtual string getString(TQContext& ctx);
};

class TExprFilename: public TExprString
{
public:
    virtual string getString(TQContext& ctx);
};


class TExprIdentifier: public TExprString
{
public:
    TExprIdentifier(int parts_ = 0): parts(parts_) {}
    virtual string getString(TQContext& ctx);
private:
    int parts;
};

class TExprXMLType: public TExprString
{
public:
    virtual bool isXML(TQContext* ctx) {return true;}
    virtual string getString(TQContext& ctx);
};

class TExprXML: public TExprXMLType
{
public:
    TExprXML(bool nochildren = false): no_children(nochildren) {}
    virtual pugi::xml_node getXML(TQContext& ctx, pugi::xml_document& xmldoc);
private:
    bool no_children;
};

class TExprAttr: public TExprString
{
public:
    TExprAttr(const string& s): attr_name(s) {}

    virtual string getString(TQContext& ctx);
private:
    string attr_name;
};

class TExprNode: public TExprString
{
public:
    virtual string getString(TQContext& ctx);
};

class TExprChild: public TExprXMLType
{
public:
    TExprChild(const string& s): name(s) {}
    virtual pugi::xml_node getXML(TQContext& ctx, pugi::xml_document& xmldoc);
private:
    string name;
};

class TExprText: public TExprString
{
public:
    TExprText(const TExpressionP& x): arg(x) {}
    virtual string getString(TQContext& ctx);
private:
    string XMLToString(const pugi::xml_node& n);
    TExpressionP arg;
};


class TExprArithmetic: public TExpression
{
public:
    TExprArithmetic(const TExpressionP& x, Operator o, const TExpressionP& y)
        : arg1(x), op(o), arg2(y) {}
    virtual bool isInt(TQContext* ctx) {return true;}
    virtual bool isDouble(TQContext* ctx);
    virtual int64_t getInt(TQContext& ctx);
    virtual double getDouble(TQContext& ctx);
    virtual bool isAggregate(TQContext* ctx)
        {return arg1->isAggregate(ctx)||arg2->isAggregate(ctx);}
private:
    TExpressionP arg1;
    TExpressionP arg2;
    Operator op;
};

class TExprConcat: public TExpression
{
public:
    TExprConcat(const TExpressionP& x, const TExpressionP& y)
        : arg1(x), arg2(y) {}
    virtual bool isString(TQContext* ctx) {return true;}
    virtual string getString(TQContext& ctx);
    virtual bool isAggregate(TQContext* ctx)
        {return arg1->isAggregate(ctx)||arg2->isAggregate(ctx);}
private:
    TExpressionP arg1;
    TExpressionP arg2;
};

class TExprSubstr: public TExpression
{
public:
    TExprSubstr(const TExpressionP& x, const TExpressionP& s, const TExpressionP& l = {})
        : str(x), start(s), length(l) {}
    virtual bool isString(TQContext* ctx) {return true;}
    virtual string getString(TQContext& ctx);
    virtual bool isAggregate(TQContext* ctx)
        {return str->isAggregate(ctx)||start->isAggregate(ctx)||length->isAggregate(ctx);}

private:
    TExpressionP str;
    TExpressionP start;
    TExpressionP length;
};

class TExprFind: public TExpression
{
public:
    TExprFind(const TExpressionP& x, const TExpressionP& y, bool cs = true)
        : str(x), searched(y), case_sensitive(cs) {}
    virtual bool isAggregate(TQContext* ctx)
        {return str->isAggregate(ctx)||searched->isAggregate(ctx);}
    virtual bool isJSON(TQContext* ctx) {return true;}
    virtual JSONValueP getJSON(TQContext& ctx);

private:
    TExpressionP str;
    TExpressionP searched;
    bool case_sensitive;
};

class TExprSubfield: public TExpression
{
public:
    TExprSubfield(const TExpressionP& x, const string& spath)
        : arg(x), subpath(spath) {}
    TExprSubfield(const TExpressionP& x, const TExpressionP& e, bool index)
        : arg(x), expr(e), is_index(index) {}
    virtual bool isAggregate(TQContext* ctx)
        {return arg->isAggregate(ctx);}
    virtual bool isJSON(TQContext* ctx) {return true;}
    virtual bool isString(TQContext* ctx);
    virtual bool isDouble(TQContext* ctx);
    virtual bool isInt(TQContext* ctx);
    virtual bool isBool(TQContext* ctx);

    virtual bool exists(TQContext& ctx);
    virtual bool isField() {return arg->isField()&&(!subpath.empty()||expr);}
    virtual JSONValueP getJSON(TQContext& ctx);
    virtual string getString(TQContext& ctx);
    virtual int64_t getInt(TQContext& ctx);
    virtual double getDouble(TQContext& ctx);
    virtual bool getBool(TQContext& ctx);

    virtual string getFieldPath(TQContext& ctx);

    string getSubpath(TQContext& ctx);
private:
    TExpressionP arg;
    string subpath;
    TExpressionP expr;
    bool is_index;
};

class TExprSize: public TExpression
{
public:
    TExprSize(const TExpressionP& a): array(a) {}
    virtual bool isInt(TQContext* ctx) {return true;}
    virtual bool isAggregate(TQContext* ctx)
        {return array->isAggregate(ctx);}
    
    virtual int64_t getInt(TQContext& ctx);

private:
    TExpressionP array;
};

class TExprLength: public TExpression
{
public:
    TExprLength(const TExpressionP& s): str(s) {}
    virtual bool isInt(TQContext* ctx) {return true;}
    virtual bool isAggregate(TQContext* ctx)
        {return str->isAggregate(ctx);}
    
    virtual int64_t getInt(TQContext& ctx);

private:
    TExpressionP str;
};

class TQAggregateData
{
public:
    virtual int64_t getInt(TQContext& ctx) = 0;
    virtual double getDouble(TQContext& ctx) {return {};}
};

typedef std::shared_ptr<TQAggregateData> TQAggregateDataP;

class TExprAggregate: public TExpression
{
public:
    virtual bool isAggregate(TQContext* ctx) {return true;}
    virtual bool isInt(TQContext* ctx) {return true;}
    virtual int64_t getInt(TQContext& ctx);
    virtual double getDouble(TQContext& ctx);
    virtual TQAggregateDataP getData(TQContext& ctx);
    virtual TQAggregateDataP makeData() = 0;

protected:
    std::map<TQData*,TQAggregateDataP> data_map;
};

class TExprCount: public TExprAggregate
{
public:
    virtual TQAggregateDataP makeData();
};

class TExprCountData: public TQAggregateData
{
public:
    TExprCountData(TExprCount* e) : expr(e) {}
    virtual int64_t getInt(TQContext& ctx);
private:
    TExprCount* expr;
    int64_t count = 0;
};

class TExprSum: public TExprAggregate
{
public:
    TExprSum(const TExpressionP& x): arg(x) {}
    virtual TQAggregateDataP makeData();

    TExpressionP arg;
};

class TExprSumData: public TQAggregateData
{
public:
    TExprSumData(TExprSum* e) : expr(e) {}
    virtual int64_t getInt(TQContext& ctx);
private:
    TExprSum* expr;
    int64_t sum = 0;
};

class TExprAvg: public TExprAggregate
{
public:
    TExprAvg(const TExpressionP& x): arg(x) {}
    virtual TQAggregateDataP makeData();
    virtual bool isDouble(TQContext* ctx) {return true;}

    TExpressionP arg;
};

class TExprAvgData: public TQAggregateData
{
public:
    TExprAvgData(TExprAvg* e) : expr(e) {}
    virtual int64_t getInt(TQContext& ctx);
    virtual double getDouble(TQContext& ctx);
private:
    TExprAvg* expr;
    double sum = 0;
    int64_t count = 0;
};

class TExprMinmax: public TExprAggregate
{
public:
    TExprMinmax(bool flag, const TExpressionP& x): arg(x), max(flag) {}
    virtual TQAggregateDataP makeData();

    TExpressionP arg;
    bool max;
};

class TExprMinmaxData: public TQAggregateData
{
public:
    TExprMinmaxData(TExprMinmax* e): expr(e) {}
    virtual int64_t getInt(TQContext& ctx);
private:
    TExprMinmax* expr;
    int64_t num = 0;
    bool first = true;
};

class TExprPrev: public TExpression
{
public:
    TExprPrev(const TExpressionP& def): dfault(def) {}
    virtual bool isAggregate(TQContext* ctx) {return true;}
    virtual bool isJSON(TQContext* ctx) {return true;}
    virtual bool isString(TQContext* ctx) {return true;}
    virtual bool isDouble(TQContext* ctx) {return true;}
    virtual bool isInt(TQContext* ctx) {return true;}
    virtual bool isBool(TQContext* ctx) {return true;}
    virtual bool exists(TQContext& ctx) {return true;}
    virtual JSONValueP getJSON(TQContext& ctx);
    virtual string getString(TQContext& ctx);
    virtual int64_t getInt(TQContext& ctx);
    //virtual double getDouble(TQContext& ctx);
    //virtual bool getBool(TQContext& ctx);

private:
    TExpressionP dfault;
};

class TExprCall: public TExpression
{
public:
    TExprCall(const string& p): proc(p) {}
    virtual bool isAggregate(TQContext* ctx) {return true;}
    virtual bool isJSON(TQContext* ctx) {return true;}

    virtual JSONValueP getJSON(TQContext& ctx);
    void addArg(const TExpressionP& exp) {
        args.push_back(exp);
    }

protected:
    std::map<TQData*,TQDataP> data_map;
    string proc;
    std::vector<TExpressionP> args;
};

class TExprVar: public TExpression
{
public:
    TExprVar(const string& s): name(s) {}
    virtual bool isJSON(TQContext* ctx) {return true;}
    virtual bool isString(TQContext* ctx);
    virtual bool isDouble(TQContext* ctx);
    virtual bool isInt(TQContext* ctx);
    virtual bool isBool(TQContext* ctx);
    virtual bool isAggregate(TQContext* ctx);
    
    virtual JSONValueP getJSON(TQContext& ctx);
    virtual string getString(TQContext& ctx);
    virtual int64_t getInt(TQContext& ctx);
    virtual double getDouble(TQContext& ctx);
    virtual bool getBool(TQContext& ctx);
    virtual bool exists(TQContext& ctx);

protected:
    string name;
};

class TExprFile: public TExpression
{
public:
    TExprFile(const TExpressionP& e): filename(e) {}
    virtual bool isJSON(TQContext* ctx) {return true;}
    virtual JSONValueP getJSON(TQContext& ctx);
    virtual bool exists(TQContext& ctx);
    string getFilename(TQContext& ctx);
protected:
    TExpressionP filename;
    JSONValueP json;
};

} // namespace xcite

#endif //TEMPLATEQUERY_H
