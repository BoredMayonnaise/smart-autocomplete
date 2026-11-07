# Presentation walkthrough

Start `./build/smartautocomplete`, then open http://127.0.0.1:8080/. Keep the server running. Disconnect internet to demonstrate local runtime.

1. **Prefix:** choose `algor` from **Try an example**. Step through five cyan edges. Cached results include `algorithm`, `algorithmic`, and `algorithms` without traversing their suffixes; the expanded vocabulary can also include possessive forms. Compare child lookups with separately labeled full-scan character checks. **Show branches** reveals neighboring context with an accurate hidden-node count.
2. **Acceptance:** type `Hello, ALGOR!`, place the caret before `!`, select a completion with Down, and press Tab. Show capitalization, punctuation, usage, and version change.
3. **One edit:** choose `programing` from **Try an example**. Step across border initialization and a few cells. Show diagonal/delete/insert costs. Scrub to the end; the source becomes `programming` after one insertion.
4. **Deeper edit logic:** choose `recieve`. The distance is two, not one: this is standard Levenshtein. Choose `kitten` for distance three. Toggle Flow and follow the same trace through initialization, cells, backtracking, and edits.
5. **Overlaps:** choose `banana`. Search `ana` in `banana bandana banana`. Show starts 1, 3, 11, 16, 18; use Next/Previous to select each match in the editor.
6. **Fallback:** choose `aaaaab`. Play slowly through LPS comparisons and fallback. The text pointer does not move backward. Scrub to completion: match start 31; KMP 9 + 68 = 77 character comparisons; naive 192. Do not claim KMP always wins.

For a counterexample, search `abc` in `abc`: KMP uses 5 comparisons including preprocessing, versus naive's 3. The completed comparison summary reports 2 more comparisons. During playback, the KMP count is partial and the naive count is the complete baseline; the final advantage appears only at completion.

Type `programing` with edit threshold 1 to show dictionary correction work: visited trie nodes, computed DP cells, pruned rows, and all eligible words before retaining eight. These counters measure the dictionary search; the pair matrix visualizer separately traces `programing` to `programming`. The bundle contains 10,000 words; changing usage updates ranking, not edit distances.
7. **Replay:** restart, step, pause, change speed, and scrub backward. Matrix cells, LPS writes, matches, and counters return to their earlier states. No Step request recomputes the algorithm.
8. **Dictionary:** open Dictionary, try a word containing `!`, and show rejection without changing the dictionary. Replace with `cat`, `cart`, and duplicate `cat`; two unique entries remain. Explain that this affects only the running process. Restart to restore the bundled list.

Visual labels and analytics carry the in-app teaching. These notes are presenter guidance, not required on-screen prose.
