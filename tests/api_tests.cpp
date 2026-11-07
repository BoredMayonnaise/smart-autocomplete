#include "api.hpp"
#include <future>
#include <iostream>
#include <cstdlib>
using namespace smart;
void check(bool ok,const char* message) {if(!ok) {std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
int main() {
    Api api({"algorithm","algorithmic","algorithms","receive","programming"});
    auto reply=api.handle("GET","/health");
    check(reply.status==200,"Health route must be available");
    auto call=[&](std::string_view path,std::string_view body,int status=200) {
        auto response=api.handle("POST",path,body);
        if(response.status!=status) std::cerr<<response.body<<'\n';
        check(response.status==status,"Expected API status");
        return crow::json::load(response.body);
    };
    call("/api/analyze","{",400);
    call("/api/analyze",R"({"text":5})",400);
    auto analysis=call("/api/analyze",R"({"text":"algor","start":5,"end":5,"threshold":2,"revision":7,"request_id":"a"})");
    check(analysis["status"].s()=="prefix" && analysis["completions"].size()==3 && analysis["corrections"].size()==0,"Incomplete prefix not marked misspelled");
    check(analysis["revision"].u()==7 && analysis["request_id"].s()=="a","Echo revision and request identity");
    check(analysis["metrics"]["lookups"].u()==5 && analysis["metrics"]["visited"].u()==0 && analysis["metrics"]["scan_words"].u()==5,"Prefix metrics count real probes and full-scan words, no fuzzy work");
    auto exact=call("/api/analyze",R"({"text":"algorithm","start":9,"end":9,"threshold":2,"revision":7,"request_id":"exact"})");
    check(exact["status"].s()=="exact" && exact["metrics"]["visited"].u()==0,"Exact words skip fuzzy correction");
    auto long_word=call("/api/analyze",R"({"text":"abcdefghijklmnopqrstuvwxyzabcdefg","start":33,"end":33,"threshold":2,"revision":7,"request_id":"long"})");
    check(long_word["status"].s()=="too_long" && long_word["metrics"]["lookups"].u()==0 && long_word["workload"]["word_limit"].u()==32,"Oversized active word reports its real limit, not empty input");
    call("/api/analyze",R"({"text":"😀a","start":1,"end":1,"threshold":2,"revision":1,"request_id":"b"})",400);
    call("/api/analyze",R"({"text":"hello","start":5,"end":5,"threshold":4,"revision":1,"request_id":"b"})",400);
    auto correction=call("/api/analyze",R"({"text":"recieve","start":7,"end":7,"threshold":2,"revision":1,"request_id":"b"})");
    check(correction["corrections"][0]["word"].s()=="receive" && correction["corrections"][0]["distance"].i()==2,"Server fuzzy results");
    check(correction["metrics"]["candidates"].u()==correction["corrections"].size() && correction["metrics"]["pruned"].u()>0,"Correction analytics expose real candidates and pruning");
    auto search=call("/api/search",R"({"text":"banana bandana banana","pattern":"ana","revision":1,"request_id":"c"})");
    check(search["matches"].size()==5 && search["matches"][2]["start"].u()==11,"Search route all overlaps");
    auto cheap=call("/api/trace",R"({"mode":"kmp","text":"abc","pattern":"abc","revision":1,"request_id":"short"})");
    check(cheap["result"]["comparison"]["difference"].i()==2 && cheap["metrics"]["matches"].u()==1,"Comparison summary reports when KMP does more work");
    auto stress=call("/api/trace",R"({"mode":"kmp","text":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab","pattern":"aaaaab","revision":1,"request_id":"stress"})");
    check(stress["result"]["comparison"]["kmp_total"].u()==77 && stress["result"]["comparison"]["naive_total"].u()==192 && stress["result"]["comparison"]["difference"].i()==-115,"Comparison summary uses measured full-run totals");
    auto trace=call("/api/trace",R"({"mode":"dp","source":"programing","target":"programming","revision":1,"request_id":"d"})");
    check(trace["schema_version"].u()==1 && trace["events"].size()>0 && trace["result"]["distance"].i()==1,"Trace envelope and real result");
    check(trace["initial"]["matrix_rows"].u()==11 && trace["flow"]["nodes"].size()>0,"Immutable structure and flow metadata");
    auto trie_trace=call("/api/trace",R"({"mode":"trie","prefix":"algor","revision":1,"request_id":"focus"})");
    check(trie_trace["initial"]["nodes"][0].has("focus") && trie_trace["initial"]["nodes"][0]["focus"].b(),"Trie root and traced/cached paths have focus metadata");
    check(trie_trace["workload"]["dictionary_words"].u()==5 && trie_trace["metrics"]["scan_words"].u()==5 && trie_trace["metric_definitions"].has("pruned"),"Workload and metric definitions come from server snapshot");
    call("/api/trace",R"({"mode":"unknown","revision":1,"request_id":"e"})",400);
    call("/api/search",R"({"text":"a","pattern":"a","revision":-1,"request_id":"e"})",400);
    auto before=crow::json::load(api.handle("GET","/api/dictionary").body);
    auto version=before["dictionary_version"].u();
    auto invalid=api.handle("PUT","/api/dictionary",R"({"words":["valid","bad!"],"dictionary_version":1,"revision":1,"request_id":"f"})");
    check(invalid.status==403,"Dictionary replacement is locked");
    auto after=crow::json::load(api.handle("GET","/api/dictionary").body);
    check(after["dictionary_version"].u()==version && after["words"].size()==before["words"].size(),"Locked dictionary preserves old dictionary");
    auto accepted=call("/api/accept",R"({"text":"Hello, ALGOR!","start":12,"end":12,"word":"algorithm","dictionary_version":1,"revision":8,"request_id":"g"})");
    check(accepted["text"].s()=="Hello, ALGORITHM!" && accepted["caret"].u()==16,"Accept uses C++ token replacement");
    call("/api/accept",R"({"text":"algor","start":5,"end":5,"word":"algorithm","dictionary_version":1,"revision":8,"request_id":"g"})",409);
    auto readers=std::async(std::launch::async,[&] {for(int i=0;i<30;++i) check(api.handle("GET","/api/dictionary").status==200,"Concurrent snapshot read");});
    auto replaced=api.handle("PUT","/api/dictionary",R"({"words":["cat","cat","cart"],"dictionary_version":2,"revision":9,"request_id":"h"})");
    check(replaced.status==403,"Dictionary replacement is locked"); readers.get();
    auto final=crow::json::load(api.handle("GET","/api/dictionary").body);
    check(final["words"].size()==before["words"].size() && final["dictionary_version"].u()==2,"Locked dictionary preserves words and version");
    Limits limits;limits.events=2;limits.body=1000;
    Api tiny({"cat"},limits);
    check(tiny.handle("POST","/api/trace",R"({"mode":"kmp","text":"banana","pattern":"ana","revision":1,"request_id":"x"})").status==413,"Oversized trace rejects completely");
    check(tiny.handle("POST","/api/search",R"({"text":"banana","pattern":"ana","revision":1,"request_id":"x"})").status==200,"Ordinary result works without trace");
    check(tiny.handle("POST","/api/search",std::string(1001,' ')).status==413,"Body limit");
    Limits short_limits;short_limits.text=3;
    Api short_editor({"cat","cart"},short_limits);
    auto oversized_accept=short_editor.handle("POST","/api/accept",R"({"text":" ca","start":3,"end":3,"word":"cart","dictionary_version":1,"revision":1,"request_id":"limit"})");
    check(oversized_accept.status==413,"Acceptance cannot exceed editor limit");
    auto unchanged=crow::json::load(short_editor.handle("GET","/api/dictionary").body);
    check(unchanged["dictionary_version"].u()==1,"Oversized acceptance must not update usage/version");
    Api capacity({"cat"});
    crow::json::wvalue replacement;
    replacement["dictionary_version"]=1;replacement["revision"]=1;replacement["request_id"]="capacity";
    std::vector<crow::json::wvalue> words;
    for(int i=0;i<10000;++i) {
        std::string word="word";
        word+=static_cast<char>('a'+i/676);word+=static_cast<char>('a'+(i/26)%26);word+=static_cast<char>('a'+i%26);
        words.emplace_back(word);
    }
    replacement["words"]=crow::json::wvalue::list(std::move(words));
    auto expanded=capacity.handle("PUT","/api/dictionary",replacement.dump());
    check(expanded.status==403,"Dictionary replacement is locked");
    auto retained=crow::json::load(capacity.handle("GET","/api/dictionary").body);
    check(retained["count"].u()==1 && retained["dictionary_version"].u()==1,"Locked dictionary preserves original words and version");
    std::cout<<"API checks passed\n";
}
