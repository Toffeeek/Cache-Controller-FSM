#include "Cache.hpp"
#include "Controller.hpp"
#include "Instruction.hpp"
#include "config.hpp"



int main()
{
    createRAM();   // for now, keep it enabled while testing

    Cache* cache = new Cache(3, 2);   // 2^3 = 8 lines, 2^2 = 4 words per line
    Controller c(cache);

    c.addInstruction(Instruction(Instruction::addWord, "0000000000000000", "0000000000010000"));
    c.addInstruction(Instruction(Instruction::addByte, "0000000000000001", "0000000000010001"));

    c.operate();

    c.evictCache();   // flush dirty lines before program ends

    delete cache;
    return 0;
}
