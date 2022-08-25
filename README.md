# blopy
Read numpy-BLOBs in sqlite

Compile this extention and load it in sqlite3 (instructions in `blopy.c`). Then you can wrap your numpy-BLOB column with the provided functions like `isnp()`, `np_ver()`, `np_size()`, `np_desc()` and most importantly `np()` which returns the first few entries of any "readable" (int, double, complex, no str or object) numpy array as a text representation.

This is my first real C-program. So I'm sorry for all the possible pointer issues. Please address any related issues in a kind tone.

See also
========
 * https://github.com/daniel-edler/sqlWrapper/ A python module which lets you add and retrieve numpy arrays transparently. Limited support for postgres
