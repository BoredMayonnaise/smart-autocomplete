#include "smartautocomplete/core.hpp"
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <numeric>
#include <random>
#include <fstream>
using namespace smart;
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
    std::ifstream bundled(std::string(SMART_SOURCE_DIR)+"/data/words.txt");
    std::vector<std::string> bundled_words;
    for(std::string word;std::getline(bundled,word);) if(!word.empty()) bundled_words.push_back(word);
    Dictionary vocabulary(bundled_words);
    check(bundled_words.size()==50000 && vocabulary.words.size()==50000,"Bundled vocabulary contains 10,000 valid unique words without duplicate lines");
    auto over_limit=bundled_words;over_limit.push_back("zzzz-limit-word");
    bool limit_rejected=false;
    try {Dictionary too_many(over_limit);} catch(const Error& e) {limit_rejected=e.code=="dictionary_limit";}
    check(limit_rejected,"Default dictionary capacity rejects word 50,001");
    check(vocabulary.find(U"garden").has_value() && vocabulary.find(U"computer").has_value() && vocabulary.find(U"music").has_value(),"Expanded everyday vocabulary available");
    check(!vocabulary.find(U"programing") && !vocabulary.find(U"recieve"),"Presentation misspellings remain correction examples");
    std::ifstream teaching(std::string(SMART_SOURCE_DIR)+"/data/teaching-words.txt");
    for(std::string word;std::getline(teaching,word);) check(vocabulary.find(decode(word).points).has_value(),"Original teaching vocabulary preserved");
    auto result = kmp(decode("banana"), decode("ana"));
    check(result.matches.size() == 2, "KMP must retain overlapping matches at 1 and 3");
    check(result.matches[0].start == 1 && result.matches[1].start == 3, "KMP overlap positions");
    auto stress = kmp(decode(std::string(36, 'a') + "b"), decode("aaaaab"));
    check(stress.matches.size() == 1 && stress.matches[0].start == 31, "KMP stress match");
    check(stress.metrics.lps == 9 && stress.metrics.search == 68, "KMP comparison fixture 9 + 68");
    check(naive(decode(std::string(36, 'a') + "b"), decode("aaaaab")).metrics.naive == 192, "Naive comparison fixture");
    check(kmp(decode(""), decode("a")).matches.empty(), "Empty text");
    check(kmp(decode("abc"), decode("")).matches.empty(), "Empty pattern convention");
    check(kmp(decode("ab"), decode("abc")).matches.empty(), "Longer pattern");
    auto short_search=kmp(decode("abc"),decode("abc"));
    check(short_search.metrics.lps+short_search.metrics.search>naive(decode("abc"),decode("abc")).metrics.naive,"KMP preprocessing can cost more than naive");
    auto unicode = decode("😀aé😀a");
    check(unicode.points.size() == 5 && unicode.utf16.back() == 7 && unicode.bytes.back() == 12, "Unicode offset units");
    check(encode(unicode.points) == "😀aé😀a", "UTF-8 round trip");
    auto unicode_matches = kmp(unicode, decode("😀a"));
    check(unicode_matches.matches == std::vector<Match>{{0,2,0,3},{3,5,4,7}}, "UTF-16 match ranges");
    bool rejected = false;
    try { unicode.index(1); } catch (const Error&) { rejected = true; }
    check(rejected, "Surrogate interior selection rejected");
    for (auto invalid : {std::string("\xc0\x80",2), std::string("\xed\xa0\x80",3), std::string("\xf4\x90\x80\x80",4), std::string("\xe2\x82",2)}) {
        rejected = false;
        try { decode(invalid); } catch (const Error&) { rejected = true; }
        check(rejected, "Malformed UTF-8 rejected");
    }
    auto editor = decode("😀 Hello, ALGOr! End.");
    auto token = active_token(editor, 15, 15);
    check(token.value == U"ALGOr", "Active English token");
    auto replacement = replace_token(editor, token, U"algorithm");
    check(replacement.text == "😀 Hello, Algorithm! End." && replacement.caret == 19, "Preserve punctuation and initial capitalization");
    auto upper = decode("Say ALGOR!");
    check(replace_token(upper, active_token(upper,9,9), U"algorithm").text == "Say ALGORITHM!", "Preserve all capitals");
    Dictionary dict({"algorithms","algorithmic","algorithm","algorithm","can't","well-known"});
    check(dict.words.size() == 5, "Dictionary duplicates collapse");
    auto prefix = dict.prefix(U"ALGOR");
    check(prefix.words.size() == 3 && prefix.words[0].word == "algorithm" && prefix.words[1].word == "algorithmic", "Deterministic prefix rank");
    check(prefix.metrics.lookups == 5 && prefix.metrics.copied == 3, "Prefix counters separated");
    check(dict.prefix(U"x").words.empty() && dict.prefix(U"xyz").metrics.lookups == 1, "Stop at first missing edge");
    check(dict.prefix(U"").words.size() == 5 && dict.find(U"ALGORITHM").has_value(), "Empty prefix and exact membership");
    check(dict.prefix(U"ALGORITHM").exact && !dict.prefix(U"algor").exact && !dict.prefix(U"").exact,"Membership shares measured prefix traversal");
    check(dict.scan(U"xyz").metrics.scan_words==dict.words.size(),"Prefix baseline scans the whole dictionary");
    std::vector<std::string> many;
    for(char c='a'; c<='l'; ++c) many.push_back(std::string("word") + c);
    Dictionary ranked(many);
    check(ranked.prefix(U"word").words.size() == 8, "Top-eight cap");
    ranked.accept(U"wordl");
    check(ranked.prefix(U"word").words[0].word == "wordl", "Promote word outside cache");
    for (const auto& node : ranked.nodes) check(node.top.size() <= 8, "Every node cache bounded");
    auto dp = levenshtein(U"programing", U"programming");
    check(dp.distance == 1 && dp.edits.size() == 1 && dp.edits[0].kind == "insert", "Single insertion reconstruction");
    check(levenshtein(U"recieve",U"receive").distance == 2, "Standard Levenshtein not transposition");
    check(levenshtein(U"kitten",U"sitting").distance == 3, "Kitten fixture");
    check(levenshtein(U"abc",U"").distance == 3 && levenshtein(U"",U"abc").distance == 3, "Empty DP borders");
    check(levenshtein(U"same",U"same").distance == 0, "Equal strings");
    std::mt19937 random(123456);
    auto reference_distance = [](const std::u32string& a, const std::u32string& b) {
        std::vector<int> row(b.size()+1);
        std::iota(row.begin(),row.end(),0);
        for(std::size_t i=0;i<a.size();++i) {
            int previous=row[0]; row[0]=static_cast<int>(i+1);
            for(std::size_t j=0;j<b.size();++j) {
                int old=row[j+1];
                row[j+1]=std::min({row[j]+1,old+1,previous+(a[i]!=b[j])}); previous=old;
            }
        }
        return row.back();
    };
    auto random_string = [&]() { std::u32string s; auto n=random()%18; for(unsigned i=0;i<n;++i) s+=U'a'+random()%4; return s; };
    for(int trial=0;trial<500;++trial) {
        auto a=random_string(), b=random_string();
        auto text=decode(encode(a)), pattern=decode(encode(b));
        Trace trace;
        auto traced=kmp(text,pattern,&trace), plain=kmp(text,pattern);
        check(traced.matches == naive(text,pattern).matches && traced.matches == plain.matches && traced.metrics == plain.metrics, "Randomized KMP and trace invariance");
        for(std::size_t j=0;j<traced.lps.size();++j) {
            auto n=traced.lps[j];
            check(n<=j && b.substr(0,n)==b.substr(j+1-n,n), "LPS prefix/suffix invariant");
            for(std::size_t longer=n+1;longer<=j;++longer) check(b.substr(0,longer)!=b.substr(j+1-longer,longer), "LPS maximality");
        }
        std::size_t last_i=0;
        std::size_t lps_comparisons=0,search_comparisons=0,lps_fallbacks=0,search_fallbacks=0,matches=0;
        for(const auto& event:trace.events) {
            lps_comparisons+=event.kind=="lps_compare";search_comparisons+=event.kind=="kmp_compare";
            lps_fallbacks+=event.kind=="lps_fallback";search_fallbacks+=event.kind=="kmp_fallback";matches+=event.kind=="match_found";
            check(event.counters.lps==lps_comparisons && event.counters.search==search_comparisons && event.counters.matches==matches,
                "Every replay counter reflects operations already executed");
            check(event.counters.lps_fallbacks==lps_fallbacks && event.counters.search_fallbacks==search_fallbacks,"Fallback counters equal observed events");
        }
        check(traced.metrics.matches==traced.matches.size() && trace.events.back().counters==traced.metrics,"Final KMP counters match actual results");
        for(const auto& event:trace.events) if(event.kind=="kmp_compare" || event.kind=="kmp_fallback") {
            check(event.i>=last_i, "KMP text pointer never moves backward"); last_i=event.i;
        }
        Trace matrix_trace;
        auto distance=levenshtein(a,b,&matrix_trace);
        check(distance.distance==reference_distance(a,b), "DP independent reference");
        check(distance.metrics == levenshtein(a,b).metrics, "DP trace invariance");
        std::u32string current=a;
        for(const auto& edit:distance.edits) {
            if(edit.kind=="insert") current.insert(edit.position,decode(edit.after).points);
            else if(edit.kind=="delete") current.erase(edit.position,1);
            else current.replace(edit.position,1,decode(edit.after).points);
        }
        check(current==b && static_cast<int>(distance.edits.size())==distance.distance, "Forward edits produce target optimally");
        std::vector<std::vector<int>> replay(a.size()+1,std::vector<int>(b.size()+1,-1));
        for(const auto& event:matrix_trace.events) if(event.kind=="dp_initialize" || event.kind=="dp_cell") replay.at(event.i).at(event.j)=event.value;
        check(replay==distance.matrix, "Matrix replay starts empty and reaches result");
    }
    std::vector<std::string> fuzzy_words={"cat","bat","cart","coat","dog","algorithm","receive","programming"};
    for(int i=0;i<120;++i) {auto word=random_string();if(!word.empty()) fuzzy_words.push_back(encode(word));}
    Dictionary fuzzy_dict(fuzzy_words);
    for(int i=0;i<50;++i) fuzzy_dict.accept(fuzzy_dict.words[random()%fuzzy_dict.words.size()].value);
    std::vector<std::u32string> queries={U"",U"ct",U"caat",U"recieve",U"programing",U"zzzz"};
    for(int i=0;i<60;++i) queries.push_back(random_string());
    for(int threshold=1;threshold<=3;++threshold) for(const auto& query:queries) {
        Metrics measured;
        auto candidates=fuzzy_dict.fuzzy(query,threshold,&measured);
        std::vector<Candidate> expected;
        for(const auto& word:fuzzy_dict.words) {
            int distance=reference_distance(query,word.value);
            if(distance<=threshold) expected.push_back({encode(word.value),distance,word.usage});
        }
        check(measured.candidates==expected.size(),"Fuzzy candidate count includes words outside the top eight");
        check(measured.cells==measured.visited*query.size() && measured.pruned<=measured.visited,"Fuzzy cells and pruned rows count actual work");
        std::sort(expected.begin(),expected.end(),[](const auto& a,const auto& b){
            if(a.distance!=b.distance) return a.distance<b.distance;
            return a.usage!=b.usage?a.usage>b.usage:a.word<b.word;
        });
        if(expected.size()>8) expected.resize(8);
        check(candidates==expected, "Pruned dictionary DP agrees with full scan");
        check(fuzzy_dict.prefix(query).words==fuzzy_dict.scan(query).words && fuzzy_dict.prefix(query).exact==fuzzy_dict.scan(query).exact,
            "Cached prefix ranking and membership agree with full scan after usage updates");
    }
    for(int threshold=1;threshold<=3;++threshold) for(auto query:{U"programing",U"recieve",U"zzzz",U"algor"}) {
        Metrics work;
        auto actual=vocabulary.fuzzy(query,threshold,&work);
        std::vector<Candidate> expected;
        for(const auto& word:vocabulary.words) {
            const auto distance=reference_distance(query,word.value);
            if(distance<=threshold) expected.push_back({encode(word.value),distance,word.usage});
        }
        check(work.candidates==expected.size(),"50,000-word candidate count agrees with independent scan");
        std::sort(expected.begin(),expected.end(),[](const auto& a,const auto& b){return a.distance!=b.distance?a.distance<b.distance:a.word<b.word;});
        if(expected.size()>8) expected.resize(8);
        check(actual==expected,"50,000-word pruned search returns exact top eight at every threshold");
    }
    Trace trie_trace;
    auto traced_prefix=dict.prefix(U"algor",&trie_trace);
    check(traced_prefix.words==dict.prefix(U"algor").words && traced_prefix.words==dict.scan(U"algor").words, "Prefix trace and scan agreement");
    for(const auto& event:trie_trace.events) check(event.node<dict.nodes.size() && event.other<dict.nodes.size(), "Trace node IDs valid");
    Trace repeat;
    dict.prefix(U"algor",&repeat);
    check(trie_trace.events==repeat.events, "Restart trace deterministic");
    rejected=false;
    try {Trace tiny(2); kmp(decode("banana"),decode("ana"),&tiny);} catch(const Error& e) {rejected=e.code=="trace_limit";}
    check(rejected, "Trace overflow explicit error");
    std::cout << "Core checks passed\n";
}
