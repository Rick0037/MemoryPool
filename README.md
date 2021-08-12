# 高并发内存池 MemoryPool

### 1.项目概述
在日常生活中操作系统的内存申请往往需要频繁的进行用户态和内核态之间的来回切换  
多线程的内存申请还涉及到锁竞争的问题，内存申请和锁竞争也被称为高性能服务器的四大杀手之二  
进行频繁的内存申请和释放过程会制造很多的内存碎片，（内部碎片和外部碎片）  
为了解决整个问题需要用到经典的池化技术配合，解除锁对线程的限制，由此影响设计了高并发内存池

### 2.层次结构
![}2(1DHQ22}00IWE 3A0}3KU](https://user-images.githubusercontent.com/86883267/129190000-eb040b81-a471-490c-a881-4e0ea539a470.png)   
主要由三部分组成：  
1. ThreadCache线程缓存 负责每个单一线程的申请操作，在小于64k的情况下，从这里申请内存并不需要加锁
每个线程独立享有一个cache，这也是高并发线程池高效的地方。  

2. CentralCache中心缓存 中心缓存是所有线程所共享，线程缓存按需从中心缓存获取对象，并周期性回收，避免一个线程占用了太多的内存达到内存分配在多个线程中均衡的目的。中心缓存是存在竞争的，所以从这里取内存对象是需要加锁。  

3. PageCache页缓存 页缓存是中心缓存上面的缓存，以页为单位存储及分配，中心缓存没有内存对象(Span)时，从页缓存分配出一定数量的page，切割并分配给中心缓存。页缓存还会定期回收内存并合并成相邻的页，组成更大的页，缓解内存碎片的问题。  

### 3.ThreadCache
线程类实现了Thread loacl storage 将每一个线程内存类的指针都保存在线程本地，  
这样多个线程在进行申请时可以直接申请属于自己的内存区域不用加锁，实现了高并发。  
![SD7BF0QT`)N7S5LIW8{E8VY](https://user-images.githubusercontent.com/86883267/129192495-0f5484ef-3cf6-451d-8bc2-add247ef5aba.png)  

从ThreadCache申请内存：
当内存申请size<=64时2在ThreadCache中申请，计算size在自由链表中的位置，如果自由链表中有内存对象直接从FreeList[i]中pop一下对象，时间复杂度时O(1)，且没有锁竞争；当Freelist[i]中没有对象时则批量从CentralCache中获取一定数量的对象，插入到自由链表并返回一个对象（剩余的n-1个对象插入到自由链表并返回一 个对象）

向ThreadCache释放内存：
当释放内存小于64k时将内存释放回Thread Cache， 计算size在自由链表中的位置，将对象Push到FreeList[i]； 当链表的长度过长， 也就是超过一次最大限制数目时则回收一部分内存对象到Central Cache。
```
class ThreadCache 
{
private:
    //自由链表
    Freelist _freelist[NLIST];
public:
    //对外
    void* Allocate(size_t size);
    //回收
    void Deallocate(void* ptr,size_t size);
    //从中心取数据
    void* FetchFromCentralCache(size_t index, size_t size);  
    //链表过长
    void ListTooLong(Freelist* list,size_t size); 
};
//线程独有的静态成员
//_thread static ThreadCache* tlslist =nullptr;
static __declspec(thread) ThreadCache* tlslist =nullptr;
```
### 4.CentralCache  
![0{){BCI)F@L5K$R938RJSC](https://user-images.githubusercontent.com/86883267/129194140-fabd83a7-d0ab-4678-8ada-0b6f063647d8.png)  

Central Cache申请内存：
当线程缓存中没有内存时，会向中心缓存申请一定数量的内存对象，中心指定位置的Spanlist中挂着span，从span中取出并给线程缓存，这个过程是需要加锁的，可能会存在多个线程同时取对象，会导致线程安全的问题。
当中心缓存中没有span对象时，就向页缓存申请一个span对象，span以页为单位，我们将span对象切成需要的内存大小并链接起来，挂到中心缓存中。
中心缓存的中的每一个span都有一个use_count，分配一个对象给线程缓存，就++use_count，当这个span的使用计数为0，说明这个span所有的内存对象都是空闲的，然后将它交给页缓存合并成更大的页，减少内存碎片。

Central Cache释放内存：
当线程缓存过长或者线程销毁，则会将内存释放回中心缓存中，每释放一个内存对象，检查该内存所在的span使用计数是否为空，释放回来一个时--use_count。
当use_count减到0时则表示所有对象都回到了span，则将span释放回页缓存，在页缓存中会对前后相邻的空闲页进行合并。
```
class CentralCache {
public :
    //获取实例的单例模式
    static CentralCache *Getinstence()
    {
        return &_inst;
    }
    //从页中获取一个span
    Span * GetOneSpan(SpanList& spanlist,size_t byte_size);
    //从中心缓存获取一定的对象回复给thread cache
    size_t FetchRangeObj(void* &start,void*& end,size_t n,size_t byte_size);
    //释放
    void ReleaseListToSpans(void* start,size_t size);

private :
    SpanList _spanlist[NLIST];
    CentralCache(){}//无法构造
    CentralCache(CentralCache&) =delete;
    static CentralCache _inst;//单例模式的静态变量 
};
```

### 5.PageCache
```
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
    PageCache(){}    //单例模式
    PageCache(const PageCache&)=delete ;
    static PageCache _inst; 
};
```

### 6.实现细节

### 7.测试结果展示
![H}IBI8JD)7A~J YAC3`$ 12](https://user-images.githubusercontent.com/86883267/129199391-bb21fa17-da53-4028-a850-75d453d09a55.png)  
4线程或8线程在每一次申请8b，16b，32b，等多种情况下进行测试  
每组进行100000次在windows下申请内存与malloc进行申请时间对比 平均申请时间之后malloc的三分之一！


### 8.目录文件结构
ThreadCache.cpp	    线程缓存  
ThreadCache.h     
CentralCache.cpp    中心缓存  
CentralCache.h	    
PageCache.cpp	      页缓存  
PageCache.h         
Common.h	        基础组件  
ConcurrentAlloc.h   对外接口  
benchmark      标准的测试接口
unittest         单元测试接口
