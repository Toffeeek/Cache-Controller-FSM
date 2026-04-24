# Cache-Controller-FSM

A C++ simulation of a write-allocate, write-back cache controller driven by a finite state machine. The design models a realistic cache hierarchy — `CacheLine` → `Cache` → `Controller` — backed by a flat binary memory file (`ram.txt`). All addresses are binary strings of `ADDRESS_SIZE` bits (default: 16).

---

## How it works

When the controller processes an instruction, it steps through the following FSM states:

```
CPU issues instruction (addWord / addByte / subWord / subByte)
          |
          v
       [IDLE]
     picks next instruction from queue
          |
          v
   [COMPARE TAG]
          |
     HIT  |  MISS
    -------+-------
    |               |
    v               v
 serve from    [WRITE BACK]  <- only if evicted slot is dirty
  cache         flush line to ram.txt
  (mark dirty        |
  on write)          v
               [ALLOCATE]
               fetchBlock() loads new block from RAM
                    |
                    v
               retry access (guaranteed hit)
```

`evictCache()` performs a bulk write-back of every dirty line — typically called when the program ends or the cache needs to be fully flushed.

---

## Address layout

Every address is a 16-bit binary string decomposed at runtime:

```
[ tag | index | word offset | byte offset ]
```

The field widths depend on the `Cache` construction parameters (`linePower`, `wordPower`). For example, `Cache(3, 2)` gives:

| Field       | Bits | Value        |
|-------------|------|--------------|
| byte offset | 2    | 4 bytes/word |
| word offset | 2    | 4 words/line |
| index       | 3    | 8 lines      |
| tag         | 9    | remainder    |

---

## Class reference

### `CacheLine`

Represents a single cache entry. Owned via pointer inside `Cache`.

| Member      | Type                        | Access  | Description                                            |
|-------------|-----------------------------|---------|--------------------------------------------------------|
| `tag`       | `std::string`               | private | Address tag stored in the line                         |
| `valid`     | `bool`                      | private | Whether the line holds valid data                      |
| `dirty`     | `bool`                      | private | Whether the line has been written since last fetch     |
| `nWords`    | `int`                       | private | Number of words in this line                           |
| `data`      | `std::vector<std::string>`  | private | Word data as binary strings                            |
| `ptr_frame` | `Cache*`                    | private | Back-pointer to the owning cache (for metadata sizing) |

Key public methods:

```cpp
CacheLine(Cache* ptr_frame = NULL);
void setMetadata(std::string tag, bool valid = true, bool dirty = false);
void insertData(std::vector<std::string> data);
void invalidate();
std::string getWord(int wordIdx) const;
std::string getTag() const;
std::string getLine() const;
int isDirty() const;
int isValid() const;
int getTagSize() const;
std::string& operator[](int idx);
```

`Cache` and `Controller` are declared `friend` classes with direct access to private members.

---

### `Cache`

Holds a `std::vector<CacheLine*>` and exposes the cache structure to the controller.

```cpp
Cache(int linePower = 4, int wordPower = 2);
```

- `linePower` → number of lines = `2^linePower`
- `wordPower` → words per line = `2^wordPower`

| Member   | Type                       | Description                        |
|----------|----------------------------|------------------------------------|
| `line`   | `std::vector<CacheLine*>`  | Public vector of cache line pointers |
| `nLines` | `const int`                | Total number of lines              |
| `nWords` | `const int`                | Words per line                     |
| `width`  | `int`                      | Display width for `printCache()`   |

Key public methods:

```cpp
void printCache() const;
int getNWords() const;
int getNLines() const;
CacheLine* operator[](int idx) const;
```

---

### `Controller`

Takes a `Cache*` and drives the FSM. Instructions are queued and consumed by `operate()`.

```cpp
Controller(Cache* cache);
```

**FSM states (internal enum):**

```cpp
enum State { idle, compareTag, writeBack, allocate };
```

Key public methods:

```cpp
// Queue an instruction for execution
void addInstruction(Instruction i);

// Process all queued instructions through the FSM
void operate();

// Execute a single instruction and return the result
long long int exeInstruction(Instruction i);

// Write a value into the cache at addr.
// wordGranularity=true  -> write full 32-bit word
// wordGranularity=false -> write single byte at byte offset
// Returns false if addr is not currently cached (caller must fetchBlock first)
bool writeToCache(const std::string& addr, const std::string& value,
                  bool wordGranularity = true);

// Flush all dirty lines to ram.txt, then invalidate them.
// Clean and invalid lines are left untouched.
// Returns the number of lines written back.
int evictCache();
```

