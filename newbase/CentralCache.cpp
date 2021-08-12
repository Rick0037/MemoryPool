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
            span=span->_next;
        }
    }
    //可以的话直接返回了

    //从page中找到一个
    Span* newspan =PageCache::GetInstence()->NewSpan(SizeClass::NumMovePage(byte_size));
    //进行page的切割
    char* cur =(char*)(newspan->_pageid<<PAGE_SHIFT);
    char* end =cur+(newspan->_npage<<PAGE_SHIFT);
    newspan->_list =cur;
    newspan->_objsize=byte_size;
    while(cur + byte_size < end)
    {
        char* next =cur +byte_size;//指定大小向后移动
        NEXT_OBJ(cur)=next;//内存中写上地址
        cur =next;//cur指针向后移动
    }

    NEXT_OBJ(cur)=nullptr;//最后完成等于nullptr
    spanlist.PushFront(newspan);//newspan入队列
    return newspan;
}

//将span插入到freelist
size_t CentralCache::FetchRangeObj(void* &start,void*& end,size_t n,size_t byte_size)
{
    //获取一个span得list批量内存对象
    size_t index=SizeClass::Index(byte_size);
    SpanList& spanlist=_spanlist[index];

    //spanlist.Lock();
    std::unique_lock<std::mutex>lock(spanlist._mutex);//同一个list得mutex

    //得到span进行start及end得分发
    Span * span =GetOneSpan(spanlist,byte_size);
    
    //在这个需要拿到多少个free小款
    size_t batchsize;
    void * prev=nullptr;
    void * cur =span->_list;
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

//将整个freelist 都释放掉了
void CentralCache::ReleaseListToSpans(void* start,size_t size)
{
    size_t index =SizeClass::Index(size);
    SpanList& spanlist= _spanlist[index];
    //
    //将锁放在循环外面
	// CentralCache:对当前桶进行加锁(桶锁)，减小锁的粒度
	// PageCache:必须对整个SpanList全局加锁
	// 因为可能存在多个线程同时去系统申请内存的情况
	//spanlist.Lock();
    std::unique_lock<std::mutex>lock(spanlist._mutex);
    
    //属于是一个一个释放
    while(start)
    {
        //取出后一个位置
        void* next=NEXT_OBJ(start);
        
        //spanlist.Lock();
        //找到所在的页面map
        Span* span =PageCache::GetInstence()->MapObjectToSpan(start);
        //接上
        NEXT_OBJ(start)=span->_list;
        //——list 接上
        span->_list=start;
        if (--span->_usecount==0)
        {
            spanlist.Erase(span);
            PageCache::GetInstence()->ReleaseSpanToPageCache(span);
        }

        //spanlist.Lock();
        start=next;
    }
    //spanlist.Lock();

}