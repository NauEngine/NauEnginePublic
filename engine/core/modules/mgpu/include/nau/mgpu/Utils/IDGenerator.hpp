#pragma once
#include <set>

class IDGenerator
{
    inline static std::set<unsigned long long> collisionMap;

public:
    static unsigned long long Generate()
    {
        static unsigned long long seq = 0;

        std::set<unsigned long long>::iterator it;
        do
        {
            seq = (++seq) & 0x7FFFFFFFF;

            it = collisionMap.find(seq);
        }
        while (it != collisionMap.end());

        collisionMap.insert(seq);

        return seq;
    }

    static void AddLoadedID(const unsigned long long existID)
    {
        collisionMap.insert(existID);
    }

    static void FreeID(const unsigned long long id)
    {
        collisionMap.erase(id);
    }
};
