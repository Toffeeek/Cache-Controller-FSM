#include "Controller.hpp"
#include "Cache.hpp"
#include "config.hpp"
#include "Instruction.hpp"
#include "fstream"
#include <sstream>
#include <map>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <cmath>
using namespace std;




Controller::Controller(Cache* cache) : state(idle)
{
    this->cache = cache;
    cout << "Tag Size:" << cache->line[0]->getTagSize() << endl;
}
void Controller::operate()
{
    while(!pendingInstructions.empty())
    {
        if(state == idle)
        {
            Instruction i = pendingInstructions.front();
            pendingInstructions.pop();
            long long int res = exeInstruction(i);
            cout << "res: " << res << endl;
        }
    
    }
}
unsigned int binToInt(const std::string& bits)
{
    if (bits.empty())
        throw std::invalid_argument("bit string is empty");

    unsigned int value = 0;

    for (char c : bits)
    {
        if (c != '0' && c != '1')
            throw std::invalid_argument("bit string must contain only 0 and 1");

        value = (value << 1) | (c - '0');
    }

    return value;
}
string Controller::getTag(const string& addr) const
{
    string tag = "";
    int tagSize = ADDRESS_SIZE - log2(cache->nLines) - log2(cache->nWords) - 2;
    for(int i = 0; i < tagSize; i++)
    {
        tag += addr[i];
    }
    return tag;
}
int Controller::getBlockIdx(const std::string& addr) const
{
    int m = log2(cache->nWords);
    int tagSize = cache->line[0]->getTagSize();
    string bin_idx = "";
    
    for(int i = tagSize; i < addr.size() - m - 2; i++)
    {
        bin_idx += addr[i];
    }
    return binToInt(bin_idx);

}
bool Controller::hit(const std::string& addr) const
{
    long long int idx = getBlockIdx(addr);

    if(!cache->line[idx]->isValid())
        return false;

    string addr_tag = getTag(addr);
    string cache_tag = cache->line[idx]->getTag();

    return addr_tag == cache_tag;
}
int Controller::getDataFromCacheByAddr(Instruction::instructionType iType, const string& addr) const
{
    int x = -1;
    switch(iType)
    {
        case Instruction::addWord:
        {
            int idx = getBlockIdx(addr);
            int wordIdx = getWordOffset(addr);
            string x_bin = cache->line[idx]->getWord(wordIdx);
            x = binToInt(x_bin);
            cout << "x_bin: " << x_bin << " x: " << x << endl;
            break;
        }
        case Instruction::addByte:
        {
            int idx = getBlockIdx(addr);
            int wordIdx = getWordOffset(addr);
            int byteOffset = getByteOffset(addr);
            string word = cache->line[idx]->getWord(wordIdx);
            string x_bin = word.substr(8 * byteOffset, 8);
            x = binToInt(x_bin);
            cout << "x_bin: " << x_bin << " x: " << x << endl;
            break;
        }
        case Instruction::subWord:
        {
            int idx = getBlockIdx(addr);
            int wordIdx = getWordOffset(addr);
            x = binToInt(cache->line[idx]->getWord(wordIdx));
            break;
        }
        case Instruction::subByte:
        {
            int idx = getBlockIdx(addr);
            int wordIdx = getWordOffset(addr);
            int byteOffset = getByteOffset(addr);
            string word = cache->line[idx]->getWord(wordIdx);
            x = binToInt(word.substr(8 * byteOffset, 8));
            break;
        }
        default : cout << "Unknown type\n";
    }
    return x;
}
long long int Controller::exeInstruction(Instruction ins)
{
    cout << "X addr: " << ins.x_addr << endl;
    cout << "Y addr: " << ins.y_addr << endl;


    if(!hit(ins.x_addr))
    {
        cout << "cache miss tryna fetch X\n";
        fetchBlock(ins.x_addr);
        cout << "done fetching X\n";
    }
    else
    {
        cout << "cache hit on X\n";
    }

    int x = getDataFromCacheByAddr(ins.iType, ins.x_addr);

    if(!hit(ins.y_addr))
    {
        cout << "cache miss tryna fetch Y\n";
        fetchBlock(ins.y_addr);
        cout << "done fetching Y\n";
    }
    else
    {
        cout << "cache hit on Y\n";
    }

    int y = getDataFromCacheByAddr(ins.iType, ins.y_addr);

    cache->printCache();

    
    switch(ins.iType)
    {
        case Instruction::addWord:
        {
            return x + y;
        }
        case Instruction::addByte:
        {
            return x + y;
        }
        case Instruction::subWord:
        {
            return x - y;
        }
        case Instruction::subByte:
        {
            return x - y;
        }
        default : cout << "unknown instruction type\n";
    }
    return 0;
}
bool Controller::fetchBlock(const std::string addr)
{
    long long int memBlockIdx = binToInt(addr) / 4;

    ifstream file("ram.txt", ios::in);

    if(file.is_open() && !file.fail())
    {
        cout << "ram opened\n";
        stringstream ss;
        for(int i = 0; i < memBlockIdx * 4; i++)
        {
            string buffer;
            getline(file, buffer);
        }
        cout << "ram traversed\n";

        vector<string> words(cache->nWords);
        for(int i = 0; i < cache->nWords; i++)
        {
            string buffer;
            for(int j = 0; j < 4; j++)
            {
                getline(file, buffer, ' ');
                // cout << "buffer discarded\n";
                getline(file, buffer);
                // cout << "byte taken\n";
                words[i] += buffer;
                // cout << "byte added\n";
            }
        }

        int idx = getBlockIdx(addr);
        (*this->cache)[idx]->insertData(words);
        (*this->cache)[idx]->setMetadata(getTag(addr), true, false);
        cout << "block written at idx " << idx << endl;

        return true;
    }
    else
    {
        cout << "error opening the ram file\n";
        return false;
    }
}
int Controller::getByteOffset(const std::string& addr) const
{
    int m = log2(cache->nWords);
    string bo = addr.substr(ADDRESS_SIZE - 2, 2);
    return binToInt(bo);
}
int Controller::getWordOffset(const std::string& addr) const
{
    int m = log2(cache->nWords);
    string wo = addr.substr(ADDRESS_SIZE - 2 - m, m);
    return binToInt(wo);
}
void Controller::addInstruction(Instruction i)
{
    pendingInstructions.push(i);
}



