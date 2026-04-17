#pragma once
#include "Cache.hpp"
#include "config.hpp"
#include "Instruction.hpp"
#include <queue>

class Controller
{
    Cache *cache;
    
    enum State
    {
        idle, compareTag, writeBack, allocate 
    };
    State state; 
    std::queue<Instruction> pendingInstructions;
    bool fetchBlock(const std::string addr);
    std::string getTag(const std::string& addr) const;
    bool hit(const std::string& addr) const;
    int getBlockIdx(const std::string& addr) const;
    int getByteOffset(const std::string& addr) const;
    int getWordOffset(const std::string& bits) const;
    int getDataFromCacheByAddr(Instruction::instructionType iType, const std::string& addr) const;
    bool writeBackLine(int lineIdx);

public:
    Controller(Cache* cache);
    void operate();
    long long int exeInstruction(Instruction i);
    void addInstruction(Instruction i);

    // Write a value into the cache at the given address and mark the line dirty.
    // granularity: true  -> write a full word (32 bits)
    //              false -> write a single byte (8 bits, selected by byte-offset)
    // value is a binary string of the appropriate width (32 or 8 chars).
    // Returns true on success, false if the address is not currently in cache
    // (caller should fetchBlock first, then retry).
    bool writeToCache(const std::string& addr, const std::string& value, bool wordGranularity = true);

    // Write back every dirty line to main memory (ram.txt), then invalidate
    // those lines.  Lines that are clean are left untouched.
    // Returns the number of lines that were written back.
    int evictCache();
};
unsigned int binToInt(const std::string& bits);
