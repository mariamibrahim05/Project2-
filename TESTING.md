# Testing

We tested the simulator before using its results in the report. The main goal
was to make sure that the program runs all the required cases and gives the
same output every time.

## What we tested

We used the fixed project settings: a 64 KB cache, 64-byte lines, random
replacement, and write-back with write-allocate. We ran all six memory
generators with 1, 2, 4, 8, and 16 ways. This gives 30 runs in total, with
1,000,000 accesses in each run.

First, we built the program with the warning options enabled. The final build
completed without compiler warnings. We then ran the full experiment and
checked the following:

- The program completed without crashing.
- The CSV had one header followed by 30 result rows.
- The hit ratio and miss ratio added up to 1 for every row.
- A second run produced exactly the same results as `results.csv`.
- `memGen1` had 0% hits at 1-way and almost 100% at 2-way and above.
- `memGen5` stayed at 93.75% for every associativity, which is the expected
  result because one 64-byte line contains sixteen 4-byte words.

These last two checks were useful because their expected behavior can be
calculated without depending on the simulator output.

## Automated check

We added a CTest test called `cache_regression`. It runs the complete experiment
again, creates `test_results.csv` inside the build folder, checks that it has 30
data rows, and compares it with the saved `results.csv` file.

After building the project, we ran:

```text
ctest --output-on-failure
```

The test took about 9.5 seconds on our machine and gave this result:

```text
1/1 Test #1: cache_regression ................. Passed    9.45 sec

100% tests passed, 0 tests failed out of 1
```

The fresh CSV matched the saved results exactly, so we used those results for
the table, plot, and analysis in the final report.

