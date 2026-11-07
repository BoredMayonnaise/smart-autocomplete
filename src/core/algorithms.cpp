#include "smartautocomplete/core.hpp"
#include <algorithm>
namespace smart {
namespace {
void event(Trace* trace,const Metrics& metrics,std::string kind,std::size_t i=0,std::size_t j=0,int value=0,std::optional<bool> condition={}) {
    if(!trace) return;
    Event e; e.kind=kind; e.flow=kind; e.i=i; e.j=j; e.value=value; e.condition=condition; e.counters=metrics;
    trace->add(std::move(e));
}
}
SearchResult kmp(const Text& text,const Text& pattern,Trace* trace) {
    SearchResult result;
    const auto& a=text.points; const auto& b=pattern.points;
    result.lps.resize(b.size(),0);
    event(trace,result.metrics,"initialize");
    if(b.empty()) {event(trace,result.metrics,"complete"); return result;}
    event(trace,result.metrics,"lps_write",0,0,0);
    for(std::size_t i=1,j=0;i<b.size();) {
        ++result.metrics.lps;
        bool equal=b[i]==b[j];
        event(trace,result.metrics,"lps_compare",i,j,0,equal);
        if(equal) {result.lps[i]=++j; event(trace,result.metrics,"lps_write",i,j,static_cast<int>(j)); ++i;}
        else if(j) {auto previous=j; j=result.lps[j-1]; ++result.metrics.lps_fallbacks; event(trace,result.metrics,"lps_fallback",i,previous,static_cast<int>(j));}
        else {event(trace,result.metrics,"lps_write",i,0,0); ++i;}
    }
    event(trace,result.metrics,"search_initialize");
    for(std::size_t i=0,j=0;i<a.size();) {
        ++result.metrics.search;
        bool equal=a[i]==b[j];
        event(trace,result.metrics,"kmp_compare",i,j,0,equal);
        if(equal) {
            ++i; ++j;
            event(trace,result.metrics,"kmp_advance",i,j);
            if(j==b.size()) {
                auto begin=i-j;
                result.matches.push_back({begin,i,text.utf16[begin],text.utf16[i]});
                ++result.metrics.matches;
                event(trace,result.metrics,"match_found",begin,i);
                auto previous=j; j=result.lps[j-1];
                ++result.metrics.search_fallbacks;
                event(trace,result.metrics,"kmp_fallback",i,previous,static_cast<int>(j));
            }
        } else if(j) {
            auto previous=j; j=result.lps[j-1];
            ++result.metrics.search_fallbacks;
            event(trace,result.metrics,"kmp_fallback",i,previous,static_cast<int>(j));
        } else {++i; event(trace,result.metrics,"kmp_advance",i,0);}
    }
    event(trace,result.metrics,"complete",a.size(),0);
    return result;
}
SearchResult naive(const Text& text,const Text& pattern) {
    SearchResult result;
    const auto& a=text.points; const auto& b=pattern.points;
    if(b.empty() || b.size()>a.size()) return result;
    for(std::size_t i=0;i<=a.size()-b.size();++i) {
        std::size_t j=0;
        for(;j<b.size();++j) {++result.metrics.naive; if(a[i+j]!=b[j]) break;}
        if(j==b.size()) {result.matches.push_back({i,i+j,text.utf16[i],text.utf16[i+j]});++result.metrics.matches;}
    }
    return result;
}
DistanceResult levenshtein(std::u32string_view source,std::u32string_view target,Trace* trace) {
    DistanceResult result;
    const auto m=source.size(), n=target.size();
    result.matrix.assign(m+1,std::vector<int>(n+1,0));
    event(trace,result.metrics,"initialize");
    for(std::size_t i=0;i<=m;++i) {result.matrix[i][0]=static_cast<int>(i); event(trace,result.metrics,"dp_initialize",i,0,static_cast<int>(i));}
    for(std::size_t j=1;j<=n;++j) {result.matrix[0][j]=static_cast<int>(j); event(trace,result.metrics,"dp_initialize",0,j,static_cast<int>(j));}
    for(std::size_t i=1;i<=m;++i) for(std::size_t j=1;j<=n;++j) {
        int diagonal=result.matrix[i-1][j-1]+(source[i-1]!=target[j-1]);
        int up=result.matrix[i-1][j]+1, left=result.matrix[i][j-1]+1;
        result.matrix[i][j]=std::min({diagonal,up,left});
        ++result.metrics.cells;
        if(trace) {
            Event e; e.kind="dp_cell"; e.flow=e.kind; e.i=i; e.j=j; e.value=result.matrix[i][j]; e.diagonal=diagonal; e.up=up; e.left=left;
            e.condition=source[i-1]==target[j-1]; e.text=e.value==diagonal?"diagonal":e.value==up?"up":"left"; e.counters=result.metrics;
            trace->add(std::move(e));
        }
    }
    result.distance=result.matrix[m][n];
    std::size_t i=m,j=n;
    result.path.emplace_back(i,j);
    std::vector<char> steps;
    while(i || j) {
        if(i && j && result.matrix[i][j]==result.matrix[i-1][j-1]+(source[i-1]!=target[j-1])) {steps.push_back(source[i-1]==target[j-1]?'=':'s'); --i; --j;}
        else if(i && result.matrix[i][j]==result.matrix[i-1][j]+1) {steps.push_back('d'); --i;}
        else {steps.push_back('i'); --j;}
        result.path.emplace_back(i,j);
    }
    for(const auto& [row,column]:result.path) event(trace,result.metrics,"dp_backtrack",row,column,result.matrix[row][column]);
    std::reverse(steps.begin(),steps.end());
    std::u32string current(source);
    std::size_t position=0, source_index=0,target_index=0;
    for(auto step:steps) {
        std::string from=step=='i'?"":encode(source.substr(source_index,1));
        std::string to=step=='d'?"":encode(target.substr(target_index,1));
        result.aligned_source.push_back(from); result.aligned_target.push_back(to);
        if(step!='=') {
            Edit edit{step=='s'?"substitute":step=='d'?"delete":"insert",position,from,to};
            result.edits.push_back(edit);
            if(step=='d') current.erase(position,1);
            else if(step=='i') current.insert(position,target.substr(target_index,1));
            else current.replace(position,1,target.substr(target_index,1));
            ++result.metrics.edits;
            if(trace) {Event e; e.kind="edit_apply"; e.flow=e.kind; e.i=position; e.text=encode(current); e.value=static_cast<int>(result.metrics.edits); e.counters=result.metrics; trace->add(std::move(e));}
        }
        if(step!='i') ++source_index;
        if(step!='d') {++target_index; ++position;}
    }
    event(trace,result.metrics,"complete",m,n,result.distance);
    return result;
}
}
