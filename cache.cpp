// Project 2: Effect of Associativity on Cache Performance
// STARTER CODE for students.
//
// Provided: memory request generators + experiment harness (CSV output).
// Your job: implement a real set-associative cache in initCache() and
// cacheAccess(), then run experiments, plot hit ratio vs ways, analyze.
//
// Do not change the memory generators (needed for comparable results).

#include <iostream>
#include <iomanip>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cassert>

using namespace std;

#define		DBG				0
#define		DRAM_SIZE		(64*1024*1024)
#define		CACHE_SIZE		(64*1024)	// fixed: 64 KB
#define		LINE_SIZE		64		// fixed: 64 B
#define		WORD_SIZE		4		// word-aligned data accesses

enum cacheResType {MISS=0, HIT=1};
enum accessType   {READ=0, WRITE=1};

// One memory transaction produced by a generator
struct MemReq {
	unsigned int addr;
	accessType   type;
};

/* -------------------------------------------------------------------- */
/* Random number generators                                             */
/*   rand_()       — used only by memory generators (stable streams)    */
/*   randReplace() — use this for random victim selection               */
/* -------------------------------------------------------------------- */
unsigned int m_w = 0xABABAB55;    /* must not be zero, nor 0x464fffff */
unsigned int m_z = 0x05080902;    /* must not be zero, nor 0x9068ffff */

unsigned int m_w_rep = 0x12345678;
unsigned int m_z_rep = 0x9ABCDEF0;

unsigned int rand_()
{
	m_z = 36969 * (m_z & 65535) + (m_z >> 16);
	m_w = 18000 * (m_w & 65535) + (m_w >> 16);
	return (m_z << 16) + m_w;  /* 32-bit result */
}

unsigned int randReplace()
{
	m_z_rep = 36969 * (m_z_rep & 65535) + (m_z_rep >> 16);
	m_w_rep = 18000 * (m_w_rep & 65535) + (m_w_rep >> 16);
	return (m_z_rep << 16) + m_w_rep;
}

// Uniform real in [0, 1)
static double rand01()
{
	return (rand_() & 0xFFFFFF) / double(0x1000000);
}

// Word-align an address and keep it inside DRAM
static unsigned int alignAddr(unsigned int a)
{
	a &= ~(WORD_SIZE - 1u);
	return a % DRAM_SIZE;
}

/* -------------------------------------------------------------------- */
/* Memory request generators                                            */
/*                                                                      */
/* Designed so hit-ratio vs ways shows strong, interpretable trends.    */
/* Addresses of the form  set*LINE_SIZE + k*CACHE_SIZE  always share    */
/* one set for every W in {1,2,4,8,16}.                                 */
/* Do not change generator logic (needed for comparable results).       */
/* -------------------------------------------------------------------- */

// Address of conflicting line k in set s (word-aligned).
// For any W in the study, lines with the same s map to the same set.
static unsigned int conflictAddr(unsigned int setId, unsigned int lineId)
{
	return alignAddr(setId * LINE_SIZE + lineId * CACHE_SIZE);
}

static accessType randRW(double writeProb)
{
	return (rand01() < writeProb) ? WRITE : READ;
}

// --- Generator state -------------------------------------------------
static unsigned int g1_i = 0;	// memGen1: K=2 cyclic
static unsigned int g3_i = 0;	// memGen3: K=8 cyclic
static unsigned int g4_i = 0;	// memGen4: multi-set K=4
static unsigned int g5_i = 0;	// memGen5: sequential
static unsigned int g6_sp = 0;	// memGen6: stack

static const unsigned int G4_NSETS = 16;	// distinct sets (safe for W<=16)
static const unsigned int G4_K     = 4;	// aliases per set

static const unsigned int G6_STACK_BASE = 8*1024*1024;
static const unsigned int G6_STACK_MAX  = 16*1024;	// tiny stack -> high hits

// memGen1 — K=2 same-set ping-pong (classic thrash / fit).
// Alternates A and A+CACHE_SIZE. Both always map to set 0.
// Expected: ways=1 → ~0% hits; ways>=2 → ~100% hits.
MemReq memGen1()
{
	MemReq r;
	r.addr = conflictAddr(0, g1_i % 2);
	g1_i++;
	r.type = randRW(0.30);
	return r;
}