Private FSM helpers:

```cpp
bool fetchBlock(const std::string addr);
bool hit(const std::string& addr) const;
bool writeBackLine(int lineIdx);
int  getBlockIdx(const std::string& addr) const;
int  getByteOffset(const std::string& addr) const;
int  getWordOffset(const std::string& bits) const;
std::string getTag(const std::string& addr) const;
int  getDataFromCacheByAddr(Instruction::instructionType, const std::string&) const;
```

---

### `Instruction`

A plain data class representing a single operation passed to the controller.

```cpp
Instruction(instructionType itype, std::string x_addr, std::string y_addr);
```

| Member   | Type              | Description                              |
|----------|-------------------|------------------------------------------|
| `iType`  | `instructionType` | The operation to perform                 |
| `x_addr` | `std::string`     | First operand address (binary string)    |
| `y_addr` | `std::string`     | Second operand address (binary string)   |

Supported instruction types:

| `instructionType` | Operation                                          |
|-------------------|----------------------------------------------------|
| `addWord`         | Add two 32-bit words; result written to `x_addr`   |
| `addByte`         | Add two bytes at the given byte offsets            |
| `subWord`         | Subtract 32-bit word at `y_addr` from `x_addr`     |
| `subByte`         | Subtract byte at `y_addr` from `x_addr`            |

---

### `config.hpp`

Global memory configuration.

```cpp
constexpr int ADDRESS_SIZE = 16;               // address width in bits
constexpr long long int TOTAL_BYTES = 1 << 16; // 65536 bytes

void createRAM(); // generates ram.txt with random byte data
```

---

## Usage

```cpp
#include "config.hpp"
#include "Cache.hpp"
#include "Controller.hpp"
#include "Instruction.hpp"

int main() {
    // Generate the backing memory file
    createRAM();

    // 8 lines (2^3), 4 words per line (2^2)
    Cache* cache = new Cache(3, 2);
    Controller ctrl(cache);

    // Addresses are 16-bit binary strings
    std::string addrA = "0000000000000000"; // 0x0000
    std::string addrB = "0000000000010110"; // 0x0016

    // Queue instructions
    ctrl.addInstruction(Instruction(Instruction::addWord, addrA, addrB));
    ctrl.addInstruction(Instruction(Instruction::addByte, addrA, addrB));

    // Run the FSM — processes all queued instructions
    ctrl.operate();
    cache->printCache();

    // Write a word directly into the cache (marks line dirty)
    std::string val = "11001100110011001100110011001100"; // 0xCCCCCCCC
    ctrl.writeToCache(addrA, val, true);

    // Flush all dirty lines back to ram.txt
    ctrl.evictCache();

    delete cache;
    return 0;
}
```

---

## Write-back policy

The RAM is **never updated on a cache write**. Instead:

| Event                       | Behaviour                                                                 |
|-----------------------------|---------------------------------------------------------------------------|
| Cache hit write             | Data written to cache line; dirty bit set; `ram.txt` unchanged            |
| Cache miss, clean evict     | Evicted line discarded silently; new block loaded from `ram.txt`          |
| Cache miss, dirty evict     | `writeBackLine()` flushes dirty line to `ram.txt` first; then new block loaded |
| `evictCache()` called       | All dirty lines flushed and invalidated; clean lines left untouched       |

---

## Project structure

```
Cache-Controller-FSM/
├── config.hpp          # ADDRESS_SIZE, TOTAL_BYTES, createRAM()
├── config.cpp
├── CacheLine.hpp       # CacheLine class definition
├── CacheLine.cpp
├── Cache.hpp           # Cache class — vector<CacheLine*>
├── Cache.cpp
├── Instruction.hpp     # Instruction data class + instructionType enum
├── Instruction.cpp
├── Controller.hpp      # Controller FSM declaration
├── Controller.cpp      # FSM implementation
├── main.cpp            # Step-by-step demo (8 stages)
└── ram.txt             # Generated at runtime by createRAM()
```

---

## Build

```bash
g++ -std=c++17 -o cache_sim \
    main.cpp config.cpp CacheLine.cpp Cache.cpp Instruction.cpp Controller.cpp

./cache_sim
```

Requires C++17 or later. No external dependencies.

---

## License

MIT
