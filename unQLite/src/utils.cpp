// Copyright (c) 2022 by Sela Mador-Haim

#include "utils.h"
#include <algorithm>
#include <map>
#include <vector>
#include <set>
#include <iostream>

using namespace std;

namespace xcite {

static const vector<string> openQuotes = {
        "\"",
        "``",
        //"‘",
        "“"
};
static const vector<string> closeQuotes = {
        "\"",
        "\'\'",
        //"’",
        "”"
};

int isOpenQuotes(const string& s, size_t pos)
{
    for (string q: openQuotes) {
        if (s.substr(pos, q.size())==q) {
            return q.size();
        }
    }
    return 0;
}

int isCloseQuotes(const string& s, size_t pos)
{
    for (string q: closeQuotes) {
        if (s.substr(pos, q.size())==q) {
            return q.size();
        }
    }
    return 0;
}

string stripQuotes(string s)
{
    for (string q: openQuotes) {
        if (s.size()>= q.size() && s.substr(0,q.size())==q) {
            s = s.substr(q.size());
            break;
        }
    }
    int len = s.size();
    for (string q: closeQuotes) {
        if (s.size()>=q.size() && s.substr(len-q.size())==q) {
            s = s.substr(0,len-q.size());
            break;
        }
    }
    return s;
}


int roman_digit(char c)
{
    if (c>='a' && c<='z') {
        c = c -'a' + 'A';
    }
    switch (c) {
        case 'I': return 1;
        case 'V': return 5;
        case 'X': return 10;
        case 'L': return 50;
        case 'C': return 100;
        case 'D': return 500;
        case 'M': return 1000;
        default: return -1;
    }
}

LetterCase getCase(const string& s)
{
    bool haveUpper = false;
    bool haveLower = false;
    for (char c: s) {
        if (islower(c)) {
            haveLower = true;
        } else if (isupper(c)) {
            haveUpper = true;
        }
    }
    if (haveUpper && haveLower) {
        return LetterCase::Mixed;
    } else if (haveUpper) {
        return LetterCase::Upper;
    } else if (haveLower) {
        return LetterCase::Lower;
    }
    return LetterCase::None;
}


int roman2int(const string& roman)
{
    int res = 0;
    int prev_digit = 0;
    int len = roman.length();
    for (int i=0; i<len; i++) {
        int digit = roman_digit(roman[i]);
        if (digit==-1) {
            return -1;
        }
        int next_digit;
        if (i!=len-1 && digit<(next_digit=roman_digit(roman[i+1]))) {
            if (digit*5!=next_digit && digit*10!=next_digit) {
                return -1;
            }
            res-=digit;
        } else {
            res +=digit;
        }
    }
    return res;
}


string int2roman(int n, bool uppercase)
{
    static const string ones[] = {"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"};
    static const string tens[] = {"", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC"};
    static const string hundreds[] = {"", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM"};

    if (n<1 || n>999) {
        return string();
    }
    string res = ones[n%10];
    n/=10;
    res = tens[n%10]+res;
    n/=10;
    res = hundreds[n]+res;
    if (!uppercase) {
        transform(res.begin(), res.end(), res.begin(), ::tolower);
    }
    return res;
}

bool isRepeatedChar(const string& s)
{
    char repeated = s[0];
    if (!isalpha(repeated)) {
        return false;
    }
    for (char c: s) {
        if (c!=repeated) {
            return false;
        }
    }
    return true;
}

// Letter enumeration to int
int letter2int(const string& letter)
{
    if (!isRepeatedChar(letter)) {
        return -1;
    }
    int res;
    char ch = letter[0];
    if (ch>='A' && ch<='Z') {
        res = ch - 'A'+1;
    } else if (ch>='a' && ch<='z') {
        res = ch - 'a'+1;
    } else {
        return -1;
    }
    return res+(letter.length()-1)*26;
}

string int2letter(int n, bool uppercase)
{
    int repeat = (n-1)/26;
    int reminder = n-1-repeat*26;
    char c;
    if (uppercase) {
        c = 'A'+reminder;
    } else {
        c = 'a'+reminder;
    }
    string res(repeat+1,c);
    cerr<<"Numer: "<<n<<", Letter: "<<res<<endl;
    return res;
}

string normalize_string(const string& s)
{
    // Add to map as needed
    static const map<string,string> chmap = {
        {u8"\u2013", "-"},
        {u8"\u2014", "-"},
        {"__", "_"}
    };
    string res = s;

    for (auto& it: chmap) {
        size_t pos=0;
        while ((pos=res.find(it.first, pos))!=string::npos) {
            res.replace(pos, it.first.size(), it.second);
        }
    }

    return res;
}

string single_to_double_quotes(const string& s)
{
    string res = s;
    size_t pos = 0;
    while ((pos=res.find("'", pos))!=string::npos) {
        res.replace(pos, 1, "\"");
    }
    return res;
}

string get_identifier_part(const string& name, const string& des)
{
    if (name=="title") {
        return "t"+des;
    } else if (name=="chapter") {
        return "ch"+des;
    } else if (name=="section") {
        return "s"+des;
    }

    return des;
}

bool isNumOrNumeral(const string& s)
{
    if (isRepeatedChar(s)) {
        return true;
    } else if (isdigit(s[0])) {
        return true;
    } else if (roman2int(s)>0) {
        return true;
    }
    return false;
}

size_t find_gen_amendment(const string& s)
{
    size_t amended_to_read = s.find("amended to read");
    if (amended_to_read!=string::npos) {
        return amended_to_read;
    }
    size_t to_read = s.find("to read");
    if (to_read!=string::npos) {
        size_t amending = s.find("amending");
        if (amending!=string::npos && amending<to_read) {
            return amending;
        }
    }
    return string::npos;
}

size_t find_is_amended(const string& s, bool return_start)
{

    static vector<string> amended_expressions = {
        "is amended",
        "is further amended",
        "are amended",
        "are each amended",
        "is repealed",
        "is hereby repealed",
        "are repealed"
    };
    
    size_t pos = string::npos;
    size_t amended_pos = string::npos;
    size_t amended_pos_end = 0;
    size_t xml_start = s.find("__xml__");
    do {
        for (const string& amended_str: amended_expressions) {
            pos = s.find(amended_str, amended_pos_end);
            if (xml_start!=string::npos && pos>xml_start) {
                pos = string::npos;
            }
            if (pos!=string::npos) {
                amended_pos = pos;
                amended_pos_end = amended_pos+amended_str.size();
                break;
            }
        }
    } while (pos!=string::npos);
    
    if (amended_pos==string::npos ||return_start) {
        return amended_pos;
    }
    return amended_pos_end;
}

string get_blanket_context_level(const string& s)
{
    static const string whenever_in_this = "whenever in this";
    size_t pos = s.find(whenever_in_this);
    if (pos==string::npos) {
        return string();
    }
    pos+= whenever_in_this.size()+1;
    size_t next_space = s.find(" ",pos);
    string res = s.substr(pos, next_space-pos);
    transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

string get_blanket_context_predicate(const string& s)
{
    static const string other_provision_of = "section or other provision of";
    static const string the_reference_shall = "the reference shall";
    size_t spos = s.find(other_provision_of);
    if (spos==string::npos) {
        return string();
    }
    spos+=other_provision_of.size()+1;
    size_t epos = s.find(the_reference_shall,spos);
    if (epos==string::npos) {
        return string();
    }
    return s.substr(spos, epos-spos-1);
}

size_t find_blanket_context(const string& s)
{
    static const string the_reference_shall = "the reference shall be considered to be made to";
    static const string provision_of = "provision of";
    size_t pos = s.find(the_reference_shall);
    if (pos==string::npos) {
        return pos;
    }
    size_t pos2 = s.find(provision_of, pos);
    if (pos2 != string::npos) {
        return pos2+provision_of.size();
    }
    return pos+the_reference_shall.size();
}

size_t find_action(const string& s)
{
    static const set<string> action_words = {
        "amending",
        "inserting",
        "insert",
        "striking",
        "strike",
        "adding",
        "redesignating",
        "redesignate",
        "designating",
        "moving",
        "move",
        "transferring",
        "deleting",
        "delete"};


    size_t by = 0;
    while ((by=s.find("by",by))!=string::npos) {
        size_t next_space = s.find(" ",by+3); // TODO: look for next word instead of space
        if (next_space==string::npos) {
            break;
        }
        string next_word = s.substr(by+3,next_space-by-3);
        if (action_words.find(next_word)!=action_words.end()) {
            return by;
        }
        by++;
    }
    return string::npos;
}

string normalize_white_spaces(const string& s, char sep)
{
    string res;
    bool whitespace = false;
    for (char c: s) {
        if (c==' ' || c=='\n' || c=='\t') {
            whitespace = true;
        } else {
            if (whitespace) {
                res+=sep;
                whitespace = false;
            }
            res+=c;
        }

    }
    if (whitespace) {
        res+=sep;
    }
    return res;
}

string clean_designation(const string& s)
{
    string res;
    string to_add;
    bool seen_space = false;
    for (char c: s) {
        if (isalnum(c) || c=='-') {
            if (seen_space) {
                res.clear();
                to_add.clear();
                seen_space = false;
            }
            res+=to_add;
            res+=c;
            to_add.clear();
        } else if (c=='.') {
            to_add = c;
        } else if (c==' ' || c=='\n') {
            seen_space=true;
        }
    }
    return res;
}

string strip_nonalnum(const string& s)
{
    string res;
    bool last_alnum = true;
    for (char c: s) {
        if (isalnum(c)) {
            if (!last_alnum && !res.empty()) {
                res+=' ';
            }
            res+=c;
            last_alnum = true;
        } else {
            last_alnum = false;
        }
    }
    return res;
}

string to_lowercase(const string& s)
{
    string res = s;
	int i;
	char c;
	for (i=0;i<s.length();i++) {
		c = s[i];
		if (c>='A' && c<='Z')
			res[i]=c+'a'-'A';
	}
	return res;
}

string to_uppercase(const string& s)
{
    string res = s;
	int i;
	char c;
	for (i=0;i<s.length();i++) {
		c = s[i];
		if (c>='a' && c<='z')
			res[i]=c-'a'+'A';
	}
	return res;
}


string getExtension(const string& filename)
{
        string::size_type pos = filename.rfind('.');
        if (pos!=string::npos) {
            return to_lowercase(filename.substr(pos+1));
        }
        return string();
}

size_t find_nth_occurance(const string& s, char c, int n)
{
    size_t pos = -1;
    for (int i=0; i<n; i++) {
        pos = s.find(c, pos+1);
        if (pos==string::npos) {
            return pos;
        }
    }
    return pos;
}

bool is_legal_id(const string& s)
{
    for (char c: s) {
        if (!isalnum(c)&&c!='_') {
            return false;
        }
    }
    return true;
}

std::string num2string(long long k, size_t n)
{
    string res(n, '\0');
    int i = n-1;
    while (k!=0 && i>=0) {
        res[i--] = k;
        k=k>>8;
    }
    return res;
}

static const std::string base64_chars = 
             "-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "_abcdefghijklmnopqrstuvwxyz";

std::string base64_encode(long long k, size_t n)
{
    if (!n&&!k) {
        return "-";
    }
    int digits = n?n:11;
    string res(digits, '-');
    int i = digits-1;
    while (k!=0 && i>=0) {
        res[i--] = base64_chars[k&63];
        k=k>>6;
    }
    if (!n) {
        return res.substr(i+1);
    }
    return res;
}

long long base64_decode(const std::string& s)
{
    long long res = 0;
    for (char c: s) {
        int digit = base64_chars.find(c);
        if (digit==string::npos) {
            return 0;
        }
        res = res*64 + digit;
    }
    return res;
}

time_t stringToTime(const std::string& s, const std::string& f)
{
    struct tm dt = {0};
    char* success;
    if (!f.empty()) {
        success = strptime(s.c_str(), f.c_str(), &dt);
    } else if (s.find('/')!=string::npos) {
        success = strptime(s.c_str(), "%m/%d/%Y:%T", &dt);
    } else {
        success = strptime(s.c_str(), "%FT%T%z", &dt);
    }
    long gmtoff = dt.tm_gmtoff;
    time_t timestamp = mktime(&dt) - gmtoff;
    return timestamp;
}

} // namespace xcite
