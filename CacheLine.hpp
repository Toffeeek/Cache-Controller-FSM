#pragma once
#include <iostream>
#include <string>
#include <vector>



class Cache;

class CacheLine
{
    std::string tag;
    bool valid;
    bool dirty;
    int nWords;
    std::vector<std::string> data;
    int getMetadataSize() const;
    void setDirty();
    int getSize() const;
    
    void setNWords();
    Cache *ptr_frame;
    void setWidth();
    
    friend class Cache;
    friend class Controller;
public:
    void invalidate();
    CacheLine(Cache* ptr_frame=NULL);
    // CacheLine(const CacheLine& line);
    void setMetadata(std::string tag, bool valid=true, bool dirty=false);
    void insertData(std::vector<std::string> data);
    int getTagSize() const;
    int isDirty() const;
    int isValid() const;
    std::string getWord(int wordIdx) const;
    std::string getTag() const;
    std::string getLine() const;


    std::string& operator [] (int idx);

};
