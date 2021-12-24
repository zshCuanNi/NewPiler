#include "utils.hpp"

bool start_with(string str, string head) {
  return str.compare(0, head.size(), head) == 0;
}

bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

char get_useful_peek(std::stringstream& sstr) {
  char ch;
  for (ch = sstr.peek(); ch == ' ' || ch == '\t'; ch = sstr.peek())
    sstr.get();
  return ch;
}

int get_unsigned_int(std::stringstream& sstr) {
  while (!is_digit(sstr.peek())) sstr.get();
  int ret_int;
  sstr >> ret_int;
  return ret_int;
}

bool is_param(string var_id) { return start_with(var_id, "p"); }

string add_suffix(const string& text) {
    int suffix;
    if (text.length() <= 10)
        suffix = 10 - text.length();
    else suffix = 1;
    return format("%s%s", text, string(suffix, ' '));
}

const char* convert(const string& value) { return value.c_str(); }