// ─────────────────────────────────────────────────────────────────────────────
// writeToCache
//
// Writes `value` into the cache line that holds `addr` and marks the line
// dirty.  Does NOT touch main memory — that is deferred until eviction.
//
// Parameters:
//   addr             – full ADDRESS_SIZE-bit binary address string
//   value            – binary string to write; must be 32 chars for a word
//                      write or 8 chars for a byte write
//   wordGranularity  – true  : overwrite the entire word selected by the
//                               word-offset bits of `addr`
//                      false : overwrite only the single byte selected by
//                               both the word-offset and byte-offset bits
//
// Returns true on success.
// Returns false if the addressed block is not currently resident in cache
// (call fetchBlock(addr) first, then retry).
// ─────────────────────────────────────────────────────────────────────────────
bool Controller::writeToCache(const string& addr, const string& value, bool wordGranularity)
{
    // The addressed line must already be in cache.
    if (!hit(addr))
    {
        cout << "writeToCache: cache miss at " << addr
             << " — fetch the block first\n";
        return false;
    }

    int lineIdx = getBlockIdx(addr);
    int wordIdx = getWordOffset(addr);

    CacheLine* line = (*cache)[lineIdx];

    if (wordGranularity)
    {
        // ── Word write ────────────────────────────────────────────────────
        if (value.size() != 32)
        {
            cout << "writeToCache: word value must be 32 bits, got "
                 << value.size() << "\n";
            return false;
        }
        (*line)[wordIdx] = value;
        cout << "writeToCache: wrote word to line " << lineIdx
             << " word " << wordIdx << "\n";
    }
    else
    {
        // ── Byte write ────────────────────────────────────────────────────
        if (value.size() != 8)
        {
            cout << "writeToCache: byte value must be 8 bits, got "
                 << value.size() << "\n";
            return false;
        }

        int byteOffset = getByteOffset(addr);    // 0–3 within the word
        string word    = line->getWord(wordIdx); // current 32-bit word

        // Splice the new byte into the correct position.
        word.replace(8 * byteOffset, 8, value);
        (*line)[wordIdx] = word;

        cout << "writeToCache: wrote byte to line " << lineIdx
             << " word " << wordIdx
             << " byte " << byteOffset << "\n";
    }

    // Mark the line dirty so eviction knows it needs a write-back.
    line->setDirty();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// writeBackLine  (private helper)
//
// Writes every word of cache line `lineIdx` back to the corresponding byte
// rows in ram.txt, then invalidates the line.
//
// ram.txt row format (one row per byte):  "xAAAA BBBBBBBB\n"
// where AAAA is the 4-digit hex address and BBBBBBBB is the 8-bit value.
//
// Strategy: stream through the whole file, replacing rows whose addresses
// fall inside this cache line's range, then atomically rename.
// ─────────────────────────────────────────────────────────────────────────────
bool Controller::writeBackLine(int lineIdx)
{
    CacheLine* line = (*cache)[lineIdx];

    int nLines   = cache->getNLines();
    int nWords   = cache->getNWords();
    int idxBits  = (int)log2(nLines);
    int wordBits = (int)log2(nWords);
    int byteBits = 2; // always 2 (byte offset within a 4-byte word)

    // Build the full binary address of byte 0 of this cache line:
    //   base = tag ++ lineIndex (idxBits wide) ++ all-zero offsets
    string tag = line->getTag();

    string idxStr = "";
    for (int b = idxBits - 1; b >= 0; --b)
        idxStr += ((lineIdx >> b) & 1) ? '1' : '0';

    string zeroOffset(wordBits + byteBits, '0');
    string baseAddrBin = tag + idxStr + zeroOffset;
    long long int baseAddr = (long long int)binToInt(baseAddrBin);

    // Build a patch map: byte_address → 8-bit binary string
    // Decompose each 32-bit word into 4 big-endian bytes.
    map<long long int, string> patchMap;
    for (int w = 0; w < nWords; ++w)
    {
        string word = line->getWord(w); // 32-bit binary string
        long long int wordBase = baseAddr + (long long int)w * 4;
        for (int b = 0; b < 4; ++b)
            patchMap[wordBase + b] = word.substr(8 * b, 8);
    }

    const string ramPath = "ram.txt";
    const string tmpPath = "ram_tmp.txt";

    ifstream fin(ramPath);
    ofstream fout(tmpPath);

    if (!fin.is_open() || !fout.is_open())
    {
        cout << "writeBackLine: could not open ram.txt\n";
        return false;
    }

    string row;
    long long int rowAddr = 0;
    while (getline(fin, row))
    {
        auto it = patchMap.find(rowAddr);
        if (it != patchMap.end())
        {
            // Row format: "xADDR BBBBBBBB"  — replace just the data field.
            size_t spacePos = row.find(' ');
            if (spacePos != string::npos)
                row = row.substr(0, spacePos + 1) + it->second;
            cout << "writeBackLine: patching byte " << rowAddr
                 << " -> " << it->second << "\n";
        }
        fout << row << "\n";
        ++rowAddr;
    }

    fin.close();
    fout.close();

    if (remove(ramPath.c_str()) != 0 || rename(tmpPath.c_str(), ramPath.c_str()) != 0)
    {
        cout << "writeBackLine: file rename failed\n";
        return false;
    }

    line->invalidate();
    cout << "writeBackLine: line " << lineIdx << " written back and invalidated\n";
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// evictCache
//
// Scans all cache lines.  Every line that is valid AND dirty is flushed to
// ram.txt via writeBackLine() and then invalidated.  Clean lines are left
// untouched.
//
// Returns the number of dirty lines that were written back.
// ─────────────────────────────────────────────────────────────────────────────
int Controller::evictCache()
{
    int nLines     = cache->getNLines();
    int writtenBack = 0;

    cout << "evictCache: scanning " << nLines << " lines...\n";

    for (int i = 0; i < nLines; ++i)
    {
        CacheLine* line = (*cache)[i];

        if (line->isValid() && line->isDirty())
        {
            cout << "evictCache: dirty line " << i
                 << " (tag=" << line->getTag() << ") — writing back\n";

            if (writeBackLine(i))
                ++writtenBack;
            else
                cout << "evictCache: write-back of line " << i << " FAILED\n";
        }
        else
        {
            cout << "evictCache: line " << i
                 << (line->isValid() ? " — clean, skipping" : " — invalid, skipping")
                 << "\n";
        }
    }

    cout << "evictCache: done — " << writtenBack << " line(s) written back\n";
    return writtenBack;
}
