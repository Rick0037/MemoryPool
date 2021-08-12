#pragma once

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"

//内存提取接口
static inline void* ConcurrentAlloc(size_t size)
{
    if (size>MAX_BYTES)
    {
        //从页中提取
        Span* span =PageCache::GetInstence()->AllocBigPageObj(size);
        void* ptr =(void*)(span->_pageid>>PAGE_SHIFT);
        return ptr ;
    }
    else
    {   
        //线程中提取
        if (tlslist ==nullptr)
        {
            tlslist = new  ThreadCache;

        }
        return tlslist->Allocate(size);//线程缓存
    }

}


//内存释放接口
static inline void ConcurrentFree(void* ptr)
{
    Span* span =PageCache::GetInstence()->MapObjectToSpan(ptr);
    size_t size =span->_objsize;
    if (size>MAX_BYTES)
    {
        PageCache::GetInstence()->FreeBigPageObj(ptr,span);
    }
    else
    {
        tlslist->Deallocate(ptr,size);
    } 


}