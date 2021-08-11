#include "ThreadCache.h"

void ThreadCache::Allocate(size_t size)
{
    size_t index =SizeClass::Index(size); 
    Freelist* freelist =&_freelist[index];
    if (!freelist->empty())
    {
        return freelist.Pop();
    }
    // 自由链表为空的要去中心缓存中拿取内存对象，一次取多个防止多次去取而加锁带来的开销 
	// 均衡策略:每次中心堆分配给ThreadCache对象的个数是个慢启动策略
	//          随着取的次数增加而内存对象个数增加,防止一次给其他线程分配太多，而另一些线程申请
	//          内存对象的时候必须去PageCache去取，带来效率问题
	//在对应位置处链表为空的话 从中心缓存处获取
    else 
    {
        return FetchFromCentralCache(index,SizeClass::Roundup(size));//申请不出了
    }
    //挂出来然后进行分配返回一个

}


void ThreadCache::Deallocate(void* ptr, size_t size)
{
    size_t index =SizeClass::Index(size);
    Freelist* freelsit =_freelist[index];
    freelist->Push(ptr);
    if (freelist->Size()>=freelist->MaxSize())
    {
        ListTooLong(freelist,size());//如果过大了需要进行回收操作
    } 

}

 //从中心缓存中摘下来链表来进行
 //
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
    //前面做完了
    Freelist* freelist =_freelist[index];
    size_t maxsize =freelist->MaxSize();//最大数值
    size_t numtomove =min(SizeClass::NumMoveSize(size),maxsize);//nummovesize(max/size)

    void *start = nullptr;
    void *end = nullptr;

    //size_t batchsize =CentralCache::Getinstaence()->//大小和几个
    size_t batchsize = CentralCache::Getinstence()->FetchRangeObj(start, end, numtomove, size);
    //未完成FetchRangeObj
    if(batchsize>1)
    {
        freelist->PushRange(NEXT_OBJ(start),end,batchsize-1);//charufreelist
    }
    if (batchsize>=freelist->MaxSize())
    {
        freelsit->SetMaxSize(maxsize+1)
    }
    //算是返回了值了
    return start;


}

void ThreadCache::ListTooLong(Freelist* list, size_t size)
{
    //全回收了？
    void* start = list->PopRange();
    CentralCache::Getinstaence()->ReleaseListToSpans(start,size);
}