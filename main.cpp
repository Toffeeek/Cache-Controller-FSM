#include <iostream>
#include <string>
#include <iomanip>
#include "config.hpp"
#include "Controller.hpp"
#include "Cache.hpp"
using namespace std;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void banner(const string& title)
{
    const int W = 70;
    string bar(W, '\xe2\x94\x80'); // UTF-8 '─'  (3 bytes each)
    // Just use plain dashes for portability
    string dash(W, '-');
    cout << "\n+" << dash << "+\n";
    cout << "|  " << left << setw(W - 2) << title << "|\n";
    cout << "+" << dash << "+\n";
}

// Pad a decimal/binary value to ADDRESS_SIZE binary digits.
string toBin(unsigned int val, int bits = ADDRESS_SIZE)
{
    string s = "";
    for (int b = bits - 1; b >= 0; --b)
        s += ((val >> b) & 1) ? '1' : '0';
    return s;
}

// Pretty-print the address fields.
void explainAddr(const string& label, const string& addr,
                 int tagBits, int idxBits, int wordBits, int byteBits)
{
    cout << "  " << label << " : " << addr << "\n";
    cout << "       tag["   << addr.substr(0, tagBits)                           << "]"
         << "  idx["        << addr.substr(tagBits, idxBits)                     << "]"
         << "  word["       << addr.substr(tagBits + idxBits, wordBits)          << "]"
         << "  byte["       << addr.substr(tagBits + idxBits + wordBits, byteBits) << "]\n";
}

void pause(const string& msg = "Press ENTER to continue...")
{
    cout << "\n  >> " << msg;
    cin.get();
}

// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    // =========================================================================
    // STEP 0 — Generate RAM
    // =========================================================================
    banner("STEP 0 | Generate main memory (ram.txt)");
    cout << "  Creating " << TOTAL_BYTES << " bytes of random data...\n";
    createRAM();
    cout << "  ram.txt is ready.\n";
    pause();

    // =========================================================================
    // STEP 1 — Build cache & controller
    // =========================================================================
    banner("STEP 1 | Initialise cache (8 lines x 4 words) and controller");

    //  Cache(linePower=3, wordPower=2)  →  2^3=8 lines,  2^2=4 words per line
    Cache* cache = new Cache(3, 2);
    Controller ctrl(cache);

    // Address field widths:
    //   byte offset = 2 bits  (log2(4 bytes/word))
    //   word offset = 2 bits  (log2(4 words/line))
    //   index       = 3 bits  (log2(8 lines))
    //   tag         = 16 - 2 - 2 - 3 = 9 bits
    const int byteBits = 2;
    const int wordBits = 2;
    const int idxBits  = 3;
    const int tagBits  = ADDRESS_SIZE - byteBits - wordBits - idxBits; // 9

    cout << "  Configuration:\n"
         << "    Lines/cache : 8   (index  = " << idxBits  << " bits)\n"
         << "    Words/line  : 4   (offset = " << wordBits << " bits)\n"
         << "    Bytes/word  : 4   (offset = " << byteBits << " bits)\n"
         << "    Tag size    : " << tagBits << " bits\n"
         << "    Address     : " << ADDRESS_SIZE << " bits total\n"
         << "\n  All lines start invalid (V=0, D=0):\n";
    cache->printCache();
    pause();

    // =========================================================================
    // STEP 2 — Explain the addresses we will use
    // =========================================================================
    banner("STEP 2 | Choose demo addresses");

    //   A  ->  tag=000000000  idx=000  word=00  byte=00  (line 0, word 0, byte 0)
    //   B  ->  tag=000000000  idx=001  word=01  byte=10  (line 1, word 1, byte 2)
    //   C  ->  tag=000000000  idx=010  word=10  byte=01  (line 2, word 2, byte 1)
    string addrA = toBin(0b0000000000000000); // 0x0000
    string addrB = toBin(0b0000000000010110); // 0x0016
    string addrC = toBin(0b0000000000101001); // 0x0029

    cout << "  Three addresses (each maps to a different cache line):\n\n";
    explainAddr("A (0x0000)", addrA, tagBits, idxBits, wordBits, byteBits);
    explainAddr("B (0x0016)", addrB, tagBits, idxBits, wordBits, byteBits);
    explainAddr("C (0x0029)", addrC, tagBits, idxBits, wordBits, byteBits);
    pause();

    // =========================================================================
    // STEP 3 — Queue and execute READ instructions (CompareTag → miss → Allocate)
    // =========================================================================
    banner("STEP 3 | Queue instructions and execute (READ path)");

    cout << "  Queuing:\n"
         << "    [1] addWord  A + B\n"
         << "    [2] addByte  B + C\n\n"
         << "  FSM path for each operand:\n"
         << "    IDLE -> COMPARE_TAG -> (miss) -> ALLOCATE (fetchBlock) -> hit -> read\n\n";

    ctrl.addInstruction(Instruction(Instruction::addWord, addrA, addrB));
    ctrl.addInstruction(Instruction(Instruction::addByte, addrB, addrC));

    cout << "  --- operate() output below ---\n";
    ctrl.operate();
    cout << "  --- end operate() ---\n";

    cout << "\n  Cache after instruction execution (lines 0,1,2 loaded, all CLEAN):\n";
    cache->printCache();
    pause();

    // =========================================================================
    // STEP 4a — Word write (writeToCache, word granularity)
    // =========================================================================
    banner("STEP 4a | Write full word to address A  -> line 0 becomes DIRTY");

    string newWord = "11001100110011001100110011001100"; // 0xCCCCCCCC
    cout << "  Target : addr A -> line 0, word 0\n"
         << "  Value  : " << newWord << "  (0xCCCCCCCC)\n\n"
         << "  writeToCache(addrA, newWord, wordGranularity=true)\n";

    bool ok = ctrl.writeToCache(addrA, newWord, true);
    cout << "\n  Result : " << (ok ? "SUCCESS" : "FAILED") << "\n"
         << "  Line 0 dirty bit should now be 1.\n";

    cout << "\n  Cache after word write:\n";
    cache->printCache();
    pause();

    // =========================================================================
    // STEP 4b — Byte write (writeToCache, byte granularity)
    // =========================================================================
    banner("STEP 4b | Write single byte to address B  -> line 1 becomes DIRTY");

    string newByte = "10101010"; // 0xAA
    cout << "  Target : addr B -> line 1, word 1, byte 2\n"
         << "  Value  : " << newByte << "  (0xAA)\n\n"
         << "  writeToCache(addrB, newByte, wordGranularity=false)\n"
         << "  Only 8 bits are patched; the other 3 bytes in that word are preserved.\n";

    ok = ctrl.writeToCache(addrB, newByte, false);
    cout << "\n  Result : " << (ok ? "SUCCESS" : "FAILED") << "\n"
         << "  Line 1 dirty bit should now be 1.\n";

    cout << "\n  Cache after byte write:\n";
    cache->printCache();
    pause();

    // =========================================================================
    // STEP 5 — Write to an un-cached address (expected miss)
    // =========================================================================
    banner("STEP 5 | Attempt write to un-cached address  -> expected miss");

    // Line 3 was never fetched.
    string addrD = toBin(0b0000000000111100); // line 3, word 3, byte 0
    cout << "  Address D (0x003C) maps to line 3 which is still invalid.\n";
    explainAddr("D (0x003C)", addrD, tagBits, idxBits, wordBits, byteBits);
    cout << "\n  writeToCache(addrD, newWord, true)  ->  should fail (miss):\n";

    ok = ctrl.writeToCache(addrD, newWord, true);
    cout << "\n  Result : " << (ok ? "SUCCESS — unexpected!" : "FAILED (cache miss) — correct behaviour") << "\n"
         << "  In a full write-allocate system the controller would now run\n"
         << "  ALLOCATE (fetchBlock) and then retry the write.\n";
    pause();

    // =========================================================================
    // STEP 6 — Evict the whole cache
    // =========================================================================
    banner("STEP 6 | evictCache()  ->  flush all dirty lines to RAM, then invalidate");

    cout << "  Expected behaviour:\n"
         << "    Line 0  DIRTY  ->  write-back to ram.txt  ->  invalidate\n"
         << "    Line 1  DIRTY  ->  write-back to ram.txt  ->  invalidate\n"
         << "    Line 2  CLEAN  ->  skip\n"
         << "    Lines 3-7 INVALID -> skip\n\n"
         << "  --- evictCache() output below ---\n";

    int flushed = ctrl.evictCache();

    cout << "  --- end evictCache() ---\n"
         << "\n  Lines written back : " << flushed << "\n";

    cout << "\n  Cache after eviction (dirty lines invalidated, rest unchanged):\n";
    cache->printCache();
    pause();

    // =========================================================================
    // STEP 7 — Re-fetch and verify write-back persisted in RAM
    // =========================================================================
    banner("STEP 7 | Re-fetch from RAM and verify the write-back persisted");

    cout << "  Lines 0 and 1 are now invalid, so the next access will miss\n"
         << "  and re-load them from ram.txt.\n"
         << "  Word 0 of line 0 should read back as 0xCCCCCCCC.\n\n"
         << "  Re-queueing addWord A + B ...\n"
         << "  --- operate() output below ---\n";

    ctrl.addInstruction(Instruction(Instruction::addWord, addrA, addrB));
    ctrl.operate();

    cout << "  --- end operate() ---\n";
    cout << "\n  Cache after re-fetch (clean, fresh from RAM):\n";
    cache->printCache();

    cout << "\n  Expected word at line 0 / word 0:\n"
         << "    " << newWord << "  (0xCCCCCCCC)\n";
    pause();

    // =========================================================================
    // STEP 8 — Full cycle diagram
    // =========================================================================
    banner("STEP 8 | Full write-allocate FSM cycle (summary)");

    cout << R"(
  Write-allocate, write-back cache cycle:

    CPU issues READ or WRITE
         |
         v
    [IDLE] -> picks next instruction
         |
         v
    [COMPARE TAG]
         |--- HIT ---> serve read/write from cache
         |              write? -> mark line DIRTY (writeToCache)
         |
         `--- MISS ---+
                      |
                      v
               [WRITE BACK]   <- only if the EVICTED slot is dirty
                      |          writeBackLine() flushes it to RAM
                      v
               [ALLOCATE]     <- fetchBlock() loads new block from RAM
                      |
                      v
               retry access   <- now a guaranteed hit

    evictCache() = bulk WRITE BACK of every dirty line
                   (called when cache is full or at program end)

)" ;
    pause("Press ENTER to exit.");

    delete cache;
    return 0;
}
