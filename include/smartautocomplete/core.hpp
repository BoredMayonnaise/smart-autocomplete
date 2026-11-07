#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace smart {
struct Error : std::runtime_error {
    int status;
    std::string code;
    Error(int status, std::string code, std::string message)
        : std::runtime_error(std::move(message)), status(status), code(std::move(code)) {}
};
struct Limits {
    std::size_t words = 50000, word = 32, text = 8000, pattern = 1000, suggestions = 8;
    std::size_t events = 20000, body = 1048576, trace_bytes = 8388608;
};
struct Text {
    std::u32string points;
    std::vector<std::size_t> bytes, utf16;
    std::size_t index(std::size_t utf16_offset) const;
};
Text decode(std::string_view input);
std::string encode(std::u32string_view input);
std::u32string normalize(std::u32string_view input);
bool word_letter(char32_t c);
struct Token {
    std::size_t begin = 0, end = 0;
    std::u32string value;
};
Token active_token(const Text&, std::size_t start, std::size_t end);
struct Replacement { std::string text; std::size_t caret; };
Replacement replace_token(const Text&, const Token&, std::u32string_view word);

struct Metrics {
    std::size_t lookups = 0, copied = 0, scan_checks = 0, visited = 0, cells = 0;
    std::size_t lps = 0, search = 0, naive = 0, edits = 0;
    std::size_t scan_words = 0, pruned = 0, candidates = 0;
    std::size_t lps_fallbacks = 0, search_fallbacks = 0, matches = 0;
    bool operator==(const Metrics&) const = default;
};
struct Event {
    std::string kind, flow;
    std::size_t i = 0, j = 0, node = 0, other = 0;
    int value = 0, diagonal = -1, up = -1, left = -1;
    std::optional<bool> condition;
    std::string text;
    Metrics counters;
    bool operator==(const Event&) const = default;
};
struct Trace {
    std::vector<Event> events;
    std::size_t limit;
    explicit Trace(std::size_t limit = 20000) : limit(limit) {}
    void add(Event event);
};
struct Candidate {
    std::string word;
    int distance = 0;
    std::uint64_t usage = 0;
    bool operator==(const Candidate&) const = default;
};
struct Word { std::u32string value; std::uint64_t usage = 0; };
struct Node {
    std::unordered_map<char32_t, std::size_t> children;
    std::optional<std::size_t> terminal;
    std::vector<std::size_t> top;
    std::size_t parent = 0;
    char32_t character = 0;
};
struct PrefixResult { std::vector<Candidate> words; Metrics metrics; bool exact = false; };
class Dictionary {
    Limits limits_;
    void rank(std::vector<std::size_t>&) const;
public:
    std::vector<Word> words;
    std::vector<Node> nodes;
    std::uint64_t version = 1;
    explicit Dictionary(const std::vector<std::string>&, Limits = {});
    std::optional<std::size_t> find(std::u32string_view) const;
    PrefixResult prefix(std::u32string_view, Trace* = nullptr) const;
    PrefixResult scan(std::u32string_view) const;
    std::vector<Candidate> fuzzy(std::u32string_view, int threshold, Metrics* = nullptr) const;
    void accept(std::u32string_view);
};
struct Edit {
    std::string kind;
    std::size_t position;
    std::string before, after;
};
struct DistanceResult {
    int distance = 0;
    std::vector<std::vector<int>> matrix;
    std::vector<std::pair<std::size_t, std::size_t>> path;
    std::vector<Edit> edits;
    std::vector<std::string> aligned_source, aligned_target;
    Metrics metrics;
};
DistanceResult levenshtein(std::u32string_view, std::u32string_view, Trace* = nullptr);
struct Match {
    std::size_t start, end, utf16_start, utf16_end;
    bool operator==(const Match&) const = default;
};
struct SearchResult {
    std::vector<Match> matches;
    std::vector<std::size_t> lps;
    Metrics metrics;
};
SearchResult kmp(const Text&, const Text&, Trace* = nullptr);
SearchResult naive(const Text&, const Text&);
}
