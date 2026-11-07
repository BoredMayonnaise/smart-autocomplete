#include "smartautocomplete/core.hpp"
#include <algorithm>
namespace smart {
Text decode(std::string_view input) {
    Text result;
    std::size_t offset = 0, units = 0;
    while (offset < input.size()) {
        result.bytes.push_back(offset);
        result.utf16.push_back(units);
        auto first = static_cast<unsigned char>(input[offset++]);
        char32_t point;
        unsigned extra;
        if (first < 0x80) { point=first; extra=0; }
        else if (first >= 0xc2 && first <= 0xdf) { point=first & 0x1f; extra=1; }
        else if (first >= 0xe0 && first <= 0xef) { point=first & 0x0f; extra=2; }
        else if (first >= 0xf0 && first <= 0xf4) { point=first & 7; extra=3; }
        else throw Error(400,"utf8","Malformed UTF-8");
        if (offset+extra > input.size()) throw Error(400,"utf8","Incomplete UTF-8 sequence");
        for (unsigned i=0; i<extra; ++i) {
            auto next=static_cast<unsigned char>(input[offset++]);
            if ((next & 0xc0)!=0x80) throw Error(400,"utf8","Invalid UTF-8 continuation");
            point=(point<<6) | (next & 0x3f);
        }
        if ((extra==1 && point<0x80) || (extra==2 && point<0x800) || (extra==3 && point<0x10000) ||
            (point>=0xd800 && point<=0xdfff) || point>0x10ffff) throw Error(400,"utf8","Invalid Unicode scalar");
        result.points+=point;
        units+=point>0xffff ? 2 : 1;
    }
    result.bytes.push_back(input.size());
    result.utf16.push_back(units);
    return result;
}
std::size_t Text::index(std::size_t offset) const {
    auto it=std::lower_bound(utf16.begin(),utf16.end(),offset);
    if (it==utf16.end() || *it!=offset) throw Error(400,"selection","Selection must be on a Unicode boundary");
    return static_cast<std::size_t>(it-utf16.begin());
}
std::string encode(std::u32string_view input) {
    std::string output;
    for (auto c:input) {
        if (c>0x10ffff || (c>=0xd800 && c<=0xdfff)) throw Error(400,"unicode","Invalid Unicode scalar");
        if (c<0x80) output+=static_cast<char>(c);
        else if (c<0x800) { output+=static_cast<char>(0xc0|(c>>6)); output+=static_cast<char>(0x80|(c&63)); }
        else if (c<0x10000) { output+=static_cast<char>(0xe0|(c>>12)); output+=static_cast<char>(0x80|((c>>6)&63)); output+=static_cast<char>(0x80|(c&63)); }
        else { output+=static_cast<char>(0xf0|(c>>18)); output+=static_cast<char>(0x80|((c>>12)&63)); output+=static_cast<char>(0x80|((c>>6)&63)); output+=static_cast<char>(0x80|(c&63)); }
    }
    return output;
}
std::u32string normalize(std::u32string_view input) {
    std::u32string result(input);
    for (auto& c:result) if(c>=U'A' && c<=U'Z') c+=U'a'-U'A';
    return result;
}
bool word_letter(char32_t c) { return (c>=U'a' && c<=U'z') || (c>=U'A' && c<=U'Z'); }
Token active_token(const Text& text, std::size_t start, std::size_t end) {
    if(start>end) throw Error(400,"selection","Selection start exceeds end");
    auto first=text.index(start), last=text.index(end);
    auto inside=[&](std::size_t i) {
        if(i>=text.points.size()) return false;
        auto c=text.points[i];
        return word_letter(c) || ((c==U'\'' || c==U'-') && i>0 && i+1<text.points.size() && word_letter(text.points[i-1]) && word_letter(text.points[i+1]));
    };
    std::size_t begin=first, finish=first;
    while(begin>0 && inside(begin-1)) --begin;
    while(finish<text.points.size() && inside(finish)) ++finish;
    if(last>finish) return {last,last,{}};
    return {begin,finish,text.points.substr(begin,finish-begin)};
}
Replacement replace_token(const Text& text, const Token& token, std::u32string_view word) {
    std::u32string replacement(word);
    bool upper=false, all_upper=true;
    for(auto c:token.value) if(word_letter(c)) { upper=true; if(c>=U'a' && c<=U'z') all_upper=false; }
    if(upper && all_upper) {
        for(auto& c:replacement) if(c>=U'a' && c<=U'z') c-=U'a'-U'A';
    } else if(!token.value.empty() && token.value.front()>=U'A' && token.value.front()<=U'Z' && !replacement.empty() && replacement.front()>=U'a' && replacement.front()<=U'z') replacement.front()-=U'a'-U'A';
    auto output=text.points.substr(0,token.begin)+replacement+text.points.substr(token.end);
    auto units=decode(encode(replacement)).utf16.back();
    return {encode(output),text.utf16.at(token.begin)+units};
}
void Trace::add(Event event) {
    if(events.size()>=limit) throw Error(413,"trace_limit","Trace exceeds event limit; shorten the input");
    events.push_back(std::move(event));
}
}
