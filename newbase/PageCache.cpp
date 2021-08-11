#include "PageCache.h" 

    //从外部申请内存
    Span* PageCache::AllocBigPageObj(size_t size)
    {
        assert(size>MAX_BYTES);
        size=SizeClass::_Roundup(size,PAGE_SHIFT);//对齐
        size_t npage =size>>PAGE_SHIFT;
        if (npages<NPAGES)
        {
            Span* span =NewSpan(npage);
            span->_objsize=size;
            return span;
        }
        else 
        {
            void* ptr =VirtualAlloc(0,npage<<PAGE_SHIFT.
            MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
            
            //申请失败
            if (ptr==nullptr) throw std::bad_alloc();
            Span* span =new Span;
            
            span->_npage =npage ;
            span->_pageid =(PageID)ptr >>PAGE_SHIFT;
            span->_objsize=npage<<PAGE_SHIFT;

            _idspanmap[span->_pageid]=span;
            return span;
        }
    }

    //从内部释放内存
    void PageCache::FreeBigPage(void *ptr,Span*span)
    {

    }
    
    //获取内部的空间以页为单位
    Span*PageCache::_NewSpan(size_t n)
    {

    }
    Span* PageCache::NewSpan(size_t n)
    {

    }
    //获取从对象到span的映射
    Span* PageCache::MapObjectToSpan(void*obj)
    {

    }
    //释放Span空间回调PageCache，合并
    void PageCache::ReleaseSpanTOpageCache(Span*span)
    {

    }