// Copyright 2024 N-GINN LLC. All rights reserved.
// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved
#pragma once

#include <EASTL/map.h>
#include <EASTL/set.h>
#include <EASTL/string.h>
#include <nau/dag_ioSys/dag_memIo.h>
//#include <nau/dagorCoreUtil/dag_base64.h>
#include <nau/dataBlock/dag_dataBlock.h>
#include <nau/math/dag_e3dColor.h>
#include <nau/math/dag_interpolator.h>
// #include <memory/dag_framemem.h>
#include <nau/math/dag_random.h>
#include <nau/math/dag_uint_noise.h>

#include "nau/math/math.h"

namespace nau
{
    inline void addOverrideParam(DataBlock& dst, const DataBlock& src, int idx, bool override, const char* rename = nullptr)
    {
        const char* name = rename ? rename : src.getParamName(idx);
        switch(src.getParamType(idx))
        {
            case DataBlock::TYPE_STRING:
                if(override)
                    dst.setStr(name, src.getStr(idx));
                else
                    dst.addStr(name, src.getStr(idx));
                break;
            case DataBlock::TYPE_INT:
                if(override)
                    dst.setInt(name, src.getInt(idx));
                else
                    dst.addInt(name, src.getInt(idx));
                break;
            case DataBlock::TYPE_REAL:
                if(override)
                    dst.setReal(name, src.getReal(idx));
                else
                    dst.addReal(name, src.getReal(idx));
                break;
            case DataBlock::TYPE_POINT2:
                if(override)
                    dst.setPoint2(name, src.getPoint2(idx));
                else
                    dst.addPoint2(name, src.getPoint2(idx));
                break;
            case DataBlock::TYPE_POINT3:
                if(override)
                    dst.setPoint3(name, src.getPoint3(idx));
                else
                    dst.addPoint3(name, src.getPoint3(idx));
                break;
            case DataBlock::TYPE_POINT4:
                if(override)
                    dst.setPoint4(name, src.getPoint4(idx));
                else
                    dst.addPoint4(name, src.getPoint4(idx));
                break;
            case DataBlock::TYPE_IPOINT2:
                if(override)
                    dst.setIPoint2(name, src.getIPoint2(idx));
                else
                    dst.addIPoint2(name, src.getIPoint2(idx));
                break;
            case DataBlock::TYPE_IPOINT3:
                if(override)
                    dst.setIPoint3(name, src.getIPoint3(idx));
                else
                    dst.addIPoint3(name, src.getIPoint3(idx));
                break;
            case DataBlock::TYPE_BOOL:
                if(override)
                    dst.setBool(name, src.getBool(idx));
                else
                    dst.addBool(name, src.getBool(idx));
                break;
            case DataBlock::TYPE_E3DCOLOR:
                if(override)
                    dst.setE3dcolor(name, src.getE3dcolor(idx));
                else
                    dst.addE3dcolor(name, src.getE3dcolor(idx));
                break;
            case DataBlock::TYPE_MATRIX:
                if(override)
                    dst.setTm(name, src.getTm(idx));
                else
                    dst.addTm(name, src.getTm(idx));
                break;
            case DataBlock::TYPE_INT64:
                if(override)
                    dst.setInt64(name, src.getInt64(idx));
                else
                    dst.addInt64(name, src.getInt64(idx));
                break;
            default:
                NAU_ASSERT(0);
                break;
        }
    }

    inline bool equalDataBlocks(const DataBlock& a, const DataBlock& b)
    {
        DynamicMemGeneralSaveCB cwr_a(nau::getDefaultAllocator() /*tmpmem*/, 0, 4 << 10), cwr_b(nau::getDefaultAllocator() /*tmpmem*/, 0, 4 << 10);

        cwr_a.seekto(0);
        cwr_a.resize(0);
        cwr_b.seekto(0);
        cwr_b.resize(0);

        a.saveToTextStream(cwr_a);
        b.saveToTextStream(cwr_b);

        if(cwr_a.size() != cwr_b.size() || memcmp(cwr_a.data(), cwr_b.data(), cwr_a.size()) != 0)
            return false;
        else
            return true;
    }

    inline void copyDataBlock(DataBlock& dst, const DataBlock& from)
    {
        for(int i = 0; i < from.blockCount(); i++)
            dst.addNewBlock(from.getBlock(i));

        for(int i = 0; i < from.paramCount(); i++)
            addOverrideParam(dst, from, i, false);
    }

    inline void overrideDataBlock(DataBlock& dst, const DataBlock& overrideBlk)
    {
        for(int i = 0; i < overrideBlk.paramCount(); ++i)
            addOverrideParam(dst, overrideBlk, i, true);

        for(int i = 0; i < overrideBlk.blockCount(); ++i)
        {
            const DataBlock* newBlk = overrideBlk.getBlock(i);
            if(!newBlk)
                continue;
            dst.removeBlock(newBlk->getBlockName());
        }

        for(int i = 0; i < overrideBlk.blockCount(); ++i)
        {
            const DataBlock* newBlk = overrideBlk.getBlock(i);
            if(!newBlk)
                continue;
            dst.addNewBlock(overrideBlk.getBlock(i));
        }
    }

