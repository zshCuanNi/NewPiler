#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <sstream>
#include <cassert>
#include <memory>

using string = std::string;

bool start_with(string str, string head);
bool is_digit(char c);
char get_useful_peek(std::stringstream& sstr);
int get_unsigned_int(std::stringstream& sstr);
bool is_param(string var_id);
string add_suffix(const string& text);

/* implemented by LianYueBiao, but only supported by Microsoft
 * https://blog.csdn.net/alvinlyb/article/details/90039254
 */
// static std::string format(const char* pszFmt, ...) {
// 	std::string str;
// 	va_list args;
// 	va_start(args, pszFmt);
// 	{
// 		int nLength = _vscprintf(pszFmt, args); // MS version
// 		nLength += 1;
// 		std::vector<char> vectorChars(nLength);
// 		_vsnprintf(vectorChars.data(), nLength, pszFmt, args); // MS version
// 		str.assign(vectorChars.data());
// 	}
// 	va_end(args);
// 	return str;
// }

template<typename T>
T convert(const T& value) { return value; }

const char* convert(const string& value);

/* provided by iFreilicht
 * https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
 * revised by Valentin
 * https://stackoverflow.com/questions/48958061/function-taking-a-variadic-template-pack-to-convert-stdstrings-to-const-char
 */
template<typename ... Args>
string format( const string& format, Args ... args ) {
    int size_s = snprintf( nullptr, 0, format.c_str(), convert(args) ... ) + 1; // Extra space for '\0'
    auto size = static_cast<size_t>( size_s );
    auto buf = std::make_unique<char[]>( size );
    snprintf( buf.get(), size, format.c_str(), convert(args) ... );
    return string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

#endif
