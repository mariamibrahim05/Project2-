# Effect of Associativity on Cache Performance

This project implements a single-level set-associative data-cache simulator and
uses it to study how cache associativity affects hit ratio. The cache capacity,
line size, replacement policy, and write policy remain fixed while the number of
ways changes from 1 to 16.

The program runs six provided memory-access generators at five associativities,
producing 30 experiments in total. Each experiment starts from a cold cache and
executes 1,000,000 memory accesses.

## Cache configuration

| Parameter | Value |
|---|---|
| Cache size | 64 KB |
| Cache-line size | 64 B |
| DRAM address space | 64 MB |
| Associativities | 1, 2, 4, 8, and 16 ways |
| Replacement policy | Random |
| Write policy | Write-back with write-allocate |
| Accesses per run | 1,000,000 |

The number of sets is recalculated for every associativity:

```text
number of sets = cache size / (line size * number of ways)
```

Each cache line stores a tag, valid bit, and dirty bit. On a miss, the simulator
uses an invalid line if one is available. Otherwise, `randReplace()` selects a
random victim. Evicting a dirty line increases the write-back counter.

## Memory generators

The supplied generators are not modified.

| Generator | Access pattern | Expected behavior |
|---|---|---|
| `memGen1` | Two-line same-set ping-pong | Thrashes at 1-way; nearly all hits at 2+ ways |
| `memGen2` | Random accesses across DRAM | Very low hit ratio at every associativity |
| `memGen3` | Eight-line same-set cycle | Improves gradually, then reaches nearly all hits at 8 ways |
| `memGen4` | Four conflicting lines across 16 sets | Reaches nearly all hits at 4+ ways |
| `memGen5` | Sequential four-byte words | Constant 93.75% hit ratio from spatial locality |
| `memGen6` | Stack push/pop | Nearly all hits because the working set is small |

## Building the project

The project requires CMake 3.16 or newer and a C++17 compiler.

### CLion

1. Open the repository folder in CLion.
2. Allow CLion to reload the CMake project.
3. Select the shared **Run Cache Experiments** configuration.
4. Press **Run**.

The configuration builds `cache_sim` and runs the complete experiment from the
project directory.

### Command line

Configure and build the program:

```powershell
cmake -S . -B build
cmake --build build
```

Run it from the project directory on Windows:

```powershell
.\build\cache_sim.exe
```

Depending on the selected CMake generator, the executable may instead be inside
a configuration folder such as `build\Debug\cache_sim.exe`.

No interactive input is required. The program automatically tests all six
generators with 1, 2, 4, 8, and 16 ways.

## Output

With no command-line argument, the simulator writes its data to `results.csv`:

```powershell
.\cache_sim.exe
```

An optional argument selects a different output path:

```powershell
.\cache_sim.exe my_results.csv
```

The CSV contains the following columns:

| Column | Meaning |
|---|---|
| `generator` | Memory generator name |
| `ways` | Associativity used for the run |
| `hit_ratio` | Fraction of accesses that hit |
| `miss_ratio` | Fraction of accesses that miss |
| `writebacks` | Dirty-line evictions |
| `reads` | Number of read requests |
| `writes` | Number of write requests |

## Testing

The repository includes an automated CTest regression test. It performs a fresh
30-run experiment, verifies that the generated CSV contains one header and 30
data rows, and compares the output byte-for-byte with the verified
`results.csv` file.

After building, run:

```powershell
ctest --test-dir build --output-on-failure
```

The verified test result is:

```text
1/1 Test #1: cache_regression ................. Passed
100% tests passed, 0 tests failed out of 1
```

Additional testing details are available in [TESTING.md](TESTING.md).

## Main results

The experiment demonstrates that associativity mainly reduces conflict misses:

- `memGen1` reaches its useful limit at 2 ways.
- `memGen4` reaches its useful limit at 4 ways.
- `memGen3` reaches its useful limit at 8 ways.
- Additional ways provide no measurable benefit after the conflicting group fits.
- Random DRAM access remains capacity-limited at every associativity.
- Sequential and stack patterns are already supported by spatial or temporal locality.

The complete analysis, hit-ratio table, and plot are available in
[Project2_Final_Report.pdf](Project2_Final_Report.pdf).

## Repository contents

| Path | Purpose |
|---|---|
| `cache.cpp` | Cache implementation, generators, and experiment harness |
| `results.csv` | Verified output from all 30 experiments |
| `CMakeLists.txt` | Build and automated-test configuration |
| `.run/` | Shared CLion run configuration |
| `tests/run_regression.cmake` | Full experiment regression test |
| `TESTING.md` | Recorded testing evidence |
| `Project2_Final_Report.pdf` | Final project report |
