#pragma once

#include "Common.h"

class ThreadCache 
{
private:
    //自由链表
    Freelist _freelist[NLISTS];

public:
    //对外
    void Allocate(size_t size);
    
    //回收
    void Deallocate(void* ptr,size_t size);

    //从中心取数据
    void FetchFromCentralCache(size_t index, size_t size);
    
    //链表过长
    void ListTooLong(Freelist* list,size_t size); 

};

//线程独有的静态成员
__thread static ThreadCache* tlslist =nullptr;