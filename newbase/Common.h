#pragma once

#include <iostream>
#include <thread>
#include <mutex>
#include <map>
#include <unordered_map>
#include <vector>
#include <stdlib.h>
#include <algorithm>
#include <assert.h>

using std::cout;
using std::endl;

const size_t MAX_BYTES=64*1024;
const size_t NLIST =16+56+56+56;
const size_t PAGE_SHIFT =12;
const size_t NPAGES=129;//超过128页了

inline static void* & NEXT_OBJ(void* obj)//保存下一个对象头四个或者头八个字节
{
    return *((void**)obj);// 先强转为void**,然后解引用就是一个void*成为指针
}

//设置一个公共的FreeList对象链表，每个对象中含有各个接口，到时候直接使用接口进行操作
//让一个类来管理自由链表

class Freelist
{
private:
    void * _list =nullptr;
    size_t _size=0;
    size_t _maxsize=1;

public:
    //压入free链表
    void Push(void* obj)
    {
        NEXT_OBJ(obj)=_list;//强制转换赋值
        _list=obj;
        ++_size;
    }
    void PushRange(void*start,void* end,size_t n)
    {
        NEXT_OBJ(end)=_list;
        _list=start;
        _size+=n;
    }

    void* Pop()
    {
        void* obj =_list;
        --_size;
        _list=NEXT_OBJ(obj);
        return obj;
    }
    
    //直接把对象整个都重置出去
    void* PopRange()
    {
        void * obj=_list
        _size=0;
        _list=nullptr;
        return _obj;
    }
    bool Empty()
    {
        return _size==0;
        //return _list=nullptr;
    }
    size_t Size()
    {
        return _size;
    }
    size_t MaxSize()
    {
        return _maxsize;
    }
    void SetMaxSize(size_t maxsize)
    {
        _maxsize=maxsize;
    }
};



//专门用来计算大小位置的类 先写计算大小的类
class SizeClass
{
public:
    //静态内联 对内存取整
    inline static size_t _Index(size_t size, size_t alignnum)
    {
        size_t alignnum = 1 <<alignnum;
        return ((size+alignnum-1)>>alignnum)-1;

    }
    
    //先加再向上就是向上取整了
    inline static size_t _Roundup(size_t size , size_t alignnum)
    {
        // 比如size是15 < 128,对齐数align是8，那么要进行向上取整，
		// ((15 + 7) / 8) * 8就可以了
		// 这个式子就是将(align - 1)加上去，这样的话就可以进一个对齐数
		// 然后再将加上去的二进制的低三位设置为0，也就是向上取整了
		// 15 + 7 = 22 : 10110 (16 + 4 + 2)
		// 7 : 111 ~7 : 000
		// 22 & ~7 : 10000 (16)就达到了向上取整的效果
        size_t alignnum =1<<align;
        return (size +alignnum -1)&~(alignnum -1);

    }
    // 对齐大小计算，向上取整


    //内存对齐尽可能的减少浪费
    // [1,128]				8byte对齐 128/8 freelist[0,16)
	// [129,1024]			16byte对齐 1024/16 freelist[16,72)
	// [1025,8*1024]		128byte对齐 7*1024/16 freelist[72,128)
	// [8*1024+1,64*1024]	1024byte对齐 56 freelist[128,184)

public:

    //获取在这个大小的FreeList数组的第几个位置位置
    inline static size_t Index(size_t size)
    {
        //编译时断言
        assert(size<=MAX_BYTES);
        
        static int group_array [4] = { 16, 56, 56, 56 };
        if (size<=128)
        {
            return _Index(size,3);
        }
        else if (size<=1024)
        {
            return _Index(size-128,4)+group_array[0];
        }
        else if (size <=8*1024)
        {
            return _Index(size-1024,7)+group_array[0]+group_array[1];
        }
        else 
        {
            return _Index(size-8*1024,10)+group_array[0]+group_array[1]+group_array[2];
        }
    }
        
    // 对齐大小计算，向上取整
    static inline size_t Roundup(size_t bytes)
    {
        assert(bytes<=MAX_BYTES);

        if (bytes<=128)
        {
            return _Roundup(bytes,3);
        }
        else if (bytes<=1024)
        {
            return _Roundup(bytes,4);
        }
        else if (bytes<=8192)
        {
            return _Roundup(bytes,7);
        }
        else 
        {
            return _Roundup(bytes,10);
        }
    }

    //动态计算从中心缓存分配多少个内存对象到ThreadCache中
    static size_t NumMoveSize(size_t size)
    {
        if (size==0) return 0;
        int num =(int)(MAX_BYTES/size)
        if (num<2) return 2;
        else if (num>521) return 512;
        return num;

    }

    //
    static size_t NumMovePage(size_t size)
    {
        size_t num =NumMoveSize(size)
        size_t npage =num*size;
        npage >>=PAGE_SHIFT;
        if (npage==0) napage =1 ;
        
        return npage;

    }
    
};

#ifdef _WIN32
	typedef size_t PageID;
#else
	typedef long long PageID;
#endif //_WIN32

struct Span
{
    //页号 
    //页数
    PageID _pageid=0;
    size_t _npage=0;
    
    //前指针和尾指针
    Span* _prev=nullptr;
    Span* _next=nullptr;
    //页面的list
    void* _list=nullptr;
    
    //目标的大小
    size_t _objsize=0;
    //使用的数量
    size_t _usecount =0;
};

class SpanList
{
public:
    //构造函数 
    SpanList()
    {
        _head= new Span;
        _head->_prev=_head;
        _head->_next=_head;
    }

    //析构函数
    ~SpanList()
    {
        Span*cur =_head->next;
        while(cur!=_head)
        {
            Span*q=cur;
            cur =cur->next;
            delete q;
        }
        delete _head;//需要head做循环条件 所以得最后释放_head
        _head=nullptr;
    }
    
    //迭代器第一个指针
    Span* Begin()
    {
        return _head->next;
    }
    
    //循环队列最后一个指针
    Span* End()
    {
        return _head;
    }
    
    //判断是否为空
    bool Empty()
    {
        return _head==(_head->next);
    }

    //插入函数在
    void Insert(Span*pos,Span*newspan)
    {
        //保存前一个结点
        Span*prev=pos->_prev;
        //新结点复制
        newspan->_prev=prev;
        newspan->next=pos;
        //老节点
        prev->_next=newspan;
        pos->_prev=newspan;

    }
    //修改
    void Erase (Span* pos)
    {
        Span* prev=pos->_prev;
        Span*next=pos->_next;
        //delete pos;没有delete
        
        prev->_next=next;
        next->_prev=prev;

    }
    //头插 尾插 头删 尾删
    void PushBack(Span* newspan)
    {
        Insert(End(),newspan);

    }

    void PushFront(Span* newspan)
    {
        Insert(Begin(),newspan);
    }
    Span* PopBack()
    {
        Span* back=_head->_prev;
        Erase(End());
        return back;
    }
    Span* PopFornt()
    {
        Span* front=_head->_next;
        Erase(Begin());
        return front;
    }
    //lock unlock
    void Lock()
    {
        _mutex.lock();
    }

    void UnLock()
    {
        _mutex.unlock();
    }


    //防止复制构造函数和复制运算符
    SpanList(const SpanList&) =delete;
    SpanList& operator=(const SpanList)=delete; 
private:
    //多线程插入删除需要锁变量
    Span* _head;
    std::mutex _mutex;

};