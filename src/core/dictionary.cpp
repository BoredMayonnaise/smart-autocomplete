#include "smartautocomplete/core.hpp"
#include <algorithm>
#include <functional>
#include <map>
#include <numeric>
namespace smart {
void Dictionary::rank(std::vector<std::size_t>& indices) const {
    std::sort(indices.begin(),indices.end(),[&](auto a,auto b) {
        return words[a].usage!=words[b].usage ? words[a].usage>words[b].usage : words[a].value<words[b].value;
    });
    indices.erase(std::unique(indices.begin(),indices.end()),indices.end());
    if(indices.size()>limits_.suggestions) indices.resize(limits_.suggestions);
}
Dictionary::Dictionary(const std::vector<std::string>& entries, Limits limits) : limits_(limits) {
    std::map<std::u32string,bool> unique;
    for(const auto& entry:entries) {
        auto word=normalize(decode(entry).points);
        if(word.empty() || word.size()>limits.word) throw Error(400,"dictionary_word","Words must contain 1–32 code points");
        for(std::size_t i=0;i<word.size();++i) {
            auto c=word[i];
            if(!word_letter(c) && !((c==U'\'' || c==U'-') && i>0 && i+1<word.size() && word_letter(word[i-1]) && word_letter(word[i+1])))
                throw Error(400,"dictionary_word","Use English letters and internal apostrophes or hyphens");
        }
        unique.emplace(std::move(word),true);
        if(unique.size()>limits.words) throw Error(413,"dictionary_limit","Too many unique dictionary words");
    }
    nodes.emplace_back();
    for(const auto& [value,unused]:unique) {
        (void)unused;
        auto index=words.size();
        words.push_back({value,0});
        std::size_t node=0;
        nodes[0].top.push_back(index); rank(nodes[0].top);
        for(auto character:value) {
            auto it=nodes[node].children.find(character);
            if(it==nodes[node].children.end()) {
                auto child=nodes.size();
                nodes[node].children.emplace(character,child);
                Node created; created.parent=node; created.character=character;
                nodes.push_back(std::move(created));
                node=child;
            } else node=it->second;
            nodes[node].top.push_back(index); rank(nodes[node].top);
        }
        nodes[node].terminal=index;
    }
}
std::optional<std::size_t> Dictionary::find(std::u32string_view input) const {
    auto value=normalize(input);
    std::size_t node=0;
    for(auto c:value) {
        auto it=nodes[node].children.find(c);
        if(it==nodes[node].children.end()) return std::nullopt;
        node=it->second;
    }
    return nodes[node].terminal;
}
PrefixResult Dictionary::prefix(std::u32string_view input, Trace* trace) const {
    PrefixResult result;
    auto prefix=normalize(input);
    std::size_t node=0;
    auto emit=[&](std::string kind,std::size_t i,std::size_t from,std::size_t to,std::optional<bool> condition={}) {
        if(!trace) return;
        Event event; event.kind=kind; event.flow=kind; event.i=i; event.node=from; event.other=to; event.condition=condition; event.counters=result.metrics;
        trace->add(std::move(event));
    };
    emit("initialize",0,0,0);
    for(std::size_t i=0;i<prefix.size();++i) {
        auto it=nodes[node].children.find(prefix[i]);
        ++result.metrics.lookups;
        bool found=it!=nodes[node].children.end();
        emit("trie_edge_lookup",i,node,found?it->second:node,found);
        if(!found) {emit("prefix_missing",i,node,node); emit("complete",i,node,node); return result;}
        node=it->second;
    }
    result.exact=nodes[node].terminal.has_value();
    for(auto index:nodes[node].top) result.words.push_back({encode(words[index].value),0,words[index].usage});
    result.metrics.copied=result.words.size();
    emit("cache_read",prefix.size(),node,node);
    emit("complete",prefix.size(),node,node);
    return result;
}
PrefixResult Dictionary::scan(std::u32string_view input) const {
    PrefixResult result;
    auto prefix=normalize(input);
    std::vector<std::size_t> matches;
    for(std::size_t w=0;w<words.size();++w) {
        ++result.metrics.scan_words;
        bool match=true;
        for(std::size_t i=0;i<prefix.size();++i) {
            if(i>=words[w].value.size()) {match=false; break;}
            ++result.metrics.scan_checks;
            if(words[w].value[i]!=prefix[i]) {match=false; break;}
        }
        if(match) {matches.push_back(w);if(words[w].value==prefix) result.exact=true;}
    }
    rank(matches);
    for(auto w:matches) result.words.push_back({encode(words[w].value),0,words[w].usage});
    result.metrics.copied=result.words.size();
    return result;
}
void Dictionary::accept(std::u32string_view input) {
    auto index=find(input);
    if(!index) throw Error(400,"unknown_word","Accepted word is not in the dictionary");
    ++words[*index].usage;
    auto update=[&](std::size_t node) {nodes[node].top.push_back(*index); rank(nodes[node].top);};
    std::size_t node=0; update(node);
    for(auto c:words[*index].value) {node=nodes[node].children.at(c); update(node);}
    ++version;
}
std::vector<Candidate> Dictionary::fuzzy(std::u32string_view input,int threshold,Metrics* metrics) const {
    if(threshold<1 || threshold>3) throw Error(400,"threshold","Edit threshold must be 1, 2, or 3");
    auto query=normalize(input);
    std::vector<Candidate> result;
    auto better=[](const Candidate& a,const Candidate& b) {
        if(a.distance!=b.distance) return a.distance<b.distance;
        return a.usage!=b.usage ? a.usage>b.usage : a.word<b.word;
    };
    std::vector<int> initial(query.size()+1);
    std::iota(initial.begin(),initial.end(),0);
    std::function<void(std::size_t,const std::vector<int>&)> visit=[&](std::size_t parent,const std::vector<int>& previous) {
        std::vector<std::pair<char32_t,std::size_t>> children(nodes[parent].children.begin(),nodes[parent].children.end());
        std::sort(children.begin(),children.end());
        for(auto [character,node]:children) {
            if(metrics) ++metrics->visited;
            std::vector<int> row(query.size()+1); row[0]=previous[0]+1;
            for(std::size_t j=1;j<row.size();++j) {
                row[j]=std::min({previous[j]+1,row[j-1]+1,previous[j-1]+(character!=query[j-1])});
                if(metrics) ++metrics->cells;
            }
            if(nodes[node].terminal && row.back()<=threshold) {
                const auto& word=words[*nodes[node].terminal];
                if(metrics) ++metrics->candidates;
                Candidate candidate{encode(word.value),row.back(),word.usage};
                auto place=std::lower_bound(result.begin(),result.end(),candidate,better);
                if(result.size()<limits_.suggestions || place!=result.end()) {
                    result.insert(place,std::move(candidate));
                    if(result.size()>limits_.suggestions) result.pop_back();
                }
            }
            if(*std::min_element(row.begin(),row.end())<=threshold) visit(node,row);
            else if(metrics) ++metrics->pruned;
        }
    };
    visit(0,initial);
    return result;
}
}
