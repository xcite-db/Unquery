// Copyright (c) 2022 by Sela Mador-Haim

#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED
#include <string>
#include <vector>

namespace xcite {

int roman2int(const std::string& roman);
std::string int2roman(int n, bool uppercase);
int letter2int(const std::string& letter);
std::string int2letter(int n, bool uppercase);

enum class LetterCase {
    Upper,
    Lower,
    Mixed,
    None
};

LetterCase getCase(const std::string& s);

int isOpenQuotes(const std::string& s, size_t pos);
int isCloseQuotes(const std::string& s, size_t pos);
std::string stripQuotes(std::string s);


std::string normalize_string(const std::string& s);

std::string get_identifier_part(const std::string& name, const std::string& des);

// Note: returns true in case its a number, a number and letters combinations (e.g. 10A), a roman numeral, or a letter numeral
bool isNumOrNumeral(const std::string& s);

bool is_number(const std::string& s);

size_t find_gen_amendment(const std::string& s);

size_t find_is_amended(const std::string& s, bool return_start);

size_t find_action(const std::string& s);

size_t find_blanket_context(const std::string& s);

std::string get_blanket_context_level(const std::string& s);

std::string get_blanket_context_predicate(const std::string& s);

std::string single_to_double_quotes(const std::string& s);

std::string normalize_white_spaces(const std::string& s, char sep = ' ');

std::string clean_designation(const std::string& s);

std::string strip_nonalnum(const std::string& s);

std::string to_lowercase(const std::string& s);

std::string to_uppercase(const std::string& s);

std::string getExtension(const std::string& filename);

size_t find_nth_occurance(const std::string& s, char c, int n);

bool is_legal_id(const std::string& s);

std::string num2string(long long i, size_t n);

std::string base64_encode(long long i, size_t n);

long long base64_decode(const std::string& s);

time_t stringToTime(const std::string& s, const std::string& f = {});

std::string timeToString(time_t t, const std::string& f);

std::vector<std::string> split_string(const std::string& s, const std::string& delim);

} // namespace xcite

#endif // UTILS_H_INCLUDED