    inline const DataBlock* get_data_block_or_file(const DataBlock& blk, const char* blk_name_param, const char* file_name_param, DataBlock& file_blk, bool default_dummy)
    {
        if(const char* fileName = blk.getStr(file_name_param, nullptr))
            if(file_blk.load(fileName))
                return &file_blk;
        return default_dummy ? blk.getBlockByNameEx(blk_name_param) : blk.getBlockByName(blk_name_param);
    }

#define GET_DATA_BLOCK_OR_FILE(sub_blk, blk, param, dummy) \
    DataBlock sub_blk##File(framemem_ptr());               \
    const DataBlock* sub_blk = get_data_block_or_file(blk, param, param "Blk", sub_blk##File, dummy)

#define GET_DATA_BLOCK_OR_FILE_VAR_NAME(sub_blk, blk, param_var, dummy) \
    DataBlock sub_blk##File(framemem_ptr());                            \
    const DataBlock* sub_blk =                                          \
        get_data_block_or_file(blk, param_var, String(strlen(param_var) + 4, "%sBlk", param_var).str(), sub_blk##File, dummy)

#define DECLARE_BLK_PATH_READ_FUNCTION(functionName, readFunctionName, type, defaultValue)          \
    inline type functionName(DataBlock* blk, const char* path)                                      \
    {                                                                                               \
        NAU_ASSERT(blk);                                                                            \
        if(!blk)                                                                                    \
            return defaultValue;                                                                    \
                                                                                                    \
        NAU_ASSERT(blk->isValid());                                                                 \
        NAU_ASSERT(path);                                                                           \
        if(!path)                                                                                   \
            return defaultValue;                                                                    \
                                                                                                    \
        char buf[DAGOR_MAX_PATH];                                                                   \
        strncpy(buf, path, sizeof(buf) - 1);                                                        \
        buf[sizeof(buf) - 1] = 0;                                                                   \
                                                                                                    \
        int len = static_cast<int>(strlen(buf));                                                    \
        int param = 0;                                                                              \
        for(int i = 0; i < len; i++)                                                                \
        {                                                                                           \
            if(buf[i] == '/')                                                                       \
            {                                                                                       \
                buf[i] = 0;                                                                         \
                param = i + 1;                                                                      \
            }                                                                                       \
                                                                                                    \
            NAU_ASSERT(buf[i] != '\\', "invalid path '{}' use '/' instead of '\\'", path);          \
        }                                                                                           \
                                                                                                    \
        {                                                                                           \
            int i = 0;                                                                              \
            while(i != param)                                                                       \
            {                                                                                       \
                blk = blk->getBlockByName(buf + i);                                                 \
                NAU_ASSERT_RETURN(blk, defaultValue, "blkpath '{}' not found ({})", path, buf + i); \
                while(buf[i] != 0)                                                                  \
                    i++;                                                                            \
                i++;                                                                                \
            }                                                                                       \
        }                                                                                           \
                                                                                                    \
        return blk->readFunctionName(buf + param, defaultValue);                                    \
    }

#define DECLARE_BLK_PATH_WRITE_FUNCTION(functionName, getBlockFuncName, writeFunctionName, type) \
    inline void functionName(DataBlock* blk, const char* path, type value)                       \
    {                                                                                            \
        NAU_ASSERT(blk);                                                                         \
        if(!blk)                                                                                 \
            return;                                                                              \
                                                                                                 \
        NAU_ASSERT(path);                                                                        \
        if(!path)                                                                                \
            return;                                                                              \
                                                                                                 \
        char buf[DAGOR_MAX_PATH];                                                                \
        strncpy(buf, path, sizeof(buf) - 1);                                                     \
        buf[sizeof(buf) - 1] = 0;                                                                \
                                                                                                 \
        int len = static_cast<int>(strlen(buf));                                                 \
        int param = 0;                                                                           \
        for(int i = 0; i < len; i++)                                                             \
        {                                                                                        \
            if(buf[i] == '/')                                                                    \
            {                                                                                    \
                buf[i] = 0;                                                                      \
                param = i + 1;                                                                   \
            }                                                                                    \
                                                                                                 \
            NAU_ASSERT(buf[i] != '\\', "invalid path '{}' use '/' instead of '\\'", path);       \
        }                                                                                        \
                                                                                                 \
        {                                                                                        \
            int i = 0;                                                                           \
            while(i != param)                                                                    \
            {                                                                                    \
                blk = blk->getBlockFuncName(buf + i);                                            \
                NAU_ASSERT_RETURN(blk, , "blkpath '{}' not found ({})", path, buf + i);          \
                while(buf[i] != 0)                                                               \
                    i++;                                                                         \
                i++;                                                                             \
            }                                                                                    \
        }                                                                                        \
                                                                                                 \
        blk->writeFunctionName(buf + param, value);                                              \
    }

    inline DataBlock* blk_cd(DataBlock* blk, const char* path)
    {
        NAU_ASSERT(blk);
        if(!blk)
            return nullptr;

        NAU_ASSERT(path);
        if(!path || !path[0])
            return blk;

        char buf[DAGOR_MAX_PATH];
        strncpy(buf, path, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;

        int len = static_cast<int>(strlen(buf));
        for(int i = 0; i < len; i++)
        {
            if(buf[i] == '/')
                buf[i] = 0;

            NAU_ASSERT(buf[i] != '\\', "invalid path '{}' use '/' instead of '\\'", path);
        }

        {
            int i = 0;
            while(i < len)
            {
                blk = blk->getBlockByName(buf + i);
                NAU_ASSERT(blk, "blkpath '{}' not found ({})", path, buf + i);

                if(!blk)
                    return nullptr;

                while(buf[i] != 0)
                    i++;
                i++;
            }
        }

        NAU_ASSERT(blk, "blk_cd: invalid path '{}'", path);
        return blk;
    }

    inline void blk_replace_str(DataBlock* blk, const char* from, const char* to)
    {
        NAU_ASSERT(from);
        NAU_ASSERT(to);
        NAU_ASSERT(blk);

        for(int i = 0; i < blk->paramCount(); i++)
            if(blk->getParamType(i) == DataBlock::TYPE_STRING)
                if(!strcmp(from, blk->getStr(i)))
                    blk->setStr(blk->getParamName(i), to);  // FIXME: set by param id

        for(int i = 0; i < blk->blockCount(); i++)
            blk_replace_str(blk->getBlock(i), from, to);
    }

    DECLARE_BLK_PATH_READ_FUNCTION(blk_get_bool, getBool, bool, false);
    DECLARE_BLK_PATH_READ_FUNCTION(blk_get_int, getInt, int, 0);
    DECLARE_BLK_PATH_READ_FUNCTION(blk_get_str, getStr, const char*, "");
    DECLARE_BLK_PATH_READ_FUNCTION(blk_get_float, getReal, float, 0.f);
    DECLARE_BLK_PATH_READ_FUNCTION(blk_get_tm, getTm, nau::math::mat4, nau::math::mat4::identity());

    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_set_bool, getBlockByName, setBool, bool);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_set_int, getBlockByName, setInt, int);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_set_str, getBlockByName, setStr, const char*);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_set_float, getBlockByName, setReal, float);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_set_tm, getBlockByName, setTm, nau::math::mat4);

    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_add_bool, getBlockByName, addBool, bool);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_add_int, getBlockByName, addInt, int);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_add_str, getBlockByName, addStr, const char*);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_add_float, getBlockByName, addReal, float);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_add_tm, getBlockByName, addTm, nau::math::mat4);

    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_create_path_and_set_bool, addBlock, setBool, bool);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_create_path_and_set_int, addBlock, setInt, int);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_create_path_and_set_str, addBlock, setStr, const char*);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_create_path_and_set_float, addBlock, setReal, float);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_create_path_and_set_tm, addBlock, setTm, nau::math::mat4);

    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_create_path_and_add_bool, addBlock, addBool, bool);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_create_path_and_add_int, addBlock, addInt, int);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_create_path_and_add_str, addBlock, addStr, const char*);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_create_path_and_add_float, addBlock, addReal, float);
    DECLARE_BLK_PATH_WRITE_FUNCTION(blk_create_path_and_add_tm, addBlock, addTm, nau::math::mat4);

    inline bool blk_path_exists(const DataBlock* blk, const char* path)
    {
        NAU_ASSERT(blk);
        if(!blk)
            return false;

        NAU_ASSERT(blk->isValid());
        NAU_ASSERT(path);
        if(!path)
            return false;

        char buf[DAGOR_MAX_PATH];
        strncpy(buf, path, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;

        int len = static_cast<int>(strlen(buf));
        int param = 0;
        for(int i = 0; i < len; i++)
        {
            if(buf[i] == '/')
            {
                buf[i] = 0;
                param = i + 1;
            }

            NAU_ASSERT(buf[i] != '\\', "invalid path '{}' use '/' instead of '\\'", path);
        }

        {
            int i = 0;
            while(i != param)
            {
                blk = blk->getBlockByName(buf + i);
                if(!blk)
                    return false;
                while(buf[i] != 0)
                    i++;
                i++;
            }
        }

        return blk->paramExists(buf + param);
    }

    inline const DataBlock* verifyDataBlockPtr(const DataBlock* blk)
    {
        NAU_ASSERT(blk, "verifyDataBlockPtr - failed");
        return blk;
    }

    inline DataBlock* verifyDataBlockPtr(DataBlock* blk)
    {
        NAU_ASSERT(blk, "verifyDataBlockPtr - failed");
        return blk;
    }

    inline const DataBlock* verifyDataBlockPtr(const DataBlock& blk)
    {
        NAU_ASSERT(blk.isValid(), "verifyDataBlockPtr - failed");
        return &blk;
    }

    inline DataBlock* verifyDataBlockPtr(DataBlock& blk)
    {
        NAU_ASSERT(blk.isValid(), "verifyDataBlockPtr - failed");
        return &blk;
    }

#define FOR_ALL_BLOCKS(parentBlockPtr, blockName)                                                \
    if(DataBlock* blockName##_b = verifyDataBlockPtr(parentBlockPtr))                            \
        for(int blockName##_i = 0; blockName##_i < blockName##_b->blockCount(); blockName##_i++) \
            if(DataBlock* blockName = blockName##_b->getBlock(blockName##_i))

#define FOR_ALL_BLOCKS_REV(parentBlockPtr, blockName)                                                  \
    if(DataBlock* blockName##_br = verifyDataBlockPtr(parentBlockPtr))                                 \
        for(int blockName##_i = blockName##_br->blockCount() - 1; blockName##_i >= 0; blockName##_i--) \
            if(DataBlock* blockName = blockName##_br->getBlock(blockName##_i))

#define FOR_EACH_BLOCK(parentBlockPtr, blockName, blockIdentifier)                                                    \
    if(DataBlock* blockName##_b = verifyDataBlockPtr(parentBlockPtr))                                                 \
        for(int blockName##_i = 0; blockName##_i < blockName##_b->blockCount(); blockName##_i++)                      \
            if(DataBlock* blockName = strcmp(blockName##_b->getBlock(blockName##_i)->getBlockName(), blockIdentifier) \
                                          ? nullptr                                                                   \
                                          : blockName##_b->getBlock(blockName##_i))

#define FOR_EACH_BLOCK_REV(parentBlockPtr, blockName, blockIdentifier)                                                 \
    if(DataBlock* blockName##_br = verifyDataBlockPtr(parentBlockPtr))                                                 \
        for(int blockName##_i = blockName##_br->blockCount() - 1; blockName##_i >= 0; blockName##_i--)                 \
            if(DataBlock* blockName = strcmp(blockName##_br->getBlock(blockName##_i)->getBlockName(), blockIdentifier) \
                                          ? nullptr                                                                    \
                                          : blockName##_br->getBlock(blockName##_i))

#define REMOVE_CURRENT_BLOCK(blockName) blockName##_br->removeBlock(blockName##_i)

    namespace blkutil
    {
        template <typename T>
        struct LoaderSaver
        {
            typedef T (DataBlock::*getterFoo)(const char*, T) const;
            typedef int (DataBlock::*setterFoo)(const char*, T);

            typedef T (DataBlock::*getterConstRefFoo)(const char*, const T&) const;
            typedef int (DataBlock::*setterConstRefFoo)(const char*, const T&);

            static inline void loadSaveBlk(DataBlock* blk, const char* name, T& val, const T& def_val, bool load, setterFoo setter, getterFoo getter)
            {
                bool save = !load;
                if(save)
                    (blk->*setter)(name, val);
                if(load)
                    val = (blk->*getter)(name, def_val);
            }

            static inline void loadSaveBlk(DataBlock* blk, const char* name, T& val, const T& def_val, bool load, setterConstRefFoo setter, getterConstRefFoo getter)
            {
                bool save = !load;
                if(save)
                    (blk->*setter)(name, val);
                if(load)
                    val = (blk->*getter)(name, def_val);
            }
        };

        static inline void loadSaveBlk(DataBlock* blk, const char* name, float& val, const float& def_val, bool load)
        {
            LoaderSaver<float>::loadSaveBlk(blk, name, val, def_val, load, &DataBlock::setReal, &DataBlock::getReal);
        }

        static inline void loadSaveBlk(DataBlock* blk, const char* name, int& val, const int& def_val, bool load)
        {
            LoaderSaver<int>::loadSaveBlk(blk, name, val, def_val, load, &DataBlock::setInt, &DataBlock::getInt);
        }

        static inline void loadSaveBlk(DataBlock* blk, const char* name, bool& val, const bool& def_val, bool load)
        {
            LoaderSaver<bool>::loadSaveBlk(blk, name, val, def_val, load, &DataBlock::setBool, &DataBlock::getBool);
        }

        static inline void loadSaveBlk(DataBlock* blk, const char* name, nau::math::vec3& val, const nau::math::vec3& def_val, bool load)
        {
            LoaderSaver<nau::math::vec3>::loadSaveBlk(blk, name, val, def_val, load, &DataBlock::setPoint3, &DataBlock::getPoint3);
        }

        static inline void loadSaveBlk(DataBlock* blk, const char* name, nau::math::vec4& val, const nau::math::vec4& def_val, bool load)
        {
            LoaderSaver<nau::math::vec4>::loadSaveBlk(blk, name, val, def_val, load, &DataBlock::setPoint4, &DataBlock::getPoint4);
        }

        static inline void loadSaveBlk(DataBlock* blk, const char* name, nau::math::vec2& val, const nau::math::vec2& def_val, bool load)
        {
            LoaderSaver<nau::math::vec2>::loadSaveBlk(blk, name, val, def_val, load, &DataBlock::setPoint2, &DataBlock::getPoint2);
        }

        static inline void loadSaveBlk(DataBlock* blk, const char* name, const char*& val, const char*& def_val, bool load)
        {
            LoaderSaver<const char*>::loadSaveBlk(blk, name, val, def_val, load, &DataBlock::setStr, &DataBlock::getStr);
        }

        template <typename T>
        static inline void loadSaveBlk(DataBlock* blk, const char* name, T& val, bool load)
        {
            loadSaveBlk(blk, name, val, val, load);
        }

        template <typename T>
        static inline void loadSaveBlkEnum(DataBlock* blk, const char* name, T& val, const T& def_val, bool load)
        {
            int intVal = val;
            LoaderSaver<int>::loadSaveBlk(blk, name, intVal, (int)def_val, load, &DataBlock::setInt, &DataBlock::getInt);
            val = (T)intVal;
        }

        template <typename T>
        static inline void loadSaveBlkEnum(DataBlock* blk, const char* name, T& val, bool load)
        {
            return loadSaveBlkEnum<T>(blk, name, val, val, load);
        }
    }  // namespace blkutil

#if DAGOR_DBGLEVEL > 0
    inline void debug_print_datablock(const char* name, const DataBlock* blk)
    {
        DynamicMemGeneralSaveCB cwr(framemem_ptr(), 0, 64 << 10);
        cwr.seekto(0);
        cwr.resize(0);
        blk->saveToTextStream(cwr);
        cwr.write("\0\0", 2);

        NAU_CORE_DEBUG_LF("---blk-'{}'-------------------------------------------------------V", name);
        const int splitSize = 2048;
        int cnt = (cwr.size() - 1) / splitSize;
        char* data = (char*)cwr.data();
        for(int i = 0; i < cnt; i++)
        {
            char* nextStart = data + (i + 1) * splitSize;
            char nextStartChar = *nextStart;
            *nextStart = 0;
            NAU_CORE_DEBUG("{}", data + i * splitSize);
            *nextStart = nextStartChar;
        }

        NAU_CORE_DEBUG_LF("{}", data + cnt * splitSize);

        NAU_CORE_DEBUG_LF("---blk------------------------------------------------------------^");
    }
#else
    inline void debug_print_datablock(const char*, const DataBlock*)
    {
    }
#endif

    inline void getParamFromBlk(const DataBlock* blk, int pid, float& out)
    {
        out = blk->getReal(pid);
    }

    inline void getParamFromBlk(const DataBlock* blk, int pid, int& out)
    {
        out = blk->getInt(pid);
    }

    inline void getParamFromBlk(const DataBlock* blk, int pid, const char*& out)
    {
        out = blk->getStr(pid);
    }

    inline void getParamFromBlk(const DataBlock* blk, int pid, nau::math::vec3& out)
    {
        out = blk->getPoint3(pid);
    }

    inline void getParamFromBlk(const DataBlock* blk, int pid, bool& out)
    {
        out = blk->getBool(pid);
    }

    template <typename T>
    inline bool setFromBlkIfFound(const DataBlock* blk, const char* paramName, T& param)
    {
        int pid = blk->findParam(paramName);
        if(pid < 0)
            return false;
        getParamFromBlk(blk, pid, param);
        return true;
    }

    template <typename T>
    inline void getParamFromDoubleBlk(const DataBlock* blk1, const DataBlock* blk2, const char* name, T def_val, T& out)
    {
        if(blk1 != nullptr && setFromBlkIfFound(blk1, name, out))
            ;
        else if(blk2 != nullptr && setFromBlkIfFound(blk2, name, out))
            ;
        else
            out = def_val;
    }

    struct ModRec
    {
        enum ModEffectType
        {
            EM_MUL = 0,
            EM_SET,
            EM_ADD
        };
        int nameId;
        float& ref;
        ModEffectType type;
        float modEffMult;

        ModRec(int nid, float& rf, ModEffectType typ, float mod_eff_mult = 1.f) :
            nameId(nid),
            ref(rf),
            type(typ),
            modEffMult(mod_eff_mult)
        {
        }
    };

    inline void registerMod(const DataBlock& modBlk, const char* name, ModRec::ModEffectType type, float& ref, eastl::vector<ModRec>& mods, float modEffect = 1.f)
    {
        int nid = modBlk.getNameId(name);
        if(nid < 0)
            return;
        mods.push_back(ModRec(nid, ref, type, modEffect));
    }

    inline bool setFromBlkIfFoundCounter(const DataBlock& blk, int nid, int paramNid, int paramIdx, float& val, int& counter)
    {
        if(nid == paramNid)
        {
            val = blk.getReal(paramIdx);
            counter++;
            return true;
        }
        return false;
    }

    inline bool addFromBlkIfFoundCounter(const DataBlock& blk, int nid, int paramNid, int paramIdx, float& val, int& counter, float mult = 1.f)
    {
        if(nid == paramNid)
        {
            val += blk.getReal(paramIdx) * mult;
            counter++;
            return true;
        }
        return false;
    }

    inline bool mulFromBlkIfFoundCounter(const DataBlock& blk, int nid, int paramNid, int paramIdx, float& val, int& counter, float mult = 1.f)
    {
        if(nid == paramNid)
        {
            float paramVal = blk.getReal(paramIdx);
            val *= 1.f + (paramVal - 1.f) * mult;
            counter++;
            return true;
        }
        return false;
    }

    template <typename DstIndex>
    inline void setParam(DataBlock& dst, DstIndex dstIndex, const DataBlock& src, int srcIndex)
    {
        switch(src.getParamType(srcIndex))
        {
            case DataBlock::TYPE_STRING:
                dst.setStr(dstIndex, src.getStr(srcIndex));
                break;
            case DataBlock::TYPE_INT:
                dst.setInt(dstIndex, src.getInt(srcIndex));
                break;
            case DataBlock::TYPE_REAL:
                dst.setReal(dstIndex, src.getReal(srcIndex));
                break;
            case DataBlock::TYPE_POINT2:
                dst.setPoint2(dstIndex, src.getPoint2(srcIndex));
                break;
            case DataBlock::TYPE_POINT3:
                dst.setPoint3(dstIndex, src.getPoint3(srcIndex));
                break;
            case DataBlock::TYPE_POINT4:
                dst.setPoint4(dstIndex, src.getPoint4(srcIndex));
                break;
            case DataBlock::TYPE_IPOINT2:
                dst.setIPoint2(dstIndex, src.getIPoint2(srcIndex));
                break;
            case DataBlock::TYPE_IPOINT3:
                dst.setIPoint3(dstIndex, src.getIPoint3(srcIndex));
                break;
            case DataBlock::TYPE_BOOL:
                dst.setBool(dstIndex, src.getBool(srcIndex));
                break;
            case DataBlock::TYPE_E3DCOLOR:
                dst.setE3dcolor(dstIndex, src.getE3dcolor(srcIndex));
                break;
            case DataBlock::TYPE_MATRIX:
                dst.setTm(dstIndex, src.getTm(srcIndex));
                break;
            case DataBlock::TYPE_INT64:
                dst.setInt64(dstIndex, src.getInt64(srcIndex));
                break;
            default:
                NAU_ASSERT(0);
        }
    }

    template <class T>
    inline void merge_data_block(DataBlock& dst, const DataBlock& src, T& strategy)
    {
        eastl::set<eastl::string> initialDstBlocks;
        for(int bi = 0, bn = dst.blockCount(); bi < bn; bi++)
            initialDstBlocks.emplace(dst.getBlock(bi)->getBlockName());

        for(int bi = 0, bn = src.blockCount(); bi < bn; bi++)
        {
            const DataBlock* sub_src = src.getBlock(bi);
            DataBlock* dstSubBlk = (initialDstBlocks.find(sub_src->getBlockName()) != initialDstBlocks.end())
                                       ? dst.addBlock(sub_src->getBlockName())
                                       : dst.addNewBlock(sub_src->getBlockName());
            merge_data_block<T>(*dstSubBlk, *sub_src, strategy);
        }

        strategy.process(dst, src);
    }

    namespace blkmerge
    {

        struct MergeUnsafe
        {
            void addParams(DataBlock& dst, const DataBlock& src)
            {
                for(int i = 0, pn = src.paramCount(); i < pn; i++)
                    addOverrideParam(dst, src, i, false);
            }

            void process(DataBlock& dst, const DataBlock& src)
            {
                addParams(dst, src);
            }
        };

        struct MergeSafe : public MergeUnsafe
        {
            void cleanupParamsWithWrongTypes(DataBlock& dst, const DataBlock& src)
            {
                for(int i = 0, pn = src.paramCount(); i < pn; i++)
                {
                    const char* name = src.getParamName(i);
                    int idx = dst.findParam(name);
                    if(idx != -1 && dst.getParamType(idx) != src.getParamType(i))
                        dst.removeParam(name);
                }
            }

            void process(DataBlock& dst, const DataBlock& src)
            {
                cleanupParamsWithWrongTypes(dst, src);
                addParams(dst, src);
            }
        };

        struct OverrideAll : public MergeUnsafe
        {
            void cleanupOverriddenParams(DataBlock& dst, const DataBlock& src)
            {
                for(int i = 0, pn = src.paramCount(); i < pn; i++)
                {
                    const char* name = src.getParamName(i);
                    dst.removeParam(name);
                }
            }

            void process(DataBlock& dst, const DataBlock& src)
            {
                cleanupOverriddenParams(dst, src);
                addParams(dst, src);
            }
        };

        struct MergeUnique : public OverrideAll
        {
            void process(DataBlock& dst, const DataBlock& src)
            {
                cleanupOverriddenParams(dst, src);
                for(int i = 0, pn = src.paramCount(); i < pn; i++)
                    addOverrideParam(dst, src, i, true);
            }
        };

        struct OverrideIds
        {
            const eastl::vector<int>* overrideIds;

            OverrideIds(const eastl::vector<int>* override_ids) :
                overrideIds(override_ids)
            {
            }

            void cleanupParamsByIds(DataBlock& dst, const DataBlock& src)
            {
                if(!overrideIds)
                    return;

                for(int i = 0, n = overrideIds->size(); i < n; i++)
                {
                    int nid = (*overrideIds)[i];
                    const char* name = src.getName(nid);
                    if(nullptr != name)
                        dst.removeParam(name);
                }
            }
        };

        struct OverrideIdsSafe : public OverrideIds,
                                 MergeSafe
        {
            OverrideIdsSafe(const eastl::vector<int>* override_ids) :
                OverrideIds(override_ids)
            {
            }

            void process(DataBlock& dst, const DataBlock& src)
            {
                cleanupParamsWithWrongTypes(dst, src);
                cleanupParamsByIds(dst, src);
                addParams(dst, src);
            }
        };

        struct OverrideIdsUnsafe : public OverrideIds,
                                   MergeUnsafe
        {
            OverrideIdsUnsafe(const eastl::vector<int>* override_ids) :
                OverrideIds(override_ids)
            {
            }

            void process(DataBlock& dst, const DataBlock& src)
            {
                cleanupParamsByIds(dst, src);
                addParams(dst, src);
            }
        };

        struct PreserveOrder
        {
            void process(DataBlock& dst, const DataBlock& src)
            {
                // key: nameId of param in src
                // value: DstInfo:
                // - nameId of src param in dst (for caching only)
                // - dstIdx for last occurence we updated, -1 if wasn't found
                struct DstInfo
                {
                    int nameId, idx;
                };
                eastl::map<int, DstInfo> idxmap;

                for(int srcIdx = 0, cnt = src.paramCount(); srcIdx < cnt; srcIdx++)
                {
                    // Look for next occurence of param from src in dst
                    int nameId = src.getParamNameId(srcIdx);
                    auto pair = idxmap.find(nameId);
                    int dstIdx;
                    if(pair != idxmap.end())
                    {
                        int dstNameId = pair->second.nameId;
                        dstIdx = pair->second.idx;
                        if(dstIdx >= 0)
                        {
                            dstIdx = dst.findParam(dstNameId, dstIdx);  // find next
                            idxmap[nameId] = {dstNameId, dstIdx};       // update map
                        }
                    }
                    else
                    {
                        int dstNameId = dst.getNameId(src.getParamName(srcIdx));
                        dstIdx = dst.findParam(dstNameId);     // find first
                        idxmap[nameId] = {dstNameId, dstIdx};  // update map
                    }

                    if(dstIdx >= 0)  // if found, set at found location
                        setParam(dst, dstIdx, src, srcIdx);
                    else  // otherwise, append to the end
                        addOverrideParam(dst, src, srcIdx, false);
                }

                // remove potential additional occurances of params in dst, of which previous occurances were updated
                for(const auto& pair : idxmap)
                {
                    if(pair.second.nameId >= 0 && pair.second.idx >= 0)
                        for(int i = dst.paramCount(); i > pair.second.idx; i--)
                            if(dst.getParamNameId(i) == pair.second.nameId)
                                dst.removeParam(i);
                }
            }
        };

    }  // namespace blkmerge

    // overwrite parameters and blocks from src to dst (unique for dst parameters and blocks remain as is)
    inline void merge_data_block(DataBlock& dst, const DataBlock& src, bool strict_param_type = false, bool override_all = true, const eastl::vector<int>* overrideIds = nullptr)
    {
        if(override_all)
        {
            blkmerge::OverrideAll strategy;
            merge_data_block(dst, src, strategy);
            return;
        }

        if(overrideIds)
        {
            if(strict_param_type)
            {
                blkmerge::OverrideIdsUnsafe strategy(overrideIds);
                merge_data_block(dst, src, strategy);
            }
            else
            {
                blkmerge::OverrideIdsSafe strategy(overrideIds);
                merge_data_block(dst, src, strategy);
            }
        }
        else
        {
            if(strict_param_type)
            {
                blkmerge::MergeUnsafe strategy;
                merge_data_block(dst, src, strategy);
            }
            else
            {
                blkmerge::MergeSafe strategy;
                merge_data_block(dst, src, strategy);
            }
        }
    }

    // overwrite parameters and blocks from src to dst (unique for dst parameters and blocks remain as is)
    inline void merge_data_block_and_save_order(DataBlock& dst, const DataBlock& src)
    {
        blkmerge::PreserveOrder strategy;
        merge_data_block(dst, src, strategy);
    }

    // Compare parameters from the right blk with the same parameters from the left blk.
    // Ignore all the other parameters from the left blk.

    inline bool compare_blk_to_blk_subset(const DataBlock& left, const DataBlock& right)
    {
        for(int bi = 0, bn = right.blockCount(); bi < bn; bi++)
        {
            const DataBlock* sub_right = right.getBlock(bi);
            bool res = compare_blk_to_blk_subset(*left.getBlockByNameEx(sub_right->getBlockName()), *sub_right);
            if(!res)
                return false;
        }

        for(int i = 0, pn = right.paramCount(); i < pn; i++)
        {
            const char* name = right.getParamName(i);

            if(left.findParam(name) == -1)
                return false;

            bool eq = false;
            switch(right.getParamType(i))
            {
                case DataBlock::TYPE_STRING:
                    eq = (strcmp(left.getStr(name), right.getStr(i)) == 0);
                    break;
                case DataBlock::TYPE_INT:
                    eq = (left.getInt(name) == right.getInt(i));
                    break;
                case DataBlock::TYPE_REAL:
                    eq = (left.getReal(name) == right.getReal(i));
                    break;
                case DataBlock::TYPE_POINT2:
                    eq = (left.getPoint2(name) == right.getPoint2(i));
                    break;
                case DataBlock::TYPE_POINT3:
                    eq = (left.getPoint3(name) == right.getPoint3(i));
                    break;
                case DataBlock::TYPE_POINT4:
                    eq = (left.getPoint4(name) == right.getPoint4(i));
                    break;
                case DataBlock::TYPE_IPOINT2:
                    eq = (left.getIPoint2(name) == right.getIPoint2(i));
                    break;
                case DataBlock::TYPE_IPOINT3:
                    eq = (left.getIPoint3(name) == right.getIPoint3(i));
                    break;
                case DataBlock::TYPE_BOOL:
                    eq = (left.getBool(name) == right.getBool(i));
                    break;
                case DataBlock::TYPE_E3DCOLOR:
                    eq = (left.getE3dcolor(name) == right.getE3dcolor(i));
                    break;
                case DataBlock::TYPE_MATRIX:
                    eq = (left.getTm(name) == right.getTm(i));
                    break;
                case DataBlock::TYPE_INT64:
                    eq = (left.getInt64(name) == right.getInt64(i));
                    break;
                default:
                    NAU_ASSERT(0);
            }

            if(!eq)
                return false;
        }

        return true;
    }
    /* TODO: base64 support (if needed)
    inline nau::string pack_blk_to_base64(const DataBlock& input_blk)
    {
        DynamicMemGeneralSaveCB dump(nau::getDefaultAllocator() *framemem_ptr()*;
        input_blk.saveToStream(dump);
        dag::Base64 b64Coder;
        b64Coder.encode(dump.data(), dump.size());
        return nau::string(512, b64Coder.c_str());
    }

    inline bool unpack_blk_from_base64(const char* input_base64, DataBlock& output_blk, dblk::ReadFlags flg = dblk::ReadFlags())
    {
        if(!input_base64 || !input_base64[0])
            return false;

        dag::Base64 b64decoder{input_base64};
        eastl::vector<uint8_t> inputBlkStream{*framemem_ptr()*};
        inputBlkStream.resize(b64decoder.decodeLength());
        b64decoder.decode(&inputBlkStream[0]);
        InPlaceMemLoadCB memloadCb(&inputBlkStream[0], inputBlkStream.size());
        return dblk::load_from_stream(output_blk, memloadCb, flg);
    }
    */

    static inline float lerp_point2(const nau::math::vec2& pt, float v)
    {
        return dag::lerp(pt[0], pt[1], v);
    }

    inline float get_point2lerp_or_real_noise(const DataBlock& data_blk, const char* name, float def, uint32_t seed, uint32_t at)
    {
        unsigned int noiseValAt0 = uint_noise1D(at * 2 + 0, seed), noiseValAt1 = uint_noise1D(at * 2 + 1, seed);
        const DataBlock* blk = data_blk.getBlockByName(name);
        if(blk && blk->blockCount() > 0)
        {
            float curWeight = (noiseValAt0 & 0xFFFFFF) / float(1 << 24);  // mantissa is 24 bit
            float prevWeight = 0.0f;
            float totalWeight = 0.0f;
            bool uniformWeight = true;
            for(int blockNo = 0; blockNo < blk->blockCount(); ++blockNo)
            {
                float weight = Vectormath::max(blk->getBlock(blockNo)->getReal("weight", 1.0f), 0.0f);
                uniformWeight &= blockNo == 0 || weight == prevWeight;
                prevWeight = weight;
                totalWeight += weight;
            }

            if(uniformWeight)
                blk = blk->getBlock(Vectormath::min(int(floor(curWeight * blk->blockCount())), int(blk->blockCount()) - 1));
            else
            {
                curWeight *= totalWeight;
                totalWeight = 0.0f;
                for(int blockNo = 0; blockNo < blk->blockCount(); ++blockNo)
                {
                    totalWeight += Vectormath::max(blk->getBlock(blockNo)->getReal("weight", 1.0f), 0.0f);
                    if(curWeight <= totalWeight)
                    {
                        blk = blk->getBlock(blockNo);
                        break;
                    }
                }
            }
        }

        int paramIndex = -1;
        if(blk == nullptr)
        {
            blk = &data_blk;
            paramIndex = blk->findParam(name);
        }
        else
            paramIndex = blk->findParam("value");

        if(paramIndex < 0)
            return def;

        nau::math::vec2 v12 = blk->getParamType(paramIndex) == DataBlock::TYPE_POINT2
                                  ? blk->getPoint2(paramIndex)
                                  : (blk->getParamType(paramIndex) == DataBlock::TYPE_REAL ? nau::math::vec2(blk->getReal(paramIndex), blk->getReal(paramIndex))
                                                                                           : nau::math::vec2(def, def));

        NAU_ASSERT(blk->getParamType(paramIndex) == DataBlock::TYPE_POINT2 || blk->getParamType(paramIndex) == DataBlock::TYPE_REAL,
                   "invalid type of param (or param in block) <%s> , should be real or point2", name);

        float lerpVal = (noiseValAt1 & 0xFFFFFF) / float(1 << 24);  // mantissa is 24 bit
        return dag::lerp(v12.getX(), v12.getY(), lerpVal);
    }

    inline float get_point2lerp_or_real(const DataBlock& data_blk, const char* name, float def, int& seed)
    {
        dagor_random::_rnd(seed);
        return get_point2lerp_or_real_noise(data_blk, name, def, 0x68E31DA4, seed);
    }

    template <typename T>
    struct InterpolatorConverterMult
    {
        inline float operator()(float /*value*/) const
        {
            NAU_ASSERT(0);
            return 0.0f;
        }
    };

    template <>
    struct InterpolatorConverterMult<float>
    {
        nau::math::vec2 mult;
        inline nau::math::vec2 operator()(const nau::math::vec2& value) const
        {
            return nau::math::vec2(value.getX() * mult.getX(), value.getY() * mult.getY());
        }
    };

    template <>
    struct InterpolatorConverterMult<nau::math::vec2>
    {
        nau::math::vec3 mult;
        inline nau::math::vec3 operator()(const nau::math::vec3& value) const
        {
            return nau::math::vec3(value.getX() * mult.getX(), value.getY() * mult.getY(), value.getZ() * mult.getZ());
        }
    };

    template <>
    struct InterpolatorConverterMult<nau::math::vec3>
    {
        nau::math::vec4 mult;
        inline nau::math::vec4 operator()(const nau::math::vec4& value) const
        {
            return nau::math::vec4(value.getX() * mult.getX(), value.getY() * mult.getY(), value.getZ() * mult.getZ(), value.getW() * mult.getW());
        }
    };

    template <typename T, class Interpolator, class Conv>
    struct InterpolatorLoader
    {
        inline void loadRecord(Interpolator& /*out_interpolator*/, const DataBlock& /*blk*/, int /*param_num*/) const
        {
            NAU_ASSERT(0);
        }
        inline bool loadDoubleRecords(Interpolator& /*out_interpolator*/, const DataBlock& /*blk*/, int /*param_num*/) const
        {
            NAU_ASSERT_RETURN(0, false);
        }
        inline void loadSingleRecord(Interpolator& /*out_interpolator*/, const DataBlock& /*blk*/, int /*param_num*/) const
        {
            NAU_ASSERT(0);
        }
    };

    template <class Interpolator, class Conv>
    struct InterpolatorLoader<float, Interpolator, Conv>
    {
        Conv conv;
        inline void loadRecord(Interpolator& out_interpolator, const DataBlock& blk, int param_num) const
        {
            const nau::math::vec2 record = conv(blk.getPoint2(param_num));
            out_interpolator.add(record.getX(), record.getY());
        }
        inline bool loadDoubleRecords(Interpolator& out_interpolator, const DataBlock& blk, int param_num) const
        {
            if(blk.getParamType(param_num) == DataBlock::TYPE_POINT4)
            {
                const nau::math::vec4 lines = blk.getPoint4(param_num);
                const nau::math::vec2 record0 = conv(nau::math::vec2(lines.getX(), lines.getY()));
                out_interpolator.add(record0.getX(), record0.getY());
                const nau::math::vec2 record1 = conv(nau::math::vec2(lines.getZ(), lines.getW()));
                out_interpolator.add(record1.getX(), record1.getY());
                return true;
            }
            else
                return false;
        }
        inline void loadSingleRecord(Interpolator& out_interpolator, const DataBlock& blk, int param_num) const
        {
            const float value = blk.getReal(param_num);
            const nau::math::vec2 record = conv(nau::math::vec2(0.0f, value));
            out_interpolator.add(record.getX(), record.getY());
        }
    };

    template <class Interpolator, class Conv>
    struct InterpolatorLoader<nau::math::vec2, Interpolator, Conv>
    {
        Conv conv;
        inline void loadRecord(Interpolator& out_interpolator, const DataBlock& blk, int param_num) const
        {
            const nau::math::vec3 record = conv(blk.getPoint3(param_num));
            out_interpolator.add(record.getX(), nau::math::vec2(record.getY(), record.getZ()));
        }
        inline bool loadDoubleRecords(Interpolator& /*out_interpolator*/, const DataBlock& /*blk*/, int /*param_num*/) const
        {
            return false;
        }
        inline void loadSingleRecord(Interpolator& out_interpolator, const DataBlock& blk, int param_num) const
        {
            const nau::math::vec2 value = blk.getPoint2(param_num);
            const nau::math::vec3 record = conv(nau::math::vec3(0.0f, value.getX(), value.getY()));
            out_interpolator.add(0.0f, nau::math::vec2(record.getY(), record.getZ()));
        }
    };

    template <class Interpolator, class Conv>
    struct InterpolatorLoader<nau::math::vec3, Interpolator, Conv>
    {
        Conv conv;
        inline void loadRecord(Interpolator& out_interpolator, const DataBlock& blk, int param_num) const
        {
            const nau::math::vec4 record = conv(blk.getPoint4(param_num));
            out_interpolator.add(record.getX(), nau::math::vec3(record.getY(), record.getZ(), record.getW()));
        }
        inline bool loadDoubleRecords(Interpolator& /*out_interpolator*/, const DataBlock& /*blk*/, int /*param_num*/) const
        {
            return false;
        }
        inline void loadSingleRecord(Interpolator& out_interpolator, const DataBlock& blk, int param_num) const
        {
            const nau::math::vec3 value = blk.getPoint3(param_num);
            const nau::math::vec4 record = conv(nau::math::vec4(0.0f, value.getX(), value.getY(), value.getZ()));
            out_interpolator.add(0.0f, nau::math::vec3(record.getY(), record.getZ(), record.getW()));
        }
    };

    template <typename T, class Interpolator, class Conv>
    struct InterpolatorSaver
    {
        Conv conv;
        inline void saveRecord(const typename Interpolator::TabPoint& /*point*/, DataBlock& /*blk*/, const char* /*param_name*/) const
        {
            NAU_ASSERT(0);
        }
        inline bool saveDoubleRecords(const Interpolator& /*interpolator*/, const DataBlock& /*blk*/, const char* /*param_name*/) const
        {
            NAU_ASSERT_RETURN(0, false);
        }
        inline void saveSingleRecord(const typename Interpolator::TabPoint& /*point*/, DataBlock& /*blk*/, const char* /*param_name*/) const
        {
            NAU_ASSERT(0);
        }
    };

    template <class Interpolator, class Conv>
    struct InterpolatorSaver<float, Interpolator, Conv>
    {
        Conv conv;
        inline void saveRecord(const typename Interpolator::TabPoint& point, DataBlock& blk, const char* param_name) const
        {
            blk.setPoint2(param_name, conv(nau::math::vec2(point.getX(), point.getY())));
        }
        inline bool saveDoubleRecords(const Interpolator& interpolator, DataBlock& blk, const char* param_name) const
        {
            if(interpolator.size() == 2)
            {
                const nau::math::vec2 line0 = conv(nau::math::vec2(interpolator[0].getX(), interpolator[0].getY()));
                const nau::math::vec2 line1 = conv(nau::math::vec2(interpolator[1].getX(), interpolator[1].getY()));
                blk.setPoint4(param_name, nau::math::vec4(line0.getX(), line0.getY(), line1.getX(), line1.getY()));
                return true;
            }
            else
                return false;
        }
        inline void saveSingleRecord(const typename Interpolator::TabPoint& point, DataBlock& blk, const char* param_name) const
        {
            blk.setReal(param_name, conv(nau::math::vec2(point.getX(), point.getY())).getY());
        }
    };

    template <class Interpolator, class Conv>
    struct InterpolatorSaver<nau::math::vec2, Interpolator, Conv>
    {
        Conv conv;
        inline void saveRecord(const typename Interpolator::TabPoint& point, DataBlock& blk, const char* param_name) const
        {
            blk.setPoint3(param_name, conv(nau::math::vec3(point.getX(), point.getY().getX(), point.getY().getY())));
        }
        inline bool saveDoubleRecords(const Interpolator& /*interpolator*/, const DataBlock& /*blk*/, const char* /*param_name*/) const
        {
            return false;
        }
        inline void saveSingleRecord(const typename Interpolator::TabPoint& point, DataBlock& blk, const char* param_name) const
        {
            const auto converted = conv(nau::math::vec3(point.getX(), point.getY().getX(), point.getY().getY()));
            blk.setPoint2(param_name, nau::math::vec2{converted.getY(), converted.getZ()});
        }
    };

    template <class Interpolator, class Conv>
    struct InterpolatorSaver<nau::math::vec3, Interpolator, Conv>
    {
        Conv conv;
        inline void saveRecord(const typename Interpolator::TabPoint& point, DataBlock& blk, const char* param_name) const
        {
            blk.setPoint4(param_name, conv(nau::math::vec4(point.getX(), point.getY().getX(), point.getY().getY(), point.getY().getZ())));
        }
        inline bool saveDoubleRecords(const Interpolator& /*interpolator*/, const DataBlock& /*blk*/, const char* /*param_name*/) const
        {
            return false;
        }
        inline void saveSingleRecord(const typename Interpolator::TabPoint& point, DataBlock& blk, const char* param_name) const
        {
            const nau::math::vec4 value = conv(nau::math::vec4(point.getX(), point.getY().getX(), point.getY().getY(), point.getY().getZ()));
            blk.setPoint3(param_name, nau::math::vec3(value.getY(), value.getZ(), value.getW()));
        }
    };

    template <class Interpolator, class Loader>
    inline bool load_interpolate_table_custom(Interpolator& out_interpolator, const DataBlock& blk, const char* param_name, const Loader& loader, int max_params)
    {
        const int paramNameLen = (int)strlen(param_name) + 2;
        out_interpolator.clear();
        for(int i = 0; i < max_params; ++i)
        {
            nau::string paramNameIndexed{/*TODO: Allocators framemem_ptr()*/ };
            paramNameIndexed.format("{}{}", param_name, i);
            const int paramNum = blk.findParam(reinterpret_cast<const char*>(paramNameIndexed.c_str()));
            if(paramNum >= 0)
                loader.loadRecord(out_interpolator, blk, paramNum);
        }
        if(out_interpolator.empty())
        {
            const int paramNum = blk.findParam(param_name);
            if(paramNum >= 0)
            {
                if(loader.loadDoubleRecords(out_interpolator, blk, paramNum))
                    ;
                else
                    loader.loadSingleRecord(out_interpolator, blk, paramNum);
            }
        }
        return !out_interpolator.empty();
    }

    template <class Interpolator, typename M>
    inline bool load_interpolate_table(Interpolator& out_interpolator, const DataBlock& blk, const char* param_name, const M& mult, int max_params)
    {
        InterpolatorLoader<typename Interpolator::Value, Interpolator, InterpolatorConverterMult<typename Interpolator::Value>> loader;
        loader.conv.mult = mult;
        return load_interpolate_table_custom(out_interpolator, blk, param_name, loader, max_params);
    }

    template <class Interpolator, class Saver>
    inline void save_interpolate_table_custom(const Interpolator& interpolator, DataBlock& blk, const char* param_name, const Saver& saver)
    {
        if(saver.saveDoubleRecords(interpolator, blk, param_name))
            ;
        else if(interpolator.size() > 1)
        {
            const int paramNameLen = (int)strlen(param_name) + 2;
            for(int i = 0; i < interpolator.size(); ++i)
            {
                nau::string paramNameIndexed{} /*(framemem_ptr())*/;
                paramNameIndexed.format("{}{}", param_name, i);
                saver.saveRecord(interpolator[i], blk, reinterpret_cast<const char*>(paramNameIndexed.c_str()));
            }
        }
        else if(interpolator.size() > 0)
            saver.saveSingleRecord(interpolator[0], blk, param_name);
    }

    template <class Interpolator, typename M>
    inline void save_interpolate_table(const Interpolator& interpolator, DataBlock& blk, const char* param_name, const M& mult)
    {
        InterpolatorSaver<typename Interpolator::Value, Interpolator, InterpolatorConverterMult<typename Interpolator::Value>> saver;
        saver.conv.mult = mult;
        save_interpolate_table_custom(interpolator, blk, param_name, saver);
    }

    template <class Interpolator, class Loader, class Saver>
    inline void load_save_override_interpolate_table_custom(Interpolator& in_out_interpolator, DataBlock& blk, const char* param_name, const Loader& loader, const Saver& saver, int max_params, bool load)
    {
        if(load)
            load_interpolate_table_custom(in_out_interpolator, blk, param_name, loader, max_params);
        else
            save_interpolate_table_custom(in_out_interpolator, blk, param_name, saver);
    }

    inline nau::math::vec2 inverse_conv_mult(const nau::math::vec2& mult)
    {
        return nau::math::vec2(dag::safeinv(mult.getX()), dag::safeinv(mult.getY()));
    }

    inline nau::math::vec3 inverse_conv_mult(const nau::math::vec3& mult)
    {
        return nau::math::vec3(dag::safeinv(mult.getX()), dag::safeinv(mult.getY()), dag::safeinv(mult.getZ()));
    }

    inline nau::math::vec4 inverse_conv_mult(const nau::math::vec4& mult)
    {
        return nau::math::vec4(dag::safeinv(mult.getX()), dag::safeinv(mult.getY()), dag::safeinv(mult.getZ()), dag::safeinv(mult.getW()));
    }

    template <class Interpolator, typename M>
    inline void load_save_override_interpolate_table(Interpolator& in_out_interpolator, DataBlock& blk, const char* param_name, const M& mult, int max_params, bool load)
    {
        if(load)
            load_interpolate_table(in_out_interpolator, blk, param_name, mult, max_params);
        else
            save_interpolate_table(in_out_interpolator, blk, param_name, inverse_conv_mult(mult));
    }

    struct ArgLoaderMult
    {
        float mult;
        inline float operator()(const DataBlock& blk, int param_num) const
        {
            return blk.getReal(param_num) * mult;
        }
    };

    template <class Interpolator2D, class ArgLoader1L, class Loader2L>
    inline bool load_interpolate_2d_table_custom(Interpolator2D& out_interpolator_2d, const DataBlock& blk, const char* param_name, const ArgLoader1L& arg_loader, const Loader2L& loader_2l, const nau::math::ivec2& max_params)
    {
        const int paramNameLen = (int)strlen(param_name) + 2;
        out_interpolator_2d.clear();
        for(int i = 0; i < max_params.getX(); ++i)
        {
            nau::string paramNameIndexed{/*TODO: Allocators framemem_ptr()*/ };
            paramNameIndexed.format("{}{}", param_name, i);
            const int argParamNum = blk.findParam(reinterpret_cast<const char*>(paramNameIndexed.c_str()));
            if(argParamNum >= 0)
            {
                const float arg = arg_loader(blk, argParamNum);
                if(typename Interpolator2D::Interpolator1DL2* element = out_interpolator_2d.add(arg))
                    load_interpolate_table_custom(*element, blk, reinterpret_cast<const char*>(paramNameIndexed.c_str()), loader_2l, max_params.getY());
            }
        }
        if(out_interpolator_2d.empty())
        {
            if(typename Interpolator2D::Interpolator1DL2* element = out_interpolator_2d.add(0.0f))
                load_interpolate_table_custom(*element, blk, param_name, loader_2l, max_params.getY());
        }
        return !out_interpolator_2d.empty();
    }

    struct ArgSaverMult
    {
        float mult;
        inline void operator()(float arg, DataBlock& blk, const char* param_name) const
        {
            blk.setReal(param_name, arg * mult);
        }
    };

    template <class Interpolator2D, class ArgSaver1L, class Saver2L>
    inline void save_interpolate_2d_table_custom(const Interpolator2D& interpolator_2d, DataBlock& blk, const char* param_name, const ArgSaver1L& arg_saver, const Saver2L& saver_2l)
    {
        const int paramNameLen = (int)strlen(param_name) + 2;
        if(interpolator_2d.getRoot().size() > 1)
        {
            for(int i = 0; i < interpolator_2d.getRoot().size(); ++i)
            {
                nau::string paramNameIndexed{} /*framemem_ptr()*/;
                paramNameIndexed.format("{}{}", param_name, i);
                arg_saver(interpolator_2d.getRoot()[i].getX(), blk, reinterpret_cast<const char*>(paramNameIndexed.c_str()));
                save_interpolate_table_custom(interpolator_2d.getRoot()[i].getY(), blk, reinterpret_cast<const char*>(paramNameIndexed.c_str()), saver_2l);
            }
        }
        else if(interpolator_2d.getRoot().size() > 0)
            save_interpolate_table_custom(interpolator_2d.getRoot()[0].getY(), blk, param_name, saver_2l);
    }

    template <class Interpolator2D, class ArgLoader1L, class Loader2L, class ArgSaver1L, class Saver2L>
    inline void load_save_override_interpolate_2d_table_custom(Interpolator2D& in_out_interpolator, DataBlock& blk, const char* param_name, const ArgLoader1L& arg_loader, const Loader2L& loader_2l, const ArgSaver1L& arg_saver, const Saver2L& saver_2l, const nau::math::ivec2& max_params, bool load)
    {
        if(load)
            load_interpolate_2d_table_custom(in_out_interpolator, blk, param_name, arg_loader, loader_2l, max_params);
        else
            save_interpolate_2d_table_custom(in_out_interpolator, blk, param_name, arg_saver, saver_2l);
    }

    inline const DataBlock* get_random_block(const DataBlock& parent_blk, int seed)
    {
        float weight = 0.f;
        for(int blockNo = 0; blockNo < parent_blk.blockCount(); blockNo++)
            weight += Vectormath::max(parent_blk.getBlock(blockNo)->getReal("weight", 1.f), 0.f);

        weight *= dagor_random::_frnd(seed);
        for(int blockNo = 0; blockNo < parent_blk.blockCount(); ++blockNo)
        {
            weight -= Vectormath::max(parent_blk.getBlock(blockNo)->getReal("weight", 1.f), 0.f);
            if(weight <= 0.f)
                return parent_blk.getBlock(blockNo);
        }

        return parent_blk.getBlock(0);
    }
}  // namespace nau