// memGen2 — Random over full DRAM (capacity thrashing).
// Almost no spatial or temporal locality. Working set >> cache.
// Expected: hit rate ~0% for ALL ways (associativity cannot help).
// ~50% reads / 50% writes.
MemReq memGen2()
{
	MemReq r;
	r.addr = alignAddr(rand_() % DRAM_SIZE);
	r.type = randRW(0.50);
	return r;
}

// memGen3 — K=8 same-set cyclic thrash / fit.
// Walks 8 aliases in one set: thrash until ways >= 8.
// Expected: ways in {1,2,4} low; ways in {8,16} ~100%.
MemReq memGen3()
{
	MemReq r;
	r.addr = conflictAddr(0, g3_i % 8);
	g3_i++;
	r.type = randRW(0.30);
	return r;
}

// memGen4 — Multi-set K=4 thrash / fit (program-like volume).
// 16 independent sets, each with 4 conflicting lines.
// Expected: ways < 4 → ~0%; ways >= 4 → ~100%.
MemReq memGen4()
{
	MemReq r;
	unsigned int t = g4_i++;
	unsigned int setId  = (t / G4_K) % G4_NSETS;
	unsigned int lineId = t % G4_K;
	r.addr = conflictAddr(setId, lineId);
	r.type = randRW(0.30);
	return r;
}

// memGen5 — Sequential stream (spatial locality only).
// Word-by-word scan; hit rate ~15/16 from line reuse within a block.
// Expected: flat ~93.75% for all ways.
MemReq memGen5()
{
	MemReq r;
	r.addr = alignAddr(g5_i * WORD_SIZE);
	g5_i++;
	r.type = randRW(0.10);
	return r;
}

// memGen6 — Stack push/pop (high hit rate).
// Tiny working set around a stack pointer: WRITE on push, READ on pop.
// Expected: ~100% hits for ALL ways (WS << cache; no conflict pressure).
MemReq memGen6()
{
	MemReq r;
	unsigned int depth = g6_sp;

	bool push;
	if (depth < WORD_SIZE)
		push = true;
	else if (depth + WORD_SIZE >= G6_STACK_MAX)
		push = false;
	else
		push = (rand01() < 0.50);

	if (push) {
		g6_sp += WORD_SIZE;
		r.addr = alignAddr(G6_STACK_BASE + g6_sp);
		r.type = WRITE;
	} else {
		r.addr = alignAddr(G6_STACK_BASE + g6_sp);
		r.type = READ;
		g6_sp -= WORD_SIZE;
	}
	return r;
}

// Call before every simulation run so each (ways, generator) pair
// sees a cold start and a reproducible request stream.
void resetMemGens()
{
	g1_i = 0;
	g3_i = 0;
	g4_i = 0;
	g5_i = 0;
	g6_sp = 0;
	m_w = 0xABABAB55;
	m_z = 0x05080902;
}

typedef MemReq (*MemGenFn)();

static MemGenFn memGens[] = {
	memGen1, memGen2, memGen3, memGen4, memGen5, memGen6
};
static const char* memGenNames[] = {
	"memGen1", "memGen2", "memGen3", "memGen4", "memGen5", "memGen6"
};
static const char* memGenDesc[] = {
	"K=2 same-set ping-pong (step at 2-way)",
	"random over DRAM (low hits, all ways)",
	"K=8 same-set cyclic (step at 8-way)",
	"16 sets x K=4 multi-set thrash (step at 4-way)",
	"sequential stream (flat ~93.75%)",
	"stack push/pop (high hits, all ways)"
};
static const int NUM_GENS = 6;

static const char* accMsg[2] = {"READ", "WRITE"};

/* -------------------------------------------------------------------- */
/* Cache simulator — YOUR WORK GOES HERE                                */
/*                                                                      */
/* Spec (see handout):                                                  */
/*   - Single-level set-associative cache                               */
/*   - Size CACHE_SIZE, line size LINE_SIZE (both fixed)                */
/*   - Associativity = ways  (sweep 1, 2, 4, 8, 16 in main)             */
/*   - Random replacement  (use randReplace() when you need a victim)   */
/*   - Write-back + write-allocate                                      */
/*                                                                      */
/* Starter below is a placeholder only: every access is treated as a    */
/* hit. Replace it with a real simulator, then run the experiment       */
/* harness, collect CSV data, plot, and analyze.                        */
/* -------------------------------------------------------------------- */

static unsigned int g_ways = 1;
static unsigned long long g_writebacks = 0;

// TODO: add the data structures you need for tags / valid / dirty / ...

