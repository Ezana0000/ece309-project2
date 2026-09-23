# Design Log — Project 2

(500–800 words total. See spec §5 for what each section must cover.)

## Growth factor and amortized cost
`Conversation` doubles its capacity whenever it becomes full, so the capacity goes 0 → 1 → 2 → 4 → 8 and so on. Each `append` writes one element, and when the buffer grows, the existing elements have to be copied over.
For n appends, the reallocations happen at sizes 1, 2, 4, …, 2^m, where 2^m < n. The total number of elements copied is 1 + 2 + 4 + … + 2^m = 2^(m+1) − 1 < 2n. There are also n normal writes, so the total work is less than 3n. This makes the total work O(n), meaning each `append` is O(1) amortized. Basically, the occasional expensive resize is spread out over the normal appends.
Growing by a fixed amount would be worse because the array would have to be copied much more often. That would lead to about n²/2k moves, or O(n) work per append. I used a growth factor of 2 because its simple and gives a good balance between copying and unused space. `test_growth_doubles` checks that the buffer gets reallocated exactly when the size is 0, 1, 2, 4, … across 1000 appends.

## Rule of Five evidence
`Conversation` owns its buffer, so all five special member functions need to handle the memory correctly. The destructor calls `delete[] data_`, which is safe even when `data_` is `nullptr`.
The copy constructor allocates a new buffer and copies every `Message`, meaning the copied object doesn't share the original's memory. Copy assignment uses copy-and-swap: it creates a copy, swaps the data, and then the temporary frees the old buffer. This also handles self-assignment without needing another check.
The move constructor uses `std::exchange` to take the other object's buffer, size, and capacity while leaving the source as `nullptr` and 0. Move assignment first frees its own buffer and then takes the other object's buffer. It checks for self-move so it doesn't delete the buffer by accident. Both move operations are `noexcept`.
`test_copy_is_deep` checks that copies have different addresses, and `test_move_steals` checks that the pointer gets transferred and the moved-from object is still usable. AddressSanitizer also helps catch problems like double frees and memory leaks.

## Sentinel scanner: bounded pending_ proof
Let S be the sentinel length, which is 20. The goal is to show that after every `feed()`, `pending_.size()` is at most S − 1.
This can be shown by induction. At the start, `pending_` is empty, so the claim is true. During `feed()`, there are three cases:

1. The sentinel is found, so `pending_` is cleared.
2. The whole window has fewer than S characters, so the entire window becomes `pending_`, which is at most S − 1.
3. The window is larger, so only the last S − 1 characters are kept.

Therefore, `pending_` can never be larger than 19 characters. This also means a sentinel split between two chunks won't be missed, because its starting point has to be within `pending_`. Any text that was already emitted can't be the start of a sentinel, since if it was, the whole sentinel would have fit in the window and been found.
Each call searches at most (S − 1) + |chunk| characters, so the work is O(chunk) instead of O(N²). `test_scanner_bounded_memory` tests this by feeding 4 MB one byte at a time and checking that bytes fed minus bytes emitted never goes above 19.

## What I would change differently
I would change the scanner so it doesn't always hold the last S − 1 characters. A lot of those characters can't actually be the beginning of the sentinel, so normal text can get delayed until the next chunk or `flush()`.
Instead, it could keep only the longest suffix that matches the beginning of the sentinel. For normal text this would usually be zero, so output could happen right away. KMP would be a good way to do this because it could keep track of how many sentinel characters have matched instead of storing the whole pending string.
I would also consider using raw storage instead of `new Message[n]`, since the current version default-constructs every slot before overwriting them. For this project the difference is probably small, but it would avoid some unnecessary work.
