#include "CentralCache.h"
#include "PageCache.h"

//单例模式静态变量

CentralCache CentralCache::_inst;

Span* CentralCache::GetOneSpan(SpanList& spanlist,size_t byte_size)
{
    Span* span =spanlist.Begin();
    while (span!=spanlist.End())
    {
        if (span->_list!=nullptr)
        {
            return span;
        }
        else 
        {
            span=span->next;
        }
    }
    //可以的话直接返回了

    //从page中找到一个
    //未完成

}

size_t CentralCache::FetchRangeObj(void* &start,void*& end,size_t n,size_t byte_size)
{
    //获取一个span得list批量内存对象
    size_t index=SizeClass::Index(byte_size);
    SpanList& spanlist=_spanlist[index];

    //spanlist.Lock();
    std::unique_lock<std::mutex>lock(spanlist,_mutex);//同一个list得mutex

    //得到span进行start及end得分发
    Span * span =GetOneSpan(spanlist,byte_size);
    
    //在这个需要拿到多少个free小款

    size_t batchsize;
    void * prev=nullptr;
    void *cur =span->_list;
    for (int i=0;i<n;i++)
    {
        prev=cur;
        cur=NEXT_OBJ(cur);
        ++batchsize;
        if (cur==nullptr) break;
    }
    //不怕有id有页号呢
    start=span->_list;
    end=prev;
    span->_list=cur;
    span->_usecount+=batchsize;

    //空的最后
    //非空的在前面
    if (span->_list==nullptr)
    {
        spanlist.Erase(span);
        spanlist.PushBack(span);
    }


    //spanlist.unLock();
    return batchsize;
}

void CentralCache::ReleaseListToSpans(void* start,size_t size)
{

}