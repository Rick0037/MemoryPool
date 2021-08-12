#include "PageCache.h" 

PageCache PageCache::_inst;

//从外部申请内存
Span* PageCache::AllocBigPageObj(size_t size)
{
    assert(size>MAX_BYTES);
    size=SizeClass::_Roundup(size,PAGE_SHIFT);//对齐
    size_t npage =size>>PAGE_SHIFT;
    if (npage<NPAGES)
    {
        Span* span =NewSpan(npage);
        span->_objsize=size;
        return span;
    }
    else 
    {
        void* ptr =VirtualAlloc(0,npage<<PAGE_SHIFT,
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
void PageCache::FreeBigPageObj(void *ptr,Span*span)
{
    size_t npage =span->_objsize>>PAGE_SHIFT;
    if(npage<NPAGES)
    {
        span->_objsize=0;
        ReleaseSpanToPageCache(span);
    }
    else 
    {
        _idspanmap.erase(npage);
        delete span;
        VirtualFree(ptr,0,MEM_RELEASE);
    }

}
    
//获取内部的空间以页为单位
Span*PageCache::_NewSpan(size_t n)
{
    std::unique_lock<std::mutex> lock(_mutex);
    return NewSpan(n);
}

Span* PageCache::NewSpan(size_t n)
{
    //先判断有没有
    assert(n < NPAGES);
    if(!_spanlist[n].Empty())
    {
        return _spanlist[n].PopFront();
    }
    //如果没有向其他的大小的span 进行分割
    for (size_t i=n+1;i<NPAGES;i++)
    {
        if (!_spanlist[i].Empty())
        {
            Span* span=_spanlist[i].PopFront();//取出来
            Span* movespan =new Span;//newspan
            //剩余挂载
            movespan->_pageid = span->_pageid;
            movespan->_npage = n;
            
            span->_pageid=span->_pageid+n;
            span->_npage=span->_npage-n;
            
            //循环进行标记映射
            for(size_t i=0;i<n;i++)
            {
                _idspanmap[movespan->_pageid + i]=movespan;
            }
            
            //剩余插入
            _spanlist[n].PushFront(span);
            return movespan;
        }
    }

    Span* sysspan = new Span; 
    //没有只能向系统申请128页大内存
    #ifdef _WIN32
	    void* ptr = VirtualAlloc(0, (NPAGES - 1)*(1 << PAGE_SHIFT), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    #else
    //  brk
    #endif
    
    sysspan->_pageid=(PageID)ptr>>PAGE_SHIFT;
    sysspan->_npage=NPAGES-1;//128

    for (size_t i=0;i<sysspan->_npage;i++)
    {
        _idspanmap[sysspan->_pageid+i]=sysspan;
    }

    _spanlist[sysspan->_npage].PushFront(sysspan);

    return _NewSpan(n);
    //系统的重新分配
}


//获取从对象到span的映射
Span* PageCache::MapObjectToSpan(void*obj)
{
    PageID id = (PageID)obj>>PAGE_SHIFT;
    auto it =_idspanmap.find(id);
    if(it!=_idspanmap.end())
    {
        return it->second;
    }
    else 
    {
        assert(false);
        return nullptr;
    }

}

//释放Span空间回调PageCache，合并
void PageCache::ReleaseSpanToPageCache(Span*cur)
{
    // 必须上全局锁,可能多个线程一起从ThreadCache中归还数据
	std::unique_lock<std::mutex> lock(_mutex);
    
    //直接归换给操作系统
    if (cur->_npage>=NPAGES)
    {
        void*ptr=(void*)(cur->_pageid<<PAGE_SHIFT);
        _idspanmap.erase(cur->_pageid);
        VirtualFree(ptr,0,MEM_RELEASE);
        delete cur;
        return; 
    }
    
    //向前合并
    while(1)
    {  
        //寻找id
        PageID curid =cur->_pageid;
        //查找前一个id
        PageID previd =curid -1;
        auto it = _idspanmap.find(previd);
        //没有找到
        if (it == _idspanmap.end())
        {
            break;
        }
        //找到不是空闲的不能合并
        if ((it!=_idspanmap.end())&&(it->second->_usecount!=0))
        {
            break;
        }
        //向前合并
        Span* prev = it->second;
        
        //大于128页不能合并
        if (cur->_npage+prev->_npage>NPAGES-1) break;

        //先把prev从表中移除
        _spanlist[prev->_npage].Erase(prev);
        //合并到一个prev中
        prev->_npage+=cur->_npage;
        //重新进行映射
        for (PageID i=0; i<cur->_npage;i++)
        {
            _idspanmap[cur->_pageid+i]=prev;
        }
        //删除
        delete cur;
        cur=prev;
    }
    
    //向后合并
    while(1)
    {
        //寻找下一个的id
        PageID nextid=cur->_pageid+cur->_npage; 
        
        auto it = _idspanmap.find(nextid);

        if (it==_idspanmap.end()) break;

        if (it->second->_usecount!=0) break;

        Span* next = it->second;
        //超过128页
        if (cur->_npage+next->_npage>=NPAGES-1) break;

        _spanlist[next->_npage].Erase(next);

        cur->_npage+=next->_npage;

        for (PageID i=0;i<next->_npage;i++)
        {
            _idspanmap[next->_pageid+i]=cur;
        }

        delete next;

    }
    
    _spanlist[cur->_npage].PushFront(cur);

}