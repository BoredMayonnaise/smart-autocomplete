# Dictionary provenance and regeneration

`words.txt` contains **50,000 unique English words**. It combines the original 242-word teaching vocabulary with SCOWL's common English word lists. All original presentation words remain present. This is a bounded practical vocabulary, not every English word or the 10,000 most frequent words in exact frequency order.

## Sources and licenses

- `teaching-words.txt`: the original, hand-curated project vocabulary, dedicated to the public domain under CC0-1.0: https://creativecommons.org/publicdomain/zero/1.0/legalcode.
- SCOWL **2020.12.07**, `final/english-words.10` and `final/english-words.20`: https://downloads.sourceforge.net/wordlist/scowl-2020.12.07.tar.gz. Archive SHA-256: `5587667caa20c4891390c2d42dbb4d5c4c3f41bee77af1457ece3ba23fb859cc`.
- SCOWL's collective work is Copyright 2000–2018 Kevin Atkinson, with permissive redistribution terms and the upstream source notices. The level 10/20 source lists include public-domain Moby and Brian Kelk material. The complete upstream `Copyright` file is preserved, unmodified, as `SCOWL-Copyright.txt`; it also describes other levels not used here. Retain this file with redistributed vocabulary.

## Selection

`tools/make_dictionary.cpp` keeps supported lowercase English forms from SCOWL levels 10, 20, and 35, plus the system dictionary and every teaching word. It reproduces the former 5,000- and 10,000-word selections, then fills to 50,000 by evenly sampling the remaining sorted words. This avoids an alphabetically biased cutoff while keeping the bundle at the application's default 50,000-word limit. The generator does not set runtime usage counts or change suggestion ranking.

To regenerate from the verified, extracted archive, run from the project root:

```sh
g++ -std=c++20 -Wall -Wextra -Wpedantic tools/make_dictionary.cpp -o /tmp/make_dictionary
/tmp/make_dictionary data/teaching-words.txt /path/to/scowl-2020.12.07/final/english-words.10 /path/to/scowl-2020.12.07/final/english-words.20 /path/to/scowl-2020.12.07/final/english-words.35 /etc/dictionaries-common/words > data/words.txt
```

Generated file SHA-256: `d7a861f0f4c6f961f27515766ef2dbce8006afb649d154b00eaac2306e88d347`. C++ tests validate all 10,000 entries through the production dictionary constructor, verify original teaching-word membership, and check everyday vocabulary and the presentation misspellings. All runtime assets are bundled; no source download or generator is needed at runtime.

## Runtime policy

Only ASCII English letters and internal apostrophes/hyphens are accepted as dictionary words. Case normalization folds ASCII A–Z only. Non-ASCII editor content is preserved, and KMP/DP process arbitrary valid Unicode scalars. This is not full Unicode word segmentation, case folding, or normalization.

The dictionary editor replaces the process-local vocabulary, not this file. Acceptance counts remain process-local and reset on normal restart or explicit replacement. The UI reports the vocabulary count separately from the 10,000-word capacity. Focused trie rendering preserves readability without shrinking the dictionary or inventing visited suffix edges.