// Cold-reset (and reconfigure) the cache for the given number of ways.
void initCache(unsigned int ways)
{
	g_ways = ways;
	g_writebacks = 0;
	m_w_rep = 0x12345678;
	m_z_rep = 0x9ABCDEF0;

	// TODO: allocate / clear cache state for this associativity
	(void)g_ways;
}

// Simulate one memory access. Return HIT or MISS.
// Placeholder: always HIT (ideal cache). Students must replace this.
cacheResType cacheAccess(unsigned int addr, accessType type)
{
	(void)addr;
	(void)type;
	return HIT;
}

unsigned long long getWritebacks()
{
	return g_writebacks;
}

/* -------------------------------------------------------------------- */
/* Experiment harness                                                   */
/* -------------------------------------------------------------------- */
const char* hitMsg[2] = {"Miss", "Hit"};

// Use 100 while debugging; set to 1000000 for reported results
#define		NO_OF_Iterations	1000000

static const unsigned int WAYS_LIST[] = {1, 2, 4, 8, 16};
static const int NUM_WAYS = 5;

struct RunResult {
	double hit_ratio;
	double miss_ratio;
	unsigned long long writebacks;
	unsigned int reads;
	unsigned int writes;
};

// Run one (generator, ways) experiment
RunResult runExperiment(int genIndex, unsigned int ways, bool verbose)
{
	resetMemGens();
	initCache(ways);

	unsigned int hits = 0;
	unsigned int reads = 0, writes = 0;
	MemGenFn gen = memGens[genIndex];

	for (int i = 0; i < NO_OF_Iterations; i++) {
		MemReq req = gen();
		if (req.type == READ)
			reads++;
		else
			writes++;

		cacheResType r = cacheAccess(req.addr, req.type);
		if (r == HIT)
			hits++;

		if (verbose && DBG)
			cout << "0x" << setfill('0') << setw(8) << hex << req.addr
			     << " " << accMsg[req.type]
			     << " (" << hitMsg[r] << ")\n" << dec;
	}

	RunResult out;
	out.hit_ratio = static_cast<double>(hits) / NO_OF_Iterations;
	out.miss_ratio = 1.0 - out.hit_ratio;
	out.writebacks = getWritebacks();
	out.reads = reads;
	out.writes = writes;

	if (verbose) {
		cout << "  reads=" << reads << " writes=" << writes
		     << " read_frac="
		     << fixed << setprecision(3)
		     << (double)reads / NO_OF_Iterations
		     << " writebacks=" << out.writebacks << "\n";
	}

	return out;
}

int main(int argc, char* argv[])
{
	const char* outPath = "results.csv";
	if (argc >= 2)
		outPath = argv[1];

	cout << "Project 2: Associativity vs Cache Hit Ratio (starter)\n";
	cout << "NOTE: cacheAccess() is a placeholder (always HIT) until you implement it.\n";
	cout << "Cache size = " << CACHE_SIZE << " B, Line size = "
	     << LINE_SIZE << " B, Replacement = random\n";
	cout << "Write policy = write-back + write-allocate\n";
	cout << "Iterations per run = " << NO_OF_Iterations << "\n\n";

	cout << "Generators:\n";
	for (int g = 0; g < NUM_GENS; g++)
		cout << "  " << memGenNames[g] << ": " << memGenDesc[g] << "\n";
	cout << "\n";

	FILE* fp = fopen(outPath, "w");
	if (!fp) {
		cerr << "Failed to open " << outPath << " for writing\n";
		return 1;
	}

	// CSV header (stdout + file)
	const char* header =
		"generator,ways,hit_ratio,miss_ratio,writebacks,reads,writes\n";
	cout << header;
	fputs(header, fp);

	for (int g = 0; g < NUM_GENS; g++) {
		for (int w = 0; w < NUM_WAYS; w++) {
			unsigned int ways = WAYS_LIST[w];
			RunResult res = runExperiment(g, ways, false);

			cout << memGenNames[g] << ","
			     << ways << ","
			     << fixed << setprecision(6) << res.hit_ratio << ","
			     << res.miss_ratio << ","
			     << res.writebacks << ","
			     << res.reads << ","
			     << res.writes << "\n";

			fprintf(fp, "%s,%u,%.6f,%.6f,%llu,%u,%u\n",
				memGenNames[g], ways,
				res.hit_ratio, res.miss_ratio,
				(unsigned long long)res.writebacks,
				res.reads, res.writes);
		}
	}

	fclose(fp);
	cout << "\nWrote " << outPath << "\n";
	return 0;
}
