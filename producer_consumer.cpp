#include <pthread.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

using namespace std;

// 华为面试题目：生产者/消费者
/* 
1.有两个生产者，一个消费者
生产与消费者之间的数据格式为：
struct data {
 pthread_t thread_id;
 size_t    product_id;
 string    product_name;
 vector<size_t> product_vendor_ids;
};
要求： 1）随机生成一些数据，由生产者产生，消费者得到后打印即可。
      2）product_id不可重复
      3）每个生产者可最多生成50条数据
      4）生产者与消费者之间的队列最多可同时容纳2条记录
      5）程序不会产生使任何生产者，消费者过度饥饿
      6）使用thread sanitizer选项编译，以便测试多线程竞争
*/

// 生产的产品信息
typedef struct data {
 pthread_t thread_id;
 size_t    product_id;
 string    product_name;
 vector<size_t> product_vendor_ids;
} Data;

// 全局produce数量
atomic<size_t> g_product_cnt(0);
atomic<size_t> g_product_target(0);

// 全局produce_id， 原子自增，保证每个product_id不重复
atomic<size_t> g_product_id(0);

// 缓冲队列
queue<Data> g_mq;

// 缓冲队列的锁
mutex g_mq_mutex;

// 生产者/消费者互相通知的条件变量
condition_variable g_not_empty_cv;
condition_variable g_not_full_cv;

// 生产者
class Producer {
public:
    Producer(pthread_t thread_id) : tid(thread_id) {};

    void produce() {
        {
            unique_lock<mutex> lock(g_mq_mutex);
            // 队列内已有数据不足2个，生产者可以生产产品
            g_not_full_cv.wait(lock, [=] {return g_mq.size() < 2;}); 
            g_product_id++;
            Data data = {.thread_id=this->tid, 
                    .product_id=g_product_id, 
                    .product_name=to_string(g_product_id)};  
            g_mq.push(data);
        } // 通知前，lock自动析构解锁
        g_not_empty_cv.notify_all();
    };

public:
    pthread_t tid;
};

// 消费者
class Consumer {
public:
    Consumer(pthread_t thread_id): tid(thread_id){};
    void consume() {
        if (g_product_cnt < g_product_target)
        {
            unique_lock<mutex> lock(g_mq_mutex);
            // 队列不空，可消费
            g_not_empty_cv.wait(lock, [=] {return !g_mq.empty();});
            Data data = g_mq.front();
            g_mq.pop();
            printf("Producer %ld produce data %ld made by consumer %ld\n", 
                    this->tid, data.product_id, data.thread_id);
        } // 通知前，lock自动析构解锁
        g_not_full_cv.notify_all();
    };

public:
    pthread_t tid;
};

void Produce() {
    Producer p(pthread_self());
    int count = 0;
    while(count < 50) {
        p.produce();
        count++;
        g_product_cnt++;
    }
    printf("g_product_cnt %d\n", g_product_cnt.load());
}

void Consume() {
    Consumer c(pthread_self());
    while(g_product_cnt < g_product_target) {
        c.consume();
    }
}

int main() {

    vector<thread> threads;
    // 5个生产者
    for (int i = 0; i < 5; i++) {
        threads.push_back(thread(&Produce));
        g_product_target += 50;
    }
    printf("g_product_target %d\n", g_product_target.load());

    // 3个消费者
    for (int j = 0; j < 3; j++) {
        threads.push_back(thread(&Consume));
    }

    for (auto& t : threads) {
        t.join();
    }

    return 0;
}
