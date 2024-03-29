// Copyright (c) 2022 by Sela Mador-Haim

#include "TemplateQuery.h"
//#include "query.h"
#include "utils.h"
#include "rapidjson/document.h"
#include <rapidjson/ostreamwrapper.h>
#include "rapidjson/filereadstream.h"
#include "rapidjson/error/en.h"
#include <regex>
#include <iostream>
#include <algorithm>
#include <csignal>
#include <sstream>
#include <fstream>

using namespace std;
using namespace rapidjson;

namespace xcite {

std::vector<std::map<TQData*,TQDataP> > TQShared::data_map;


TQContext::TQContext()
    : doc(new rapidjson::Document)
{ 
}


TQContext::TQContext(XMLDBSettings& settings, MDB_txn* txn, const string& identifier, const string& index)
    : xml_reader(new XMLReader(settings, txn)), tr(new JSONMetaReader(settings, txn, index)), doc(new rapidjson::Document)
{
    pushIdentifier(identifier, index);
    pushPath({});
    pushDate(settings.date);
    pushBranch(settings.branch);
}

void TQContext::reset(const string& identifier, const string& index)
{
    paths.clear();
    identifiers.clear();
    dates.clear();
    branches.clear();
    indexes.clear();
    reskeys.clear();
    curr_data.clear();
    path_frames.clear();
    index_frames.clear();
    identifier_frames.clear();
    local_json_frames.clear();
    localJSONs.clear();
    localDocs.clear();
    if (!identifier.empty()) {
        pushIdentifier(identifier, index);
    }
    pushPath({});
    if (tr) {
        pushDate(tr->getDate());
        pushBranch(tr->getBranch());
    }
    in_local = false;
}

JSONValueP TQContext::addVar(const string& s, const JSONValueP& j)
{
    variables[s].push_back(j);
    return variables[s].back();
}

JSONValueP TQContext::assignVar(const string& s, const JSONValueP& j)
{
    auto it = variables.find(s);
    if (it!=variables.end()&&!it->second.empty()) {
        it->second.pop_back();
    }
    return addVar(s,j);
}

JSONValueP TQContext::getVar(const string& s)
{
    static JSONValueP nullJSON(new JSONValue);
    auto it = variables.find(s);
    if (it==variables.end() || it->second.empty()) {
        return nullJSON;
    }
    return it->second.back();
}

void TQContext::popVar(const string& s)
{
    variables[s].pop_back();
}

Function& TQContext::getFunc(const string& name)
{
    return funcs[name];
}


void TQContext::pushPath(const string& path)
{
    paths.push_back(path);
}

void TQContext::adjustPath(const string& path)
{
    pushPath(path);
    if (in_local) {
        JSONValueP val = findLocalPath(path, localDocs.back(), true);
        localJSONs.push_back(val);
    }
}

void TQContext::addToPath(const string& added)
{
    if (in_local) {
        JSONValueP val = findLocalPath(added);
        localJSONs.push_back(val);
    }
    pushPath(fullPath(added));
}

void TQContext::pushIdentifier(const string& identifier, const string& index)
{
    identifiers.push_back(identifier);
    string idx = index.empty()?xml_reader->getNodeFromIdentifier(identifier):index;
    indexes.push_back(idx);
    tr->changeNode(idx);
}

void TQContext::pushDate(const string& date)
{
    dates.push_back(date);
    tr->setDate(date);
    xml_reader->setDate(date);
}

void TQContext::pushBranch(const string& branch)
{
    branches.push_back(branch);
    tr->setBranch(branch);
    xml_reader->setBranch(branch);
}

void TQContext::startLocalJSON(const JSONValueP& json)
{
    localJSONs.push_back(json);
    localDocs.push_back(json);
    in_local = true;
}

void TQContext::endLocalJSON()
{
    popJSON();
    localDocs.pop_back();
}

void TQContext::pushData(TQData* data)
{
    curr_data.push_back(data);
}

void TQContext::pushReskey(const string& key)
{
    reskeys.push_back(key);
}

void TQContext::pushFilename(const string& key)
{
    filenames.push_back(key);
}


void TQContext::popPath()
{
    if (in_local) {
        popJSON();
    }
    paths.pop_back();
}

void TQContext::popIdentifier()
{
    identifiers.pop_back();
    indexes.pop_back();
    tr->changeNode(indexes.back());
}

void TQContext::popDate()
{
    dates.pop_back();
    tr->setDate(dates.back());
    xml_reader->setDate(dates.back());
}

void TQContext::popBranch()
{
    branches.pop_back();
    tr->setBranch(branches.back());
    xml_reader->setBranch(branches.back());
}

void TQContext::popJSON()
{
    localJSONs.pop_back();
    if (localJSONs.empty()) {
        in_local = false;
    }
}

void TQContext::popData(TQData* data)
{
    if (!curr_data.empty() && curr_data.back()==data) {
        curr_data.pop_back();
    }
}

void TQContext::popReskey()
{
    reskeys.pop_back();
}

void TQContext::popFilename()
{
    filenames.pop_back();
}

void TQContext::beginFrame()
{
    path_frames.push_back(paths.size()-1);
    index_frames.push_back(indexes.size()-1);
    identifier_frames.push_back(identifiers.size()-1);
    local_json_frames.push_back(localJSONs.size()-1);
}

void TQContext::endFrame()
{
    path_frames.pop_back();
    index_frames.pop_back();
    identifier_frames.pop_back();
    local_json_frames.pop_back();
}

void TQContext::pushLastFrame()
{
    if (identifier_frames.empty() || path_frames.empty()) {
        return;
    }
    if (!identifiers.empty()) {
        pushIdentifier(identifiers[identifier_frames.back()], indexes[index_frames.back()]);
    }
    paths.push_back(paths[path_frames.back()]);
    int local = local_json_frames.back();
    if (local<0) {
        in_local = false;
    } else {
        localJSONs.push_back(localJSONs[local]);
    }
}

void TQContext::popFrame()
{
    if (identifier_frames.empty() || path_frames.empty()) {
        return;
    }
    if (!identifiers.empty()) {
        popIdentifier();
    }
    paths.pop_back();
    if (!localJSONs.empty()) {
        if (!in_local) {
            in_local = true;
        } else {
            localJSONs.pop_back();
        }
    }
}

string TQContext::fullPath(const string& subpath, string currpath) 
{
    if (subpath=="." || subpath.empty()) {
        return currpath;
    }
    if (subpath[0]=='/') {
        return fullPath(subpath.substr(1), {});
    }
    if (subpath.compare(0,3,"../")==0) {
        size_t pos = currpath.find_last_of(".");
        if (pos==string::npos) {
            currpath.clear();
        } else {
            currpath = currpath.substr(0,pos);
        }
        return fullPath(subpath.substr(3), currpath);
    }
    if (currpath.empty()) {
        return (subpath==".")?"":subpath;
    }
    if (subpath[0]=='[' || subpath[0]=='.') {
        return currpath+subpath;
   }
    return currpath+"."+subpath;
}


JSONValueP TQContext::findLocalPath(const string& path, JSONValueP val, bool allow_projection)
{
    if (val->IsNull()) {
        return JSONValueP(new JSONValue);
    }
    if (path.empty() || path==".") {
        return val;
    }
    if (path[0]=='/') {
        return findLocalPath(path.substr(1), localDocs.back(), allow_projection);
    }
    if (path.compare(0,3,"../")==0) {
        int i = localJSONs.size()-2;
        while (i>=0 && localJSONs[i]->IsArray()) {
            i--;
        }
        if (i<0) {
            return JSONValueP(new JSONValue);
        }
        return findLocalPath(path.substr(3), localJSONs[i], allow_projection);
    }

    size_t i = 0;
    size_t len = path.size();
    while (i!=string::npos && i<len) {
        if (path[i]=='.'|| path[i]=='[') {
            i++;
        }
        size_t next = path.find_first_of(".[]", i);
        string f = (next==string::npos)?path.substr(i):path.substr(i,next-i);
        f = unescape_field_name(f);
        if (i>0 && path[i-1]=='[') {
            if (!val->IsArray()) {
                return JSONValueP(new JSONValue);
            }
            int inx = stoi(f);
            if (val->Size()<=inx) {
                return JSONValueP(new JSONValue);
            }
            val = JSONValueP(&val->GetArray()[inx], DontDeleteJSONValue());
            next++;
        } else {
            if (allow_projection && val->IsArray() && !val->GetArray().Empty()) {
                JSONValueP newval(new JSONValue(rapidjson::kArrayType));
                for (auto& i: val->GetArray()) {
                    if (!i.IsObject()) {
                        continue;
                    }
                    auto it = i.FindMember(f.c_str());
                    if (it!=i.MemberEnd()) {
                        newval->PushBack(JSONValue(it->value,doc->GetAllocator()), doc->GetAllocator());
                    }
                }
                val = newval;
            } else if (!val->IsObject()) {
                return JSONValueP(new JSONValue);
            } else {
                auto it = val->FindMember(f.c_str());
                if (it!=val->MemberEnd()) {
                    val = JSONValueP(&it->value, DontDeleteJSONValue());
                } else return JSONValueP(new JSONValue);
            }
        }

        i = next;
    }
    return val;

}

bool TQContext::isObject(const string& key)
{
    if (in_local) {
        JSONValueP val = findLocalPath(key);
        return val->IsObject();
    }

    string path = unescaped_fullPath(key);
    string s = tr->readMetaNode(path);
    return !s.empty()&&s[0]=='{';
}

bool TQContext::isArray(const string& key)
{
    if (in_local) {
        JSONValueP val = findLocalPath(key);
        return val->IsArray();
    }

    string path = unescaped_fullPath(key);
    string s = tr->readMetaNode(path);
    return !s.empty()&&s[0]=='[';
}

bool TQContext::isString(const string& key)
{
    if (in_local) {
        JSONValueP val = findLocalPath(key);
        return val->IsString();
    }

    string path = unescaped_fullPath(key);
    string s = tr->readMetaNode(path);
    return !s.empty()&&s[0]=='\"';

}

bool TQContext::isDouble(const string& key)
{
    if (in_local) {
        JSONValueP val = findLocalPath(key);
        return val->IsDouble();
    }
    string path = unescaped_fullPath(key);
    string s = tr->readMetaNode(path);
    return !s.empty()&&isdigit(s[0])&&s.find('.')!=string::npos;

}

bool TQContext::isInt(const string& key)
{
    if (in_local) {
        JSONValueP val = findLocalPath(key);
        return val->IsInt64();
    }

    string path = unescaped_fullPath(key);
    string s = tr->readMetaNode(path);
    return !s.empty()&&s[0]>='0'&&s.find('.')==string::npos;

}

bool TQContext::isBool(const string& key)
{
    if (in_local) {
        JSONValueP val = findLocalPath(key);
        return val->IsBool();
    }
    string path = unescaped_fullPath(key);
    string s = tr->readMetaNode(path);
    return s=="%T" || s=="%F";    
}

bool TQContext::exists(const string& key)
{
    if (in_local) {
        JSONValueP val = findLocalPath(key, false);
        return !val->IsNull();
    }

    string path = unescaped_fullPath(key);
    string s = tr->readMetaNode(path);
    return !s.empty();
}


JSONValueP TQContext::getJSON(const string& key)
{
    static JSONValueP nullValue(new JSONValue);
    if (in_local) {
        JSONValueP val = findLocalPath(key);
        return val;
    }

    string path = unescaped_fullPath(key);
    //json_documentP doc = make_shared<rapidjson::Document>();
    JSONValueP value = JSONValueP(new JSONValue(tr->traverse(path, *doc)));
    return value;
}

string TQContext::getMetaKey(const string& key)
{
    if (in_local) {
        return {};
    }
    string path = unescaped_fullPath(key);
    return tr->readMetaNodeKey(path);
}

string TQContext::getString(const string& key)
{
    JSONValueP value = getJSON(key);
    return valToString(value);
}

int64_t TQContext::getInt(const string& key)
{
    JSONValueP value = getJSON(key);
    return valToInt(value);
}

double TQContext::getDouble(const string& key)
{
    JSONValueP value = getJSON(key);
    return valToDouble(value);
}

bool TQContext::getBool(const string& key)
{
    JSONValueP value = getJSON(key);
    return valToBool(value);
}

int TQContext::getArraySize(const string& key)
{
    if (in_local) {
        JSONValueP val = findLocalPath(key);
        if (val->IsArray()) {
            return val->GetArray().Size();
        }
        return 0;
    }
    
    string size_txt = tr->readMetaNode(unescaped_fullPath(key));
    if (size_txt.empty() || size_txt[0]!='[') {
        return 0;
    }
    return stoi(size_txt.substr(1,size_txt.size()-2));
}

ObjectFieldSet TQContext::getMembers(const string& key)
{
    if (in_local) {
        JSONValueP val = findLocalPath(key);
        ObjectFieldSet obj;
        if (val->IsObject()) {
            for (auto& m: val->GetObject()) {
                obj.add(m.name.GetString());
            }
        }
        return obj;
    }
    string obj_str = tr->readMetaNode(unescaped_fullPath(key));
    ObjectFieldSet obj(obj_str);
    return obj;
}


bool compare_data(const TQDataP& a, const TQDataP& b)
{
    return a->compare(b);
}

bool equal_data(const TQDataP& a, const TQDataP& b)
{
    return a->equal(b);
}

Strings TQSimpleKey::getKeys(TQContext& ctx)
{
    Strings res;
    res.push_back(key);
    return res;
}

Strings TQParamKey::getKeys(TQContext& ctx)
{
    Strings res;
    string key;
    ctx.in_key = true;
    if (expr->isString(&ctx)) {
        key = expr->getString(ctx);
    } else if (expr->isInt(&ctx)) {
        key = to_string(expr->getInt(ctx));
    } else if (expr->isDouble(&ctx)) {
        key = to_string(expr->getDouble(ctx));
    } else if (expr->isJSON(&ctx)) {
        key = valToString(expr->getJSON(ctx));
    }
    ctx.in_key = false;
    if (!key.empty()) {
        res.push_back(key);
    }
    return res;
}

bool TQParamKey::isSorted() const
{
    return expr->isSortedKey();
}

Strings TQRegexKey::getKeys(TQContext& ctx)
{
    Strings res;
    ObjectFieldSet obj = ctx.getMembers({});
    for (const string& field: obj) {
        if (r.empty() || regex_match(field, re)) {
            res.push_back(field);
        }
    }
    return res;
}

void TQFuncDefinition::addParam(const string& s)
{
    params.push_back(s);
}

bool TQInnerValueData::compare(const TQDataP& other) const
{
    const TQInnerValueData* o = dynamic_cast<TQInnerValueData*>(other.get());
    return innerData->compare(o->innerData);
}

bool TQInnerValueData::equal(const TQDataP& other) const
{
    const TQInnerValueData* o = dynamic_cast<TQInnerValueData*>(other.get());
    return innerData->equal(o->innerData);
}

TQDataP TQContextMod::makeData()
{
    return TQDataP(new TQContextModData(this));
}

TemplateQueryP TQContextMod::replace(const TemplateQueryP& v)
{
    if (expr) {
        return TemplateQueryP(new TQContextMod(val->replace(v), expr, mode, arrow, new_frame));
    }
    return TemplateQueryP(new TQContextMod(val->replace(v), context, mode, arrow, new_frame));
}

bool TQContextModData::isAggregate(TQContext* ctx) const
{
    return q->val->isAggregate(ctx);
}


bool TQContextModData::processData(TQContext& ctx)
{
    if (!innerData) {
        innerData = q->val->makeData();
    }

    if (ctx.in_get_JSON) {
        return innerData->processData(ctx);
    }
    return contextMod(innerData, ctx);
}

JSONValue TQContextModData::getJSON(TQContext& ctx)
{
    if (!innerData) {
        return {};
    }
    return innerData->getJSON(ctx);
}

bool TQContextModData::contextMod(const TQDataP& data, TQContext& ctx)
{
    bool res = false;
    const string& context = q->context;
    ContextMode mode = q->mode;
    if (q->new_frame && mode!=ContextMode::Arrow) {
        ctx.beginFrame();
    }
    if (mode==ContextMode::None) {
        if (context.empty()) {
            res = data->processData(ctx);
        } else {
            ctx.addToPath(context);
            res = data->processData(ctx);
            ctx.popPath();
        }
    } else if (mode==ContextMode::Eval && q->expr->isString(&ctx)) {
        string added = q->expr->getFieldPath(ctx);
        ctx.addToPath(added);
        res = data->processData(ctx);
        ctx.popPath();
    } else if (mode==ContextMode::Array) {
        int size = ctx.getArraySize(context);
        ctx.addToPath(context);
        for (int i=0; i<size; i++) {
            ctx.addToPath("["+to_string(i)+"]");
            res = data->processData(ctx) || res;
            ctx.popPath();
        }
        
        if (size == 0 && !context.empty() && context!="." && ctx.isObject(context)) {
            res = data->processData(ctx);
        }
        ctx.popPath();
    } else if (mode==ContextMode::Regex) {
        ObjectFieldSet obj = ctx.getMembers({});
        regex re(context);
        for (const string& field: obj) {
            if (context.empty() || regex_match(field, re)) {
                ctx.addToPath(escape_field_name(field));
                res = data->processData(ctx) || res;
                ctx.popPath();
            }
        }
    } else if (mode==ContextMode::AllPaths) {
        res = allPaths(data, ctx);
    } else if (mode==ContextMode::Reskey) {
        ctx.addToPath(ctx.reskey());
        res = data->processData(ctx) || res;
        ctx.popPath();
    } else if (mode==ContextMode::Arrow) {
        res = handleArrow(data, ctx);
    }
    if (q->new_frame) {
        ctx.endFrame();
    }
    return res;
}

bool TQContextModData::allPaths(const TQDataP& data, TQContext& ctx)
{
    bool res = data->processData(ctx);
    int size = ctx.getArraySize("");
    if (size>0) {
        for (int i=0; i<size; i++) {
            ctx.addToPath("["+to_string(i)+"]");
            res = allPaths(data, ctx) || res;
            ctx.popPath();
        }
    } else if (ctx.isObject("")) {
        ObjectFieldSet obj = ctx.getMembers({});
        for (const string& field: obj) {
            ctx.addToPath(field);
            res = allPaths(data, ctx) || res;
            ctx.popPath();
        }
    }
    return res;
}


ArrowOp getArrowOp(const string& str)
{
    if (str.empty() || str[0]!='$') {
        return ArrowOp::None;
    } else if (str=="$self") {
        return ArrowOp::Self;
    } else if (str=="$parent") {
        return ArrowOp::Parent;
    } else if (str=="$ancestors") {
        return ArrowOp::Ancestors;
    } else if (str=="$ancestors_and_self") {
        return ArrowOp::AncestorsAndSelf;
    } else if (str=="$children") {
        return ArrowOp::Children;
    } else if (str=="$descendants") {
        return ArrowOp::Descendants;
    } else if (str=="$descendants_and_self") {
        return ArrowOp::DescendantsAndSelf;
    } else if (str=="$date") {
        return ArrowOp::Date;
    } else if (str=="$branch") {
        return ArrowOp::Branch;
    } else if (str=="$all") {
        return ArrowOp::All;
    } else if (str=="$var") {
        return ArrowOp::Var;
    } else if (str=="$file" || str=="$csv") {
        return ArrowOp::File;
    } else {
        return ArrowOp::Other;
    }
}

bool TQContextModData::handleArrow(const TQDataP& data, TQContext& ctx)
{
    bool res = false;
    const string& context = q->context;
    ArrowOp arrow = q->arrow;
    if (q->new_frame) {
        ctx.beginFrame();
    }

    if (arrow==ArrowOp::AncestorsAndSelf||arrow==ArrowOp::DescendantsAndSelf||arrow==ArrowOp::Self) {
        res = processIdentifier(data, ctx, ctx.identifier()) || res;
    }
    if (arrow==ArrowOp::Var) {
        JSONValueP j = ctx.getVar(context);
        if (j->IsNull()) {
            return false;
        }
        ctx.startLocalJSON(j);
        res = data->processData(ctx);
        ctx.endLocalJSON();
    } else if (arrow==ArrowOp::Other || arrow==ArrowOp::File) {
        if (arrow==ArrowOp::File) {
            TExprFile* f = static_cast<TExprFile*>(q->expr.get());
            string filename = f->getFilename(ctx);
            ctx.pushFilename(filename);
        }
        JSONValueP j = q->expr->getJSON(ctx);
        ctx.startLocalJSON(j);
        res = data->processData(ctx);
        if (arrow==ArrowOp::File) {
            ctx.popFilename();
        }
        ctx.endLocalJSON();
    } else if (arrow==ArrowOp::Date) {
        string date = q->expr.get()->getString(ctx);
        ctx.pushDate(date2key(date));
        res = data->processData(ctx);
        ctx.popDate();
    } else if (arrow==ArrowOp::Branch) {
        string branch = q->expr.get()->getString(ctx);
        ctx.pushBranch(branch);
        res = data->processData(ctx);
        ctx.popBranch();
    } else if (arrow==ArrowOp::None && (context[0]=='\"'||context[0]=='\'')) {
        string identifier = context.substr(1, context.size()-2);
        res = processIdentifier(data, ctx, identifier) || res;
    } else if (q->expr || arrow==ArrowOp::None) {
        string name = (q->expr)?q->expr->getString(ctx):context;
        JSONValueP value = ctx.getJSON(name);

        if (value->IsString()) {
            res = processIdentifier(data, ctx, value->GetString()) || res;            
        } else if (value->IsArray()) {
            for (auto& v: value->GetArray()) {
                res = processIdentifier(data, ctx, v.GetString()) || res;
            }
        } else if (value->IsNull()) {
            return res;
        } else {
            throw QueryError("Expected array or string at ->"+ctx.identifier()+"{"+ctx.fullPath(context)+"}");
        }        
    } else if (arrow==ArrowOp::Parent|| arrow==ArrowOp::Ancestors || arrow==ArrowOp::AncestorsAndSelf) {
        string identifier = ctx.identifier();
        size_t pos;

        while ((pos=identifier.rfind('/'))!=string::npos && pos>5) {
            identifier=identifier.substr(0, pos);
            res = processIdentifier(data, ctx, identifier) || res;

            if (arrow==ArrowOp::Parent) {
                break;
            }
        }
    } else if (arrow==ArrowOp::Descendants||arrow==ArrowOp::DescendantsAndSelf||arrow==ArrowOp::Children||arrow==ArrowOp::All) {
        SimpleQuery query(ctx.xml_reader->get_txn(), ctx.xml_reader->get_base_txn());
        string cur_id = ctx.identifier();

        if (arrow!=ArrowOp::All) {
            query.match_start = cur_id+"/";
        }
        query.beginQuery();
        while(query.readyForNext()) {
            string identifier = query.getNextIdentifier();
            if (identifier.size()>cur_id.size()&&
               (arrow!=ArrowOp::Children||identifier.find('/',cur_id.size()+1)==string::npos)) {
                res = processIdentifier(data, ctx, identifier) || res;
            }
        }
    }
    return res;
}

TQDataP TQContextModOr::makeData()
{
    return TQDataP(new TQContextModOrData(this));
}

TemplateQueryP TQContextModOr::replace(const TemplateQueryP& v)
{
    TQContextModOr* mod = new TQContextModOr;
    for (auto& val: vals) {
        mod->addVal(val->replace(v));
    }
    return TemplateQueryP(mod);
}

bool TQContextModOrData::isAggregate(TQContext* ctx) const
{
    for (auto& val: q->vals) {
        if (val->isAggregate(ctx)) {
            return true;
        }
    }
    return false;
}

bool TQContextModOrData::processData(TQContext& ctx)
{
    for (int i=data.size(); i<q->vals.size(); ++i) {
        data.push_back(q->vals[i]->makeData());
    }

    // In getJSON, we can traverse only one branch, since we don't do any context modification
    if (ctx.in_get_JSON) {
        return data[0]->processData(ctx);
    }
    ctx.pushData(this);
    bool res = false;
    for (auto& d: data) {
        res = d->processData(ctx) || res;
    }
    ctx.popData(this);
    return res;
}

JSONValue TQContextModOrData::getJSON(TQContext& ctx)
{
    JSONValue res;
    for (auto& d: data) {
        res = d->getJSON(ctx);
        if (!res.IsNull()) {
            break;
        }
    }
    return res;
}

TQDataP TQShared::makeData()
{
    return TQDataP(new TQSharedData(this));
}

TemplateQueryP TQShared::replace(const TemplateQueryP& v)
{
    return TemplateQueryP(new TQShared(val->replace(v), id));
}

bool TQSharedData::processData(TQContext& ctx)
{
    if (!innerData) {
        auto& m = q->data_map[q->id];
        auto it = m.find(ctx.data());
        if (it==m.end()) {
            innerData = q->val->makeData();
            m[ctx.data()] = innerData;
        } else {
            innerData = it->second;
        }
    }
    bool res = innerData->processData(ctx);
    return res;
}

JSONValue TQSharedData::getJSON(TQContext& ctx)
{
    if (!innerData) {
        return {};
    }
    return innerData->getJSON(ctx);
}

TQDataP TQArray::makeData()
{
    return TQDataP(new TQArrayData(this));
}

bool TQArrayData::processData(TQContext& ctx)
{
    bool res = false;
    for (TemplateQueryP& val: q->vals) {
        TQDataP new_data = val->makeData();
        if (new_data->processData(ctx)) {
            array.push_back(new_data);
            res = true;
        }
    }
    return res;
}

JSONValue TQArrayData::getJSON(TQContext& ctx)
{
    bool ordered = false;
    for (auto& val: q->vals) {
        if (val->isOrdered()) {
            ordered = true;
        }
    }

    if (ordered) {
        sort(array.begin(), array.end(), compare_data);
        auto it = unique(array.begin(), array.end(), equal_data);
        array.resize(std::distance(array.begin(), it));
    }
    JSONValue res(rapidjson::kArrayType);
    for (TQDataP& v: array) {
        JSONValue j = v->getJSON(ctx);
        if (!j.IsNull()) {
            res.PushBack(j, ctx.doc->GetAllocator());
        }
    }

    return res;
}

bool TQValueWithCond::isAggregate(TQContext* ctx) const
{
    return cond->isAggregate(ctx)|| (val&& val->isAggregate(ctx));
}

TQDataP TQValueWithCond::makeData()
{
    return TQDataP(new TQValueWithCondData(this));
}

TemplateQueryP TQValueWithCond::replace(const TemplateQueryP& v) 
{
    return TemplateQueryP(new TQValueWithCond(val->replace(v), cond));
}

bool TQValueWithCondData::processData(TQContext& ctx)
{
    if (!innerData) {
        innerData = q->val->makeData();
    }
    if (q->cond->isAggregate(&ctx)) {
        // Call test just to calculate the aggregate function
        ctx.pushData(this);
        q->cond->test(ctx);
        ctx.popData(this);
    } else if (!ctx.in_get_JSON && !q->cond->test(ctx)) {
        return false;
    }
    return innerData->processData(ctx);
}

JSONValue TQValueWithCondData::getJSON(TQContext& ctx)
{
    if (q->cond->isAggregate(&ctx)) {
        ctx.pushData(this);
        bool t = q->cond->test(ctx);
        ctx.popData(this);
        if (!t) {
            return {};
        }
    }
    if (!innerData) {
        return {};
    }
    return innerData->getJSON(ctx);
}

bool TQValueWithCondData::isAggregate(TQContext* ctx) const 
{
    return q->isAggregate(ctx);
}

TQDataP TQCondWrapper::makeData()
{
    return this_p;
}

bool TQCondWrapper::isAggregate(TQContext* ctx) const
{
    return cond->isAggregate(ctx);
}

bool TQCondWrapper::processData(TQContext& ctx)
{
    return cond->test(ctx);
}

TQDataP TQObject::makeData()
{
    return TQDataP(new TQObjectData(this));
}

void TQObject::add(const TQKeyP& key, const TemplateQueryP& value, const TemplateQueryP& cond)
{
    if (cond) {
        TQKeyP ck = TQKeyP(new TQDirectiveKey(KeyType::Cond));
        fields.emplace_back(ck, cond);
        conditions_data.push_back(cond->makeData());
    }
    if (key->getKeyType()==KeyType::Variable) {
        local_vars.push_back(key->getName());
        fields.emplace_back(key, value);
    } else if (value) {
        //fields[key] = value;
        fields.emplace_back(key, value);
        if (value->isOrdered()) {
            ordered = true;
        }
    }
}

TQDataP TQObjectData::getFieldData(const string& key, TemplateQueryP& tq, bool sorted)
{
    if (sorted) {
        auto it = sorted_fields.find(key);
        if (it==sorted_fields.end()) {
            return tq->makeData();
        }
        return it->second;
    }
    // unsorted
    auto it = unsorted_fields_map.find(key);
    if (it==unsorted_fields_map.end()) {
        return tq->makeData();
    }
    return unsorted_fields[it->second].second;
}

void TQObjectData::storeData(const string& key, TQDataP& data, bool sorted)
{
    if (sorted) {
        sorted_fields[key] = data;
    } else {
        auto it = unsorted_fields_map.find(key);
        if (it==unsorted_fields_map.end()) {
            unsorted_fields_map[key] = unsorted_fields.size();
            unsorted_fields.push_back(make_pair(key, data));
        } else {
            unsorted_fields[it->second].second = data;
        }
    }
}



bool TQObjectData::processData(TQContext& ctx)
{
    // Test all conditions
    ctx.pushData(this);
    for (auto& m: q->fields) {
        KeyType kt = m.first->getKeyType();
        if (kt==KeyType::Cond || kt==KeyType::Exists) {
            auto cond = m.second->makeData();
            if (!cond->processData(ctx) && !cond->isAggregate(&ctx)) {
                return false;
            }
            continue;
        } else if (kt==KeyType::Notexists) {
            auto expr = m.second->makeData();
            if (expr->processData(ctx)) {
                return false;
            }
            continue;
        } else if (kt==KeyType::Func) {
            TQFuncDefinition* f = static_cast<TQFuncDefinition*>(m.first.get());
            ctx.addFunc(m.first->getName(), m.second, f->params);
            continue;
        } else if (kt==KeyType::Variable) {
            const string& name = m.first->getName();
            TQDataP d= m.second->makeData();
            if (d) {
                d->processData(ctx);
            }
            JSONValueP j = JSONValueP(new JSONValue(d->getJSON(ctx)));
            ctx.addVar(name, j);
            continue;
        } else if (kt==KeyType::Assign) {
            const string& name = m.first->getName();
            TQDataP d= m.second->makeData();
            d->processData(ctx);
            JSONValueP j = JSONValueP(new JSONValue(d->getJSON(ctx)));
            ctx.assignVar(name, j);
            continue;
        } else if (kt==KeyType::Return) {
            if (!returned) {
                returned = m.second->makeData();
            }
            return returned->processData(ctx);
        } else if (kt==KeyType::ReturnIf) {
            if (!returned) {
                TQDataP data = m.second->makeData();
                bool res = data->processData(ctx);
                if (res) {
                    returned = data;
                    return true;
                }
                continue;
            }
            return returned->processData(ctx);
        }
        Strings ks = m.first->getKeys(ctx);
        bool sorted = m.first->isSorted();
        for (const string& k: ks) {
            ctx.pushReskey(k);
            TQDataP data = getFieldData(k, m.second, sorted);
            if (data->processData(ctx)) {
                storeData(k, data, sorted);
            }
            if (data->isOrdered()) {
                ordering[data->getOrderNumber()] = data;
            }
            ctx.popReskey();
        }
    }
    for (const string& var: q->local_vars) {
        ctx.popVar(var);
    }
    ctx.popData(this);
    return !isEmpty();
}

JSONValue TQObjectData::getJSON(TQContext& ctx)
{
    if (returned) {
        return returned->getJSON(ctx);
    }
    auto& alloc = ctx.doc->GetAllocator();

    for (auto& c: q->conditions_data) {
        if (c->isAggregate(&ctx)) {
           ctx.pushData(this);
           bool t = c->processData(ctx);
           ctx.popData(this);
           if (!t) {
              return {};
           }
        }
    }

    set<string> used;
    JSONValue res(rapidjson::kObjectType);
    
    for (auto& m: unsorted_fields) {
        JSONValue key_val(m.first.c_str(), alloc);
        JSONValue val = m.second->getJSON(ctx);
        if (!val.IsNull() || ctx.opt_show_null) {
            res.AddMember(key_val, val, alloc);
        }
    }

    for (auto& m: sorted_fields) {
        
        JSONValue key_val(m.first.c_str(), alloc);
        JSONValue val = m.second->getJSON(ctx);
        if (!val.IsNull() || ctx.opt_show_null) {
            res.AddMember(key_val, val, alloc);
        }
    }
    return res;
}

bool TQObjectData::compare(const TQDataP& other) const
{
    const TQObjectData* o = dynamic_cast<TQObjectData*>(other.get());
    if (!o) {
        return false;
    }
    for (auto& m: o->ordering) {
        if (ordering.find(m.first)==ordering.end()) {
            return true;
        }
    }

    for (auto& m: ordering) {
        auto it = o->ordering.find(m.first);
        if (it==o->ordering.end()) {
            return false;
        }
        if (m.second->compare(it->second)) {
            return true;
        }
        if (it->second->compare(m.second)) {
            return false;
        }
    }
    return false;
}

bool TQObjectData::equal(const TQDataP& other) const
{
    const TQObjectData* o = dynamic_cast<TQObjectData*>(other.get());
    if (!o) {
        return false;
    }
    if (ordering.empty()&& !o->ordering.empty()) {
        return false;
    }

    for (auto& m: ordering) {
        auto it = o->ordering.find(m.first);
        if (it==o->ordering.end()) {
            return false;
        }
        if (!m.second->equal(it->second)) {
            return false;
        }
    }
    return true;
}



TQDataP TQValue::makeData()
{
    return TQDataP(new TQValueData(this));
}

bool TQValueData::processData(TQContext& ctx)
{
    // Read value only once
    if (updated && !q->exp->isAggregate(&ctx)) {
        return false;
    }
    ctx.pushData(this);
    JSONValueP v = q->exp->asJSON(ctx);
    // Create a copy of the JSONValue, ensuring it wont get lost when the source json is gone
    val = JSONValueP(new JSONValue(*v, ctx.doc->GetAllocator()));
    //val = q->exp->asJSON(ctx);
    ctx.popData(this);
    if (val->IsNull()) {
        return ctx.opt_show_null;
    }
    updated = true;
    return true;
}

JSONValue TQValueData::getJSON(TQContext& ctx)
{
    if (!val) {
        return {};
    }
    // It's a bit faster to do a move, but destroys the value in this object
    return JSONValue(*val, ctx.doc->GetAllocator());
//    return std::move(val);
}

bool TQValueData::compare(const TQDataP& other) const
{
    if (q->ord_type == OrderType::None) {
        return false;
    }
    const TQValueData* o = dynamic_cast<TQValueData*>(other.get());
    if (!o) {
        return false;
    }
    const TQValueData* x;
    const TQValueData* y;
    if (q->ord_type==OrderType::Ascend||q->ord_type==OrderType::UAscend) {
        x = this;
        y = o;
    } else {
        x = o;
        y = this;
    }
    if (x->val->IsInt64() && y->val->IsInt64()) {
        return x->val->GetInt64() < y->val->GetInt64();
    } else if (x->val->IsString() && y->val->IsString()) {
        return strcmp(x->val->GetString(), y->val->GetString())<0;
    } else if (x->val->IsString() || y->val->IsString()) {
        string sx = valToString(x->val);
        string sy = valToString(y->val);
        return sx<sy;
    } else if (x->val->IsDouble() || y->val->IsDouble()) {
        return valToDouble(x->val)<valToDouble(y->val);
    }
    return x<y;
}

bool TQValueData::equal(const TQDataP& other) const
{
    if (q->ord_type!=OrderType::UAscend && q->ord_type!=OrderType::UDescend) {
        return false;
    }
    const TQValueData* o = dynamic_cast<TQValueData*>(other.get());
    if (!o) {
        return false;
    }
    return *val==*(o->val);
}

bool TQCondBool::test(TQContext& ctx)
{
    bool r1 = cond1->test(ctx);
    if (op==Operator::NOT) {
        return !r1;
    }
    switch(op) {
        case Operator::AND:
            return r1 && cond2->test(ctx);
        case Operator::OR:
            return r1 || cond2->test(ctx);
        default:
            return false;
    }
}


bool TQStringTest::test(TQContext& ctx)
{
    string v1 = x->getString(ctx);
    string v2 = y->getString(ctx);
    switch (op) {
        case Operator::EQ:
            return v1==v2;
        case Operator::NEQ:
            return v1!=v2;
        case Operator::CONTAINS:
            return v1.find(v2)!=string::npos;
        case Operator::STARTS:
            return v1.rfind(v2,0)==0;
        case Operator::ENDS:
            return v1.compare(v1.size()-v2.size(), v2.size(), v2)==0;
        case Operator::MATCH: {
            regex re(v2);
            return regex_match(v1, re);
        }
        default:
            return false;
    }
}

bool TQCompareTest::test(TQContext& ctx)
{
    if (x->isString(&ctx)||y->isString(&ctx)) {
        return test(x->getString(ctx),y->getString(ctx));
    }

    if (x->isInt(&ctx)&&y->isInt(&ctx)) {
        int64_t v1 = x->getInt(ctx);
        int64_t v2 = y->getInt(ctx);
        return test(v1,v2);
    }
    if (x->isDouble(&ctx)||y->isDouble(&ctx)) {
        return test(x->getDouble(ctx),y->getDouble(ctx));      
    }
    if (x->isBool(&ctx)&&y->isBool(&ctx)) {
        return test(x->getBool(ctx),y->getBool(ctx));
    }
    return test(x->asJSON(ctx),y->asJSON(ctx));
}

bool TQJSONTest::test(TQContext& ctx)
{
    JSONValueP v1 = x->asJSON(ctx);
    JSONValueP v2 = y->asJSON(ctx);
    switch (op) {
        case Operator::EQ:
            return *v1==*v2;
        case Operator::NEQ:
            return *v1!=*v2;
        case Operator::IN:
            return in_operator(v1, v2);
        case Operator::NOTIN:
            return !in_operator(v1, v2);
        default:
            return false;
    }
}

bool TQJSONTest::in_operator(const JSONValueP& v1, const JSONValueP& v2)
{
    if (!v2->IsArray()) {
        return false;
    }
    for (auto& k: v2->GetArray()) {
        if (k==*v1) {
            return true;
        }
    }
    return false;
}


bool TQExistsTest::test(TQContext& ctx)
{
    return x->exists(ctx);
}

bool TQTypeTest::test(TQContext& ctx)
{
    string path = x->getFieldPath(ctx);
    switch (op) {
        case Operator::IS_ARRAY:
            return ctx.isArray(path);
        case Operator::IS_OBJECT:
            return ctx.isObject(path);
        case Operator::IS_LITERAL:
            return ctx.isString(path) || ctx.isDouble(path) || ctx.isInt(path) || ctx.isBool(path);
        case Operator::IS_STRING:
            return ctx.isString(path);
        case Operator::IS_NUMBER:
            return ctx.isInt(path) || ctx.isDouble(path);
        case Operator::IS_INT:
            return ctx.isInt(path);
        case Operator::IS_FLOAT:
            return ctx.isDouble(path);
        case Operator::IS_BOOL:
            return ctx.isBool(path);

    }
    return false;
}


JSONValueP TExpression::asJSON(TQContext& ctx)
{
    if (isJSON(&ctx)) {
        return getJSON(ctx);
    }
    if (isString(&ctx)) {
        string s = getString(ctx);
        return JSONValueP(new JSONValue(s.c_str(), s.size(), ctx.doc->GetAllocator()));
    } else if (isDouble(&ctx)) {
        double d = getDouble(ctx);
        return JSONValueP(new JSONValue(d));
    } else if (isInt(&ctx)) {
        int64_t i = getInt(ctx);
        return JSONValueP(new JSONValue(i));
    } else if (isBool(&ctx)) {
        bool b = getBool(ctx);
        return JSONValueP(new JSONValue(b));
    }
    return JSONValueP(new JSONValue);
}

bool TExprField::isString(TQContext* ctx)
{
    if (!ctx) return true;
    string field = getFieldName(ctx);
    return ctx->isString(field);
}

bool TExprField::isDouble(TQContext* ctx)
{
    if (!ctx) return true;
    string field = getFieldName(ctx);
    return ctx->isDouble(field);

}

bool TExprField::isInt(TQContext* ctx)
{
    if (!ctx) return true;
    string field = getFieldName(ctx);
    return ctx->isInt(field);
}

bool TExprField::isBool(TQContext* ctx)
{
    if (!ctx) return true;
    string field = getFieldName(ctx);
    return ctx->isBool(field);
}


bool TExprField::exists(TQContext& ctx)
{
    string field = getFieldName(&ctx);
    return ctx.exists(field);
}

string TExprField::getFieldName(TQContext* ctx)
{
    string name = (expr)?expr->getString(*ctx):field;
    if (name==".") {
        return {};
    }
    return name;
}

JSONValueP TExprField::getJSON(TQContext& ctx)
{
    string field = getFieldName(&ctx);
    //JSONValueP res = ctx.getJSON(field);
    //return JSONValueP(new JSONValue(*res, ctx.doc->GetAllocator()));
    return ctx.getJSON(field);
}

string TExprField::getString(TQContext& ctx)
{
    string field = getFieldName(&ctx);
    return ctx.getString(field);
}

int64_t TExprField::getInt(TQContext& ctx)
{
    string field = getFieldName(&ctx);
    return ctx.getInt(field);
}

double TExprField::getDouble(TQContext& ctx)
{
    string field = getFieldName(&ctx);
    return ctx.getDouble(field);
}

bool TExprField::getBool(TQContext& ctx)
{
    string field = getFieldName(&ctx);
    return ctx.getBool(field);
}

bool TExprChangepath::exists(TQContext& ctx)
{
    adjustPath(ctx);
    bool res = exp->exists(ctx);
    restorePath(ctx);
    return res;
}

JSONValueP TExprChangepath::getJSON(TQContext& ctx)
{
    if (isField()) {
        return ctx.getJSON(getFieldPath(ctx));
    }

    adjustPath(ctx);
    JSONValueP res = exp->asJSON(ctx);
    restorePath(ctx);
    return res;
}

string TExprChangepath::getString(TQContext& ctx)
{
    if (isField()) {
        return ctx.getString(getFieldPath(ctx));
    }
    adjustPath(ctx);
    string res = exp->getString(ctx);
    restorePath(ctx);
    return res;
}

int64_t TExprChangepath::getInt(TQContext& ctx)
{
    if (isField()) {
        return ctx.getInt(getFieldPath(ctx));
    }
    adjustPath(ctx);
    int64_t res = exp->getInt(ctx);
    restorePath(ctx);
    return res;
}

double TExprChangepath::getDouble(TQContext& ctx)
{
    if (isField()) {
        return ctx.getDouble(getFieldPath(ctx));
    }

    adjustPath(ctx);
    double res = exp->getDouble(ctx);
    restorePath(ctx);
    return res;
}

bool TExprChangepath::getBool(TQContext& ctx)
{
    if (isField()) {
        return ctx.getBool(getFieldPath(ctx));
    }

    adjustPath(ctx);
    bool res = exp->getBool(ctx);
    restorePath(ctx);
    return res;
}

string TExprChangepath::getFieldPath(TQContext& ctx)
{
    if (op==Operator::ROOT) {
        return "/"+exp->getFieldPath(ctx);
    } else if (op==Operator::UP) {
        return "../"+exp->getFieldPath(ctx);
    }
    return {};
}


void TExprChangepath::adjustPath(TQContext& ctx)
{
    if (op==Operator::ROOT) {
        ctx.adjustPath("");
    } else if (op==Operator::UP) {
        string path = ctx.path();
        size_t pos = path.find_last_of(".");
        if (pos==string::npos) {
            ctx.adjustPath("");
        } else {
            ctx.adjustPath(path.substr(0, pos));
        }
    } else if (op==Operator::PREVID) {
        ctx.pushLastFrame();
    }
}

void TExprChangepath::restorePath(TQContext& ctx)
{
    if (op==Operator::PREVID) {
        ctx.popFrame();
    } else {
        ctx.popPath();
    }
}

JSONValueP TExprITE::getJSON(TQContext& ctx)
{
    if (cond->test(ctx)) {
        return th->asJSON(ctx);
    } else {
        return el->asJSON(ctx);
    }
}

string TExprChangeCase::getString(TQContext& ctx)
{
    string res = exp->getString(ctx);
    if (lower) {
        return to_lowercase(res);
    } else {
        return to_uppercase(res);
    }
}


string TExprPath::getString(TQContext& ctx)
{
    return unescape_field_name(ctx.path());
}

string TExprIndex::getString(TQContext& ctx)
{

    string path=ctx.path();
    size_t starts = path.find_last_of('[');
    if (starts==string::npos) {
        return {};
    }
    size_t ends = path.find_first_of(']', starts);
    if (ends==string::npos) {
        return {};
    }
    return path.substr(starts+1,ends-starts-1);
}

int64_t TExprIndex::getInt(TQContext& ctx)
{
    string s = getString(ctx);
    return stoi(s);
}

string TExprKey::getString(TQContext& ctx)
{
    string res = ctx.path();
    size_t pos = res.rfind('.');
    if (pos!=string::npos) {
        res = res.substr(pos+1);
    }
    pos = res.find('[');
    if (pos!=string::npos) {
        res = res.substr(0,pos);
    }
    return unescape_field_name(res);
}

string TExprReskey::getString(TQContext& ctx)
{
    return ctx.reskey();
}

string TExprFilename::getString(TQContext& ctx)
{
    return ctx.filename();
}

string TExprEnv::getString(TQContext& ctx)
{
    string env = name->getString(ctx);
    return getenv(env.c_str());
}

string TExprIdentifier::getString(TQContext& ctx)
{
    if (parts==0) {
        return ctx.identifier();
    }
    string id = ctx.identifier();
    return getMainIdPart(id, parts);
}

string TExprXMLType::getString(TQContext& ctx)
{
    pugi::xml_document xmldoc;
    pugi::xml_node n = getXML(ctx, xmldoc);
    ostringstream oss;
    n.print(oss);
    return oss.str();
}


pugi::xml_node TExprXML::getXML(TQContext& ctx, pugi::xml_document& xmldoc)
{
    bool success = ctx.xml_reader->getNodeXML(ctx.index(), xmldoc);
    if (!success) {
        return {};
    }
    ctx.xml_reader->no_children = no_children;
    pugi::xml_node top = xmldoc.first_child();
    ctx.xml_reader->addNamespace(top, ctx.identifier());
    ctx.xml_reader->traverse(top);
    return xmldoc;
}

string TExprAttr::getString(TQContext& ctx)
{
    pugi::xml_document xmldoc;
    bool success = ctx.xml_reader->getNodeXML(ctx.index(), xmldoc);
    if (!success) {
        return {};
    }
    pugi::xml_attribute attr = xmldoc.first_child().attribute(attr_name.c_str());
    if (attr.empty()) {
        return {};
    }

    return attr.value();
}

string TExprNode::getString(TQContext& ctx)
{
    pugi::xml_document xmldoc;
    bool success = ctx.xml_reader->getNodeXML(ctx.index(), xmldoc);
    if (!success) {
        return {};
    }

    return xmldoc.first_child().name();
}

pugi::xml_node TExprChild::getXML(TQContext& ctx, pugi::xml_document& xmldoc)
{
    bool success = ctx.xml_reader->getNodeXML(ctx.index(), xmldoc);
    if (!success) {
        return {};
    }
    pugi::xml_node top = xmldoc.first_child().child(name.c_str());
    if (top.empty()) {
        return {};
    }
    ctx.xml_reader->no_children = false;
    ctx.xml_reader->traverse(top);
    return top;
}

TExprXPath::TExprXPath(const string& path_expr, bool no_children_)
   : query(path_expr.c_str()), TExprXML(no_children_)
{
}

pugi::xpath_node TExprXPath::getXPathNode(TQContext& ctx, pugi::xml_document& xmldoc)
{
    pugi::xml_node top = TExprXML::getXML(ctx, xmldoc);
    pugi::xpath_node_set nodes = query.evaluate_node_set(top);
    if (nodes.empty()) {
        return {};
    }
    return nodes[0];
}

pugi::xml_node TExprXPath::getXML(TQContext& ctx, pugi::xml_document& xmldoc)
{
    pugi::xpath_node ret = getXPathNode(ctx, xmldoc);
    return ret.node();
}

string TExprXPath::getString(TQContext& ctx)
{
    pugi::xml_document xmldoc;
    pugi::xpath_node ret = getXPathNode(ctx, xmldoc);
    if (!ret.node().empty()) {
        ostringstream oss;
        ret.node().print(oss);
        return oss.str();
    }
    return ret.attribute().value();
}


bool TExprInFilter::getBool(TQContext& ctx)
{
    pugi::xml_document xmldoc;
    bool success = ctx.xml_reader->getNodeXML(ctx.index(), xmldoc);
    if (!success) {
        return false;
    }
    const string& name = xmldoc.first_child().name();
    return ctx.xml_reader->doc_conf().isInFilter(name, filter_name);

}


string TExprText::getString(TQContext& ctx)
{
    pugi::xml_document xmldoc;
    pugi::xml_node node = arg->getXML(ctx, xmldoc);
    return XMLToString(node);
}

string TExprText::XMLToString(const pugi::xml_node& n)
{
    if (n.type()==pugi::node_pcdata) {
        return n.value();
    }
    string res;
    for (auto& child: n.children()) {
        res+=XMLToString(child);
    }
    return res;
}

bool TExprBinaryOp::isDouble(TQContext* ctx)
{
    return (arg1->isDouble(ctx)||arg2->isDouble(ctx))&&!isString(ctx);
}

bool TExprBinaryOp::isInt(TQContext* ctx)
{
    return arg1->isInt(ctx)&&arg2->isInt(ctx);
}

bool TExprBinaryOp::isString(TQContext* ctx)
{
    return arg1->isString(ctx)||arg2->isString(ctx);
}


int64_t TExprBinaryOp::getInt(TQContext& ctx)
{
    int64_t x = arg1->getInt(ctx);
    int64_t y = arg2->getInt(ctx);
    switch (op) {
        case Operator::PLUS:
            return x+y;
        case Operator::MINUS:
            return x-y;
        case Operator::MULTIPLY:
            return x*y;
        case Operator::DIVISION:
            if (y!=0) {
                return x/y;
            } else return 0;
        case Operator::MOD:
            return x%y;
    }
    return 0;
}

double TExprBinaryOp::getDouble(TQContext& ctx)
{
    double x = arg1->getDouble(ctx);
    double y = arg2->getDouble(ctx);
    switch (op) {
        case Operator::PLUS:
            return x+y;
        case Operator::MINUS:
            return x-y;
        case Operator::MULTIPLY:
            return x*y;
        case Operator::DIVISION:
            if (y!=0) {
                return x/y;
            } else return 0;
        case Operator::MOD:
            return (int)x%(int)y;
    }
    return 0;
}

string TExprBinaryOp::getString(TQContext& ctx)
{
    string x = arg1->getString(ctx);
    string y = arg2->getString(ctx);
    switch (op) {
        case Operator::PLUS:
            return x+y;
    }
    return {};
}

string TExprSubstr::getString(TQContext& ctx)
{
    string s = str->getString(ctx);
    int64_t st = start->getInt(ctx);
    if (st>=s.size()) {
        return {};
    }
    if (length) {
        int64_t l = length->getInt(ctx);
        return s.substr(st, l);
    } else {
        return s.substr(st);
    }
}

JSONValueP TExprFind::getJSON(TQContext& ctx)
{
    auto& alloc = ctx.doc->GetAllocator();
    JSONValueP res(new JSONValue(rapidjson::kArrayType));
    string s1 = str->getString(ctx);
    string s2 = searched->getString(ctx);
    if (!case_sensitive) {
        s1 = to_lowercase(s1);
        s2 = to_lowercase(s2);
    }
    size_t pos = 0;
    while ((pos=s1.find(s2, pos))!=string::npos) {
        JSONValue num((unsigned)pos);
        res->PushBack(num, alloc);
        pos++;
    }
    return res;
}

string TExprReplace::getString(TQContext& ctx)
{
    string s = source->getString(ctx);
    string f = from->getString(ctx);
    string t = to->getString(ctx);
    string res;
    size_t pos = 0;
    while (pos<s.size()) {
        size_t next = s.find(f,pos);
        if (next==string::npos) {
            res += s.substr(pos);
            break;
        }
        res+= s.substr(pos,next-pos) + t;
        pos = next+f.size();
        if (!replace_all) {
            res+=s.substr(pos);
            break;
        }
    }
    return res;
}


string TExprSubfield::getSubpath(TQContext& ctx)
{
    if (!subpath.empty()) {
        return subpath;
    }
    if (!expr) {
        return {};
    }
    string s;
    if (is_index) {
        size_t n = expr->getInt(ctx);
        s = "["+to_string(n)+"]";
    } else {
        s = expr->getFieldPath(ctx);
        s = "."+s;
    }
    return s;
}

bool TExprSubfield::isString(TQContext* ctx)
{
    if (!ctx) {
        return false;
    }
    if (isField()) {
        return ctx->isString(getFieldPath(*ctx));
    }
    return getJSON(*ctx)->IsString();;
}

bool TExprSubfield::isDouble(TQContext* ctx)
{
    if (!ctx) {
        return false;
    }
    if (isField()) {
        return ctx->isDouble(getFieldPath(*ctx));
    }
    return getJSON(*ctx)->IsDouble();

}

bool TExprSubfield::isInt(TQContext* ctx)
{
    if (!ctx) {
        return false;
    }
    if (isField()) {
        return ctx->isInt(getFieldPath(*ctx));
    }
    return getJSON(*ctx)->IsInt();
}

bool TExprSubfield::isBool(TQContext* ctx)
{
    if (!ctx) {
        return false;
    }
    if (isField()) {
        return ctx->isBool(getFieldPath(*ctx));
    }
    return getJSON(*ctx)->IsBool();
}

bool TExprSubfield::exists(TQContext& ctx)
{
    string field = getFieldPath(ctx);
    return ctx.exists(field);
}


JSONValueP TExprSubfield::getJSON(TQContext& ctx)
{
    if (isField()) {
        return ctx.getJSON(getFieldPath(ctx));
    }
    if (arg->isField() && subpath.empty()&&!expr && is_index) {
        JSONValueP res(new JSONValue(rapidjson::kArrayType));
        string path = arg->getFieldPath(ctx);
        if (path==".") {
            path.clear();
        }
        int size = ctx.getArraySize(path);
        for (int i=0; i<size; ++i) {
            JSONValueP v = ctx.getJSON(path+"["+to_string(i)+"]");
            res->PushBack(*v,ctx.doc->GetAllocator());
        }
        if (size==0) {
            JSONValueP v = ctx.getJSON(path);
            res->PushBack(*v,ctx.doc->GetAllocator());
        }
        return res;
    }

    JSONValueP val = arg->getJSON(ctx);
    if (val->IsNull()) {
        return JSONValueP(new JSONValue);
    }
    string s = getSubpath(ctx);
    if (s.empty()||s==".") {
        return JSONValueP(new JSONValue);
    }
    ctx.startLocalJSON(val);
    JSONValueP res = ctx.findLocalPath(s);
    ctx.endLocalJSON();
    return res;
}

string TExprSubfield::getString(TQContext& ctx)
{
    JSONValueP v = getJSON(ctx);
    return valToString(v);
}

int64_t TExprSubfield::getInt(TQContext& ctx)
{
    JSONValueP v = getJSON(ctx);
    return valToInt(v);
}

double TExprSubfield::getDouble(TQContext& ctx)
{
    JSONValueP v = getJSON(ctx);
    return valToDouble(v);
}

bool TExprSubfield::getBool(TQContext& ctx)

{
    JSONValueP v = getJSON(ctx);
    return valToBool(v);
}




string TExprSubfield::getFieldPath(TQContext& ctx)
{
    return arg->getFieldPath(ctx)+getSubpath(ctx);
}

int64_t TExprSize::getInt(TQContext& ctx)
{
    JSONValueP v = array->getJSON(ctx);
    if (!v->IsArray()) {
        return 0;
    }
    return v->GetArray().Size();
}

int64_t TExprLength::getInt(TQContext& ctx)
{
    string s = str->getString(ctx);
    return s.length();
}

JSONValueP TExprSplit::getJSON(TQContext& ctx)
{
    string s = expr->getString(ctx);
    string d = delim->getString(ctx);
    vector<string> vec = split_string(s, d);
    auto& alloc = ctx.doc->GetAllocator();
    JSONValueP res(new JSONValue(rapidjson::kArrayType));
    for (const string& s: vec) {
        JSONValue str_val(s.c_str(),alloc);
        res->PushBack(str_val, alloc);
    }
    return res;
    
}

string TExprJoin::getString(TQContext& ctx)
{
    JSONValueP v = expr->getJSON(ctx);
    string d = delim->getString(ctx);
    return joinAllVals(*v, d);
}

int64_t TExprToTime::getInt(TQContext& ctx)
{
    string s = expr->getString(ctx);
    return stringToTime(s,format);
}

string TExprTimeToString::getString(TQContext& ctx)
{
    time_t tm = expr->getInt(ctx);
    return timeToString(tm, format);    
}

int64_t TExprLastChange::getInt(TQContext& ctx)
{
    if (!for_meta && ! for_tree) {
        auto p = ctx.xml_reader->read_node_revision(ctx.index());
        string date = node_date(p.first);
        return base64_decode(date);
    } else if (for_meta) {
        string key = ctx.getMetaKey(arg->getFieldPath(ctx));
        string date = node_date(key);
        return base64_decode(date);
    }
    return 0;
}

bool TExprTypeCast::isInt(TQContext* ctx) 
{
    if (t==CastType::Int) {
        return true;
    }
    if (t==CastType::Number) {
        if (arg->isString(ctx)) {
            string res = arg->getString(*ctx);
            return res.find('.')==string::npos;
        } else {
            return arg->isInt(ctx);
        }
    }
    return false;
}

bool TExprTypeCast::isDouble(TQContext* ctx) 
{
    if (t==CastType::Double) {
        return true;
    }
    if (t==CastType::Number) {
        if (arg->isString(ctx)) {
            string res = arg->getString(*ctx);
            return res.find('.')!=string::npos;
        } else {
            return arg->isDouble(ctx);
        }
    }
    return false;
}

string TExprTypeCast::getString(TQContext& ctx)
{
    auto v = arg->asJSON(ctx);
    return valToString(v);
}

int64_t TExprTypeCast::getInt(TQContext& ctx)
{
    if (arg->isString(&ctx)) {
        try {
            return stoll(arg->getString(ctx));
        } catch (const std::invalid_argument& ia) {
            return {};
        }
    }
    return arg->getInt(ctx);
}

double TExprTypeCast::getDouble(TQContext& ctx)
{
    if (arg->isString(&ctx)) {
        try {
            return stod(arg->getString(ctx));
        } catch (const std::invalid_argument& ia) {
            return {};
        }
    }
    return arg->getDouble(ctx);
}

bool TExprTypeCast::getBool(TQContext& ctx)
{
    if (arg->isString(&ctx)) {
        return arg->getString(ctx)=="true";
    }
    return arg->getBool(ctx);
}

TQAggregateDataP TExprAggregate::getData(TQContext& ctx)
{
    TQData* ctx_data = ctx.data();
    auto it = data_map.find(ctx_data);
    if (it!=data_map.end()) {
        return it->second;
    }
    return data_map[ctx_data] = makeData();
}

bool TExprAggregate::isInt(TQContext* ctx)
{
    if (!ctx) {
        return true;
    }
    return getData(*ctx)->isInt(ctx);
}

bool TExprAggregate::isDouble(TQContext* ctx)
{
    if (!ctx) {
        return true;
    }
    return getData(*ctx)->isDouble(ctx);
}

int64_t TExprAggregate::getInt(TQContext& ctx)
{
    return getData(ctx)->getInt(ctx);
}

double TExprAggregate::getDouble(TQContext& ctx)
{
    return getData(ctx)->getDouble(ctx);
}

TQAggregateDataP TExprCount::makeData()
{
    return TQAggregateDataP(new TExprCountData(this));
}

int64_t TExprCountData::getInt(TQContext& ctx)
{
    if (ctx.in_get_JSON) {
        return count;
    }
    return ++count;
}

TQAggregateDataP TExprSum::makeData()
{
    return TQAggregateDataP(new TExprSumData(this));
}

bool TExprSum::isDouble(TQContext* ctx)
{
    return arg->isDouble(ctx) || getData(*ctx)->isDouble(ctx);
}

int64_t TExprSumData::getInt(TQContext& ctx)
{
    if (expr->isDouble(&ctx)) {
        return getDouble(ctx);
    }
    if (ctx.in_get_JSON) {
        return sum;
    }
    return sum+=expr->arg->getInt(ctx);
}

double TExprSumData::getDouble(TQContext& ctx)
{
    if (ctx.in_get_JSON) {
        return is_double?sum_d:sum;
    }
    if (!is_double) {
        sum_d = sum;
    }
    is_double = true;
    return sum_d += expr->arg->getDouble(ctx);
}


TQAggregateDataP TExprAvg::makeData()
{
    return TQAggregateDataP(new TExprAvgData(this));
}

int64_t TExprAvgData::getInt(TQContext& ctx)
{
    return getDouble(ctx);
}

double TExprAvgData::getDouble(TQContext& ctx)
{
    if (ctx.in_get_JSON) {
        return sum/count;
    }
    sum+=expr->arg->getDouble(ctx);
    count++;
    return sum/count;
}

TQAggregateDataP TExprMinmax::makeData()
{
    return TQAggregateDataP(new TExprMinmaxData(this));
}

bool TExprMinmax::isDouble(TQContext* ctx)
{
    return arg->isDouble(ctx) || getData(*ctx)->isDouble(ctx);
}

int64_t TExprMinmaxData::getInt(TQContext& ctx)
{
    if (expr->isDouble(&ctx)) {
        return getDouble(ctx);
    }
    if (ctx.in_get_JSON) {
        return num;
    }
    int64_t i = expr->arg->getInt(ctx);
    if (first) {
        num = i;
        first = false;
    } else if (expr->max && num<i) {
        num=i;
    } else if (!expr->max && num>i) {
        num=i;
    }
    
    return num;
}

double TExprMinmaxData::getDouble(TQContext& ctx)
{
    if (ctx.in_get_JSON) {
        return is_double?num_d:num;
    }
    double d = expr->arg->getDouble(ctx);
    if (!is_double) {
        num_d = num;
    }
    is_double = true;
    if (first) {
        num_d = d;
        first = false;
    } else if (expr->max && num_d<d) {
        num_d=d;
    } else if (!expr->max && num_d>d) {
        num_d=d;
    }
    
    return num_d;
}

JSONValueP TExprPrev::getJSON(TQContext& ctx)
{
    JSONValueP v = JSONValueP(new JSONValue(ctx.data()->getJSON(ctx)));
    if (v->IsNull()) {
        return dfault->asJSON(ctx);
    }
    return v;
}

int64_t TExprPrev::getInt(TQContext& ctx)
{
    JSONValueP v = getJSON(ctx);
    return valToInt(v);
}

string TExprPrev::getString(TQContext& ctx)
{
    JSONValueP v = getJSON(ctx);
    return valToString(v);
}

JSONValueP TExprCall::getJSON(TQContext& ctx)
{
    TQData* ctx_data = ctx.data();
    TQDataP call;
    Function& f = ctx.getFunc(proc);
    if (!f.body) {
        return JSONValueP(new JSONValue);
    }
    if (ctx.in_key) {
        call = f.body->makeData();
    } else {
        auto it = data_map.find(ctx_data);
        if (it!=data_map.end()) {
            call =  it->second;
        } else {
            call =  data_map[ctx_data] = f.body->makeData();
        }
    }
    if (args.size()!=f.params.size()) {
        return JSONValueP(new JSONValue);
    }
    for (int i=0; i<f.params.size(); ++i) {
        ctx.addVar(f.params[i], args[i]->asJSON(ctx));
    }

    if (!ctx.in_get_JSON) {
        call->processData(ctx);
    }
    JSONValueP res = JSONValueP(new JSONValue(call->getJSON(ctx)));
    for (int i=0; i<f.params.size(); ++i) {
        ctx.popVar(f.params[i]);
    }

    return res;
}

bool TExprVar::isString(TQContext* ctx)
{
    if (!ctx) {
        return false;
    }
    JSONValueP j = ctx->getVar(name);
    return (j->IsString());
}

bool TExprVar::isDouble(TQContext* ctx)
{
    if (!ctx) {
        return false;
    }
    JSONValueP j = ctx->getVar(name);
    return (j->IsDouble());
}

bool TExprVar::isInt(TQContext* ctx)
{
    if (!ctx) {
        return false;
    }
    JSONValueP j = ctx->getVar(name);
    return (j->IsInt64());
}

bool TExprVar::isBool(TQContext* ctx)
{
    if (!ctx) {
        return false;
    }
    JSONValueP j = ctx->getVar(name);
    return (j->IsBool());
}

bool TExprVar::isAggregate(TQContext* ctx)
{
    return false;
}

JSONValueP TExprVar::getJSON(TQContext& ctx)
{
    JSONValueP j = ctx.getVar(name);
    return j;
}

string TExprVar::getString(TQContext& ctx)
{
    JSONValueP j = ctx.getVar(name);
    return valToString(j);
}

int64_t TExprVar::getInt(TQContext& ctx)
{
    JSONValueP j = ctx.getVar(name);
    return valToInt(j);

}

double TExprVar::getDouble(TQContext& ctx)
{
    JSONValueP j = ctx.getVar(name);
    return valToDouble(j);
}

bool TExprVar::getBool(TQContext& ctx)
{
    JSONValueP j = ctx.getVar(name);
    return valToBool(j);
}

bool TExprVar::exists(TQContext& ctx)
{
    JSONValueP j = ctx.getVar(name);
    if (j->IsNull()) {
        return false;
    }
    if (j->IsObject() && j->ObjectEmpty()) {
        return false;
    }
    if (j->IsArray() && j->GetArray().Empty()) {
        return false;
    }
    return true;
}

JSONValueP TExprFile::getJSON(TQContext& ctx)
{
    string file_name = filename->getString(ctx);
    if (file_name.empty()) {
        return JSONValueP(new JSONValue);
    }
    FILE* fp = fopen(file_name.c_str(), "r");
    if (!fp) {
        cerr<<"Warning: failed opening file "<<file_name<<endl;
        return JSONValueP(new JSONValue);
    }

    json = JSONValueP(new Document);
    Document& json_doc = *static_cast<Document*>(json.get());

    char readBuffer[65536];
    FileReadStream jsonfile(fp, readBuffer, sizeof(readBuffer));
    json_doc.ParseStream<kParseCommentsFlag>(jsonfile);
    fclose(fp);
    if (json_doc.HasParseError()) {
        cerr<<"File: "<<file_name<<" Not a valid JSON\n";
        cerr<<"Error(offset "<<static_cast<unsigned>(json_doc.GetErrorOffset())<<"): "<<GetParseError_En(json_doc.GetParseError())<<endl;
        return JSONValueP(new JSONValue);        
    }
    return json;
}

bool TExprFile::exists(TQContext& ctx)
{
    string file_name = filename->getString(ctx);
    FILE* fp = fopen(file_name.c_str(), "r");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

string TExprFile::getFilename(TQContext& ctx)
{
    return filename->getString(ctx);
}

JSONValueP TExprCSV::getJSON(TQContext& ctx)
{
    string file_name = filename->getString(ctx);
    if (file_name.empty()) {
        return JSONValueP(new JSONValue);
    }
    ifstream is(file_name);
    if (is.fail()) {
        cerr<<"Warning: failed opening file "<<file_name<<endl;
        return JSONValueP(new JSONValue);
    }

    json = readCSV(is, delim, with_header);
    return json;
}


} //namespace xcite
