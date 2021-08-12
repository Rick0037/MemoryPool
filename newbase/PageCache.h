#pragma once 

#include "Common.h"

class PageCache 
{
public:
    //单例模式
    static PageCache* GetInstence ()
    {
        return &_inst;
    }
    //从外部申请内存
    Span* AllocBigPageObj(size_t size);
    //从内部释放内存
    void FreeBigPageObj(void *ptr,Span*span);
    
    //获取内部的空间以页为单位
    Span*_NewSpan(size_t n);
    Span* NewSpan(size_t n);
    //获取从对象到span的映射
    Span* MapObjectToSpan(void*obj);
    //释放Span空间回调PageCache，合并
    void ReleaseSpanToPageCache(Span*span);
    
private:
    SpanList _spanlist[NLIST];
    std::unordered_map<PageID,Span*> _idspanmap;
    
    std::mutex _mutex;
    //单例模式
    PageCache(){}
    PageCache(const PageCache&)=delete ;
    static PageCache _inst; 
};