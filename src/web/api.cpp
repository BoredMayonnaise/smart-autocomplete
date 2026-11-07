#include "api.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
namespace smart {
namespace {
using Json=crow::json::wvalue;
using Read=crow::json::rvalue;
using Type=crow::json::type;
std::string string_field(const Read& body,const char* field) {
    if(!body.has(field) || body[field].t()!=Type::String) throw Error(400,"field",std::string(field)+" must be a string");
    return std::string(body[field].s());
}
std::uint64_t integer(const Read& body,const char* field) {
    if(!body.has(field) || body[field].t()!=Type::Number || body[field].nt()==crow::json::num_type::Floating_point)
        throw Error(400,"field",std::string(field)+" must be a nonnegative integer");
    if(body[field].nt()==crow::json::num_type::Signed_integer && body[field].i()<0) throw Error(400,"field",std::string(field)+" cannot be negative");
    auto value=body[field].u();
    if(value>9007199254740991ULL) throw Error(400,"field","Integer exceeds browser precision");
    return value;
}
Text text_field(const Read& body,const char* key,std::size_t limit) {
    auto result=decode(string_field(body,key));
    if(result.points.size()>limit) throw Error(413,"input_limit",std::string(key)+" exceeds code-point limit");
    return result;
}
Json metrics(const Metrics& m) {
    return Json{{"lookups",m.lookups},{"copied",m.copied},{"scan_checks",m.scan_checks},{"visited",m.visited},{"cells",m.cells},{"lps",m.lps},{"search",m.search},{"naive",m.naive},{"edits",m.edits},
        {"scan_words",m.scan_words},{"pruned",m.pruned},{"candidates",m.candidates},{"lps_fallbacks",m.lps_fallbacks},{"search_fallbacks",m.search_fallbacks},{"matches",m.matches}};
}
Json candidates(const std::vector<Candidate>& words) {
    std::vector<Json> values;
    for(const auto& word:words) values.push_back(Json{{"word",word.word},{"distance",word.distance},{"usage",word.usage}});
    return Json::list(std::move(values));
}
Json characters(std::u32string_view input) {
    std::vector<Json> values;
    for(auto character:input) values.emplace_back(encode(std::u32string(1,character)));
    return Json::list(std::move(values));
}
Json search_result(const SearchResult& search) {
    std::vector<Json> matches, lps;
    for(const auto& m:search.matches) matches.push_back(Json{{"start",m.start},{"end",m.end},{"utf16_start",m.utf16_start},{"utf16_end",m.utf16_end}});
    for(auto value:search.lps) lps.emplace_back(value);
    const auto total=search.metrics.lps+search.metrics.search;
    return Json{{"matches",Json::list(std::move(matches))},{"lps",Json::list(std::move(lps))},{"metrics",metrics(search.metrics)},
        {"comparison",Json{{"kmp_total",total},{"naive_total",search.metrics.naive},
            {"difference",static_cast<std::int64_t>(total)-static_cast<std::int64_t>(search.metrics.naive)}}}};
}
Json distance_result(const DistanceResult& dp) {
    std::vector<Json> matrix, path, edits, source,target;
    for(const auto& row:dp.matrix) {std::vector<Json> cells;for(auto cell:row) cells.emplace_back(cell);matrix.push_back(Json::list(std::move(cells)));}
    for(auto [i,j]:dp.path) path.push_back(Json{{"i",i},{"j",j}});
    for(const auto& edit:dp.edits) edits.push_back(Json{{"kind",edit.kind},{"position",edit.position},{"before",edit.before},{"after",edit.after}});
    for(const auto& c:dp.aligned_source) source.emplace_back(c);
    for(const auto& c:dp.aligned_target) target.emplace_back(c);
    return Json{{"distance",dp.distance},{"matrix",Json::list(std::move(matrix))},{"path",Json::list(std::move(path))},{"edits",Json::list(std::move(edits))},
        {"aligned_source",Json::list(std::move(source))},{"aligned_target",Json::list(std::move(target))},{"metrics",metrics(dp.metrics)}};
}
const std::map<std::string,std::string> labels={
    {"initialize","Start / initialize"},{"complete","Output / end"},{"trie_edge_lookup","Character edge exists?"},{"prefix_missing","Missing prefix / empty output"},{"cache_read","Read top-eight cache"},
    {"dp_initialize","Initialize empty-string border"},{"dp_cell","Characters equal? / compute minimum"},{"dp_backtrack","Follow optimal predecessor"},{"edit_apply","Apply next edit"},
    {"lps_compare","Pattern characters equal?"},{"lps_write","Write LPS value"},{"lps_fallback","Reuse previous LPS"},{"search_initialize","Initialize text / pattern pointers"},
    {"kmp_compare","Text / pattern equal?"},{"kmp_advance","Advance pointers"},{"kmp_fallback","Retain text / fall back pattern"},{"match_found","Record match"}
};
void trace_json(Json& output,const Trace& trace) {
    std::vector<Json> events, nodes, edges;
    std::set<std::string> operations, links;
    std::size_t sequence=0;
    for(const auto& e:trace.events) {
        Json value{{"seq",sequence++},{"kind",e.kind},{"flow",e.flow},{"i",e.i},{"j",e.j},{"node",e.node},{"other",e.other},{"value",e.value},
            {"diagonal",e.diagonal},{"up",e.up},{"left",e.left},{"text",e.text},{"counters",metrics(e.counters)}};
        if(e.condition) value["condition"]=*e.condition;
        events.push_back(std::move(value));
        if(operations.insert(e.flow).second) nodes.push_back(Json{{"id",e.flow},{"label",labels.at(e.flow)},{"condition",e.condition.has_value()}});
    }
    for(std::size_t i=1;i<trace.events.size();++i) {
        const auto& from=trace.events[i-1]; const auto& to=trace.events[i];
        std::string label=from.condition?(*from.condition?"True":"False"):"";
        auto id=from.flow+"/"+to.flow+"/"+label;
        if(links.insert(id).second) edges.push_back(Json{{"id",id},{"from",from.flow},{"to",to.flow},{"label",label}});
    }
    output["events"]=Json::list(std::move(events));
    output["flow"]=Json{{"scope","Observed operations and branches only"},{"nodes",Json::list(std::move(nodes))},{"edges",Json::list(std::move(edges))}};
}
Json trie_structure(const Dictionary& dictionary,const Trace& trace) {
    std::set<std::size_t> selected{0};
    for(const auto& e:trace.events) {selected.insert(e.node);selected.insert(e.other);}
    auto endpoint=trace.events.back().node;
    for(auto word:dictionary.nodes[endpoint].top) {
        std::size_t node=0;
        for(auto c:dictionary.words[word].value) {node=dictionary.nodes[node].children.at(c);selected.insert(node);}
    }
    auto path_nodes=selected;
    for(auto id:path_nodes) {
        std::vector<std::pair<char32_t,std::size_t>> children(dictionary.nodes[id].children.begin(),dictionary.nodes[id].children.end());
        std::sort(children.begin(),children.end());
        for(auto [c,node]:children) { (void)c;if(selected.size()<400) selected.insert(node); }
    }
    auto copy=selected;
    for(auto node:copy) while(node) {node=dictionary.nodes[node].parent;selected.insert(node);}
    std::vector<Json> nodes,edges;
    std::map<std::size_t,std::size_t> rows;
    for(auto id:selected) {
        const auto& node=dictionary.nodes[id];
        std::size_t depth=0,current=id;
        while(current) {++depth;current=dictionary.nodes[current].parent;}
        nodes.push_back(Json{{"id",id},{"parent",node.parent},{"character",id?encode(std::u32string(1,node.character)):"∅"},{"terminal",node.terminal.has_value()},{"focus",path_nodes.contains(id)},
            {"depth",depth},{"row",rows[depth]++}});
        if(id) edges.push_back(Json{{"from",node.parent},{"to",id},{"character",encode(std::u32string(1,node.character))}});
    }
    return Json{{"nodes",Json::list(std::move(nodes))},{"edges",Json::list(std::move(edges))},{"total_nodes",dictionary.nodes.size()},{"hidden_nodes",dictionary.nodes.size()-selected.size()}};
}
}
Api::Api(const std::vector<std::string>& words,Limits limits) : limits_(limits),dictionary_(std::make_shared<Dictionary>(words,limits)) {}
std::shared_ptr<const Dictionary> Api::snapshot() const {std::lock_guard lock(mutex_);return dictionary_;}
void Api::publish(std::shared_ptr<const Dictionary> previous,std::shared_ptr<Dictionary> next) {
    std::lock_guard lock(mutex_);
    if(dictionary_!=previous) throw Error(409,"dictionary_version","Dictionary changed; refresh and try again");
    next->version=previous->version+1;
    dictionary_=std::move(next);
}
Reply Api::handle(std::string_view method,std::string_view path,std::string_view body) {
    try {
        if(body.size()>limits_.body) throw Error(413,"body_limit","Request body exceeds limit");
        if(!slots_.try_acquire()) throw Error(429,"busy","Two computations are already running; retry shortly");
        struct Guard {std::counting_semaphore<2>& slots;~Guard(){slots.release();}} guard{slots_};
        decode(body);
        auto input=crow::json::load(body.empty()?"{}":std::string(body));
        if(!input || input.t()!=Type::Object) throw Error(400,"json","Expected a JSON object");
        auto output=route(method,path,input).dump();
        if(path=="/api/trace" && output.size()>limits_.trace_bytes) throw Error(413,"trace_size","Serialized trace exceeds limit; shorten inputs");
        return {200,std::move(output)};
    } catch(const Error& e) {
        return {e.status,Json{{"error",Json{{"code",e.code},{"message",e.what()}}}}.dump()};
    } catch(const std::exception&) {
        return {400,Json{{"error",Json{{"code","invalid_request"},{"message","Invalid JSON value or field"}}}}.dump()};
    }
}
Json Api::route(std::string_view method,std::string_view path,const Read& body) {
    auto dictionary=snapshot();
    Json output{{"dictionary_version",dictionary->version}};
    if(method=="GET" && path=="/health") {output["status"]="ok"; return output;}
    auto metadata=[&](const Dictionary& current) {
        std::vector<Json> words;
        for(const auto& word:current.words) words.emplace_back(encode(word.value));
        Json value{{"dictionary_version",current.version},{"count",current.words.size()},{"words",Json::list(std::move(words))},
            {"scope","Single-user process; replacement changes current dictionary; usage resets on restart"},
            {"limits",Json{{"words",limits_.words},{"word",limits_.word},{"text",limits_.text},{"pattern",limits_.pattern},{"events",limits_.events},{"suggestions",limits_.suggestions}}}};
        return value;
    };
    if(method=="GET" && path=="/api/dictionary") return metadata(*dictionary);
    if(method!="POST" && method!="PUT") throw Error(404,"route","Unknown route");
    output["revision"]=integer(body,"revision");
    auto request_id=string_field(body,"request_id");
    if(request_id.empty() || request_id.size()>80) throw Error(400,"request_id","Request ID must contain 1–80 bytes");
    decode(request_id);
    output["request_id"]=request_id;
    if(method=="PUT" && path=="/api/dictionary") throw Error(403,"dictionary_locked","The bundled dictionary cannot be changed or edited");
    if(method!="POST") throw Error(404,"route","Unknown route");
    if(path=="/api/analyze" || path=="/api/accept") {
        auto text=text_field(body,"text",limits_.text);
        auto token=active_token(text,integer(body,"start"),integer(body,"end"));
        auto normalized=normalize(token.value);
        if(path=="/api/accept") {
            if(integer(body,"dictionary_version")!=dictionary->version) throw Error(409,"dictionary_version","Dictionary changed; refresh suggestions");
            if(token.value.empty()) throw Error(400,"token","Place the caret inside an English word");
            auto word=normalize(text_field(body,"word",limits_.word).points);
            if(!dictionary->find(word)) throw Error(400,"unknown_word","Accepted word is not in dictionary");
            auto replacement=replace_token(text,token,word);
            if(decode(replacement.text).points.size()>limits_.text) throw Error(413,"input_limit","Accepted word would exceed editor code-point limit");
            auto next=std::make_shared<Dictionary>(*dictionary);next->accept(word);publish(dictionary,next);
            output["text"]=replacement.text;output["caret"]=replacement.caret;output["dictionary_version"]=next->version;return output;
        }
        auto threshold=integer(body,"threshold");
        if(threshold<1 || threshold>3) throw Error(400,"threshold","Edit threshold must be 1, 2, or 3");
        PrefixResult prefix, scan;Metrics fuzzy_metrics;std::vector<Candidate> corrections;
        std::string status=normalized.size()>limits_.word?"too_long":"empty";
        if(!normalized.empty() && normalized.size()<=limits_.word) {
            prefix=dictionary->prefix(normalized);scan=dictionary->scan(normalized);
            if(prefix.exact) status="exact";
            else if(!prefix.words.empty()) status="prefix";
            else {corrections=dictionary->fuzzy(normalized,static_cast<int>(threshold),&fuzzy_metrics);status=corrections.empty()?"unknown":"misspelled";}
        }
        prefix.metrics.scan_checks=scan.metrics.scan_checks;prefix.metrics.visited=fuzzy_metrics.visited;prefix.metrics.cells=fuzzy_metrics.cells;
        prefix.metrics.scan_words=scan.metrics.scan_words;prefix.metrics.pruned=fuzzy_metrics.pruned;prefix.metrics.candidates=fuzzy_metrics.candidates;
        output["workload"]=Json{{"dictionary_words",dictionary->words.size()},{"trie_nodes",dictionary->nodes.size()},{"query_points",normalized.size()},{"word_limit",limits_.word}};
        output["token"]=Json{{"word",encode(token.value)},{"normalized",encode(normalized)},{"start",text.utf16[token.begin]},{"end",text.utf16[token.end]}};
        output["status"]=status;output["completions"]=candidates(prefix.words);output["corrections"]=candidates(corrections);output["metrics"]=metrics(prefix.metrics);return output;
    }
    if(path=="/api/search") {
        auto text=text_field(body,"text",limits_.text), pattern=text_field(body,"pattern",limits_.pattern);
        auto search=kmp(text,pattern);search.metrics.naive=naive(text,pattern).metrics.naive;
        auto result=search_result(search);result["dictionary_version"]=dictionary->version;result["revision"]=integer(body,"revision");result["request_id"]=request_id;return result;
    }
    if(path=="/api/trace") {
        auto mode=string_field(body,"mode");
        Trace trace(limits_.events);Metrics final;
        output["workload"]=Json{{"dictionary_words",dictionary->words.size()},{"trie_nodes",dictionary->nodes.size()}};
        output["schema_version"]=1;output["algorithm"]=mode;output["indexing"]="Unicode code points; zero-based, end-exclusive; matches include UTF-16";
        if(mode=="trie") {
            auto prefix=normalize(text_field(body,"prefix",limits_.word).points);
            auto result=dictionary->prefix(prefix,&trace);auto baseline=dictionary->scan(prefix);
            result.metrics.scan_checks=baseline.metrics.scan_checks;result.metrics.scan_words=baseline.metrics.scan_words;final=result.metrics;
            output["workload"]["prefix_points"]=prefix.size();
            output["inputs"]=Json{{"prefix",encode(prefix)}};output["initial"]=trie_structure(*dictionary,trace);output["initial"]["prefix"]=characters(prefix);
            output["result"]=Json{{"suggestions",candidates(result.words)}};
        } else if(mode=="dp") {
            auto source=text_field(body,"source",limits_.word),target=text_field(body,"target",limits_.word);
            auto result=levenshtein(source.points,target.points,&trace);final=result.metrics;
            output["workload"]["source_points"]=source.points.size();output["workload"]["target_points"]=target.points.size();
            output["inputs"]=Json{{"source",encode(source.points)},{"target",encode(target.points)}};
            output["initial"]=Json{{"source",characters(source.points)},{"target",characters(target.points)},{"matrix_rows",source.points.size()+1},{"matrix_columns",target.points.size()+1}};
            output["result"]=distance_result(result);
        } else if(mode=="kmp") {
            auto text=text_field(body,"text",limits_.text),pattern=text_field(body,"pattern",limits_.pattern);
            auto result=kmp(text,pattern,&trace);result.metrics.naive=naive(text,pattern).metrics.naive;final=result.metrics;
            output["workload"]["text_points"]=text.points.size();output["workload"]["pattern_points"]=pattern.points.size();
            output["inputs"]=Json{{"text",encode(text.points)},{"pattern",encode(pattern.points)}};
            output["initial"]=Json{{"text",characters(text.points)},{"pattern",characters(pattern.points)}};
            output["result"]=search_result(result);
        } else throw Error(400,"mode","Mode must be trie, dp, or kmp");
        output["metrics"]=metrics(final);
        output["metric_definitions"]=Json{{"lookups","Trie child-map lookups"},{"copied","Cached suggestions copied"},{"scan_checks","Full-scan prefix character comparisons"},
            {"cells","DP cells computed, excluding initialized borders"},{"visited","Trie nodes evaluated during fuzzy correction, excluding root"},
            {"pruned","Fuzzy rows whose minimum exceeded threshold; descendants were not evaluated"},{"candidates","Eligible fuzzy words before the suggestion limit"},
            {"scan_words","Dictionary words visited by the full prefix scan"},
            {"lps","KMP preprocessing character comparisons"},{"search","KMP search character comparisons"},{"naive","Naive character comparisons"},{"edits","Edits applied"},
            {"lps_fallbacks","LPS pointer fallback operations"},{"search_fallbacks","Search pointer fallbacks, including after a match"},{"matches","Exact occurrences recorded, including overlaps"}};
        trace_json(output,trace);return output;
    }
    throw Error(404,"route","Unknown route");
}
}
