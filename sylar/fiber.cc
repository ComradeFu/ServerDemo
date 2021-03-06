#include "fiber.h"
#include "config.h"
#include "macro.h"
#include "log.h"
#include "scheduler.h"
#include <atomic>

namespace sylar
{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static std::atomic<uint64_t> s_fiber_id {0};
static std::atomic<uint64_t> s_fiber_count {0};

//用普通指针是为了方便判断，正在跑的
static thread_local Fiber* t_fiber = nullptr;
//线程的主协程，main fiber
static thread_local Fiber::ptr t_threadFiber = nullptr;

//32够用了
static ConfigVar<uint32_t>::ptr g_fiber_stack_size =
    Config::Lookup<uint32_t>("fiber.stack_size", 1024 * 1024, "fiber stack size");

//fiber栈内存申请类
class MallocStackAllocator
{
public:
    static void* Alloc(size_t size)
    {
        return malloc(size);
    }

    //留出 size，以后可以考虑切换用 mmap 的方式
    static void Dealloc(void* vp, size_t size)
    {
        return free(vp);
    }
};

//别名，typedef 也行。方便切换。默认构造函数只有主协程！
using StackAllocator = MallocStackAllocator;

uint64_t Fiber::GetFiberId()
{
    if(t_fiber)
    {
        return t_fiber->getId();
    }

    return 0;
}

Fiber::Fiber()
{
    //主协程
    m_state = EXEC;

    //这个函数执行，肯定是 main
    SetThis(this);

    //这里注释是因为，用了swapcontext，会自动保存好。现在get初始化没什么意义，会给要call 的fiber的ctx给覆盖
    // if(getcontext(&m_ctx))
    // {
    //     SYLAR_ASSERT2(false, "getcontext");
    // }

    ++s_fiber_count;

    // SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber main";
}

//真正的才开始开辟协程
Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool use_caller)
    :m_id(++s_fiber_id)
    ,m_cb(cb)
{
    SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber id = " << m_id;
    //这个是真正的协程创建
    ++s_fiber_count;
    //不给就以配置为准
    m_stacksize = stacksize ? stacksize : g_fiber_stack_size->getValue();

    m_stack = StackAllocator::Alloc(m_stacksize);
    if(getcontext(&m_ctx))
    {
        SYLAR_ASSERT2(false, "getcontext");
    }

    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    if(!use_caller)
    {
        makecontext(&m_ctx, &Fiber::MainFunc, 0);
    }
    else
    {
        makecontext(&m_ctx, &Fiber::CallerMainFunc, 0);
    }
}

Fiber::~Fiber()
{
    --s_fiber_count;
    if(m_stack)
    {
        //已经分配好了栈的话，就必然是这两种状态，才能够被析构
        SYLAR_ASSERT2(m_state == TERM
                || m_state == EXCEPT
                || m_state == INIT, m_state);

        StackAllocator::Dealloc(m_stack, m_stacksize);
    }
    else
    {
        //主协程
        SYLAR_ASSERT(!m_cb);
        //主协程是不会停的
        SYLAR_ASSERT(m_state == EXEC);

        //肯定是等于自己的把
        Fiber* cur = t_fiber;
        if(cur == this)
        {
            SetThis(nullptr);
        }
    }

    SYLAR_LOG_DEBUG(g_logger) << "fiber destory " << m_id;
}

void Fiber::reset(std::function<void()> cb)
{
    //主协程是不会有栈的
    SYLAR_ASSERT(m_stack);

    SYLAR_ASSERT(m_state == TERM
                    || m_state == EXCEPT
                    || m_state == INIT);

    m_cb = cb;
    //重新初始化
    if(getcontext(&m_ctx))
    {
        SYLAR_ASSERT2(false, "getcontext in reset");
    }

    //等于重新初始化了吧
    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    makecontext(&m_ctx, &Fiber::MainFunc, 0);
    m_state = INIT;
}

//当前协程置换成目标线程协程
//其实也可以协程跟 swapout 一样的判断分支走掉
//但如果多了一个判断，反复切换是有判断消耗的（不是吧这个也在意？）
void Fiber::call()
{
    SetThis(this);

    m_state = EXEC;
    if(swapcontext(&(*t_threadFiber).m_ctx, &m_ctx))
    {
        SYLAR_ASSERT2(false, "swapcontext in call");
    }
}

void Fiber::swapIn()
{
    SetThis(this);
    SYLAR_ASSERT(m_state != EXEC);

    m_state = EXEC;

    //切换上下文，从主协程手里拿过来
    //注释是因为 scheduler 不能这样了，要切换回 scheduler 的主协程
    //但这样修改就没办法单独使用 Fiber 模块了，额
    // if(swapcontext(&t_threadFiber->m_ctx, &m_ctx))
    if(swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx))
    {
        SYLAR_ASSERT2(false, "swapcontext in swapIn");
    }
}

void Fiber::back()
{
    SYLAR_LOG_INFO(g_logger) << "back !";

    SetThis(t_threadFiber.get());
    //只能跟真正的主协程进行切换
    if(swapcontext(&m_ctx, &t_threadFiber->m_ctx ))
    {
        SYLAR_ASSERT2(false, "swapcontext in swapOut2");
    }
}

//之前是有专门判断的，现在又改回去了
void Fiber::swapOut()
{
    SetThis(Scheduler::GetMainFiber());

    //这个同上面，跟调度器协程切换
    if(swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx))
    {
        SYLAR_ASSERT2(false, "swapcontext in swapOut1");
    }
}

void Fiber::SetThis(Fiber* f)
{
    t_fiber = f;
}

Fiber::ptr Fiber::GetThis()
{
    if(t_fiber)
        return t_fiber->shared_from_this();
    
    //还没有的话，就是主协程了，第一次来
    //创建好它
    Fiber::ptr main_fiber(new Fiber);
    //应该是相等的，构建的时候 set 了
    SYLAR_ASSERT(t_fiber == main_fiber.get());
    t_threadFiber = main_fiber;

    return t_fiber->shared_from_this();
}

void Fiber::YieldToReady()
{
    //静态方法，对当前正在执行的 fiber 进行操作
    Fiber::ptr cur = GetThis();
    cur->m_state = READY;
    cur->swapOut();
}

void Fiber::YieldToHold()
{
    Fiber::ptr cur = GetThis();
    cur->m_state = HOLD;
    cur->swapOut();
}

uint64_t Fiber::TotalFibers()
{
    return s_fiber_count;
}

//执行函数
void Fiber::MainFunc()
{
    //注意下面这种用法，会导致 cur 引用 + 1
    //而这个方法调用swapout 的时候，这个 ptr 还在栈上，所以没有被释放掉
    Fiber::ptr cur = GetThis();
    SYLAR_ASSERT(cur);

    try
    {
        cur->m_cb();
        cur->m_cb = nullptr; //null ptr 是因为用的是 functional，可能会有智能指针在内部
        cur->m_state = TERM;
    }
    catch(const std::exception& e)
    {
        cur->m_state = EXCEPT;
        SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " << e.what()
            << " fiber_id=" << cur->getId()
            << std::endl
            << sylar::BacktraceToString();
    }
    catch(...)
    {
        //防止打印不出来
        cur->m_state = EXCEPT;
        SYLAR_LOG_ERROR(g_logger) << "Fiber Except";
    }
    
    //解决的就是 + 1 的问题。
    //cur 引起释放之后，它的栈也会被释放掉。继而这里的 cur 等等栈变量，会被自动销毁。
    auto raw_ptr = cur.get();
    cur.reset();

    // SYLAR_LOG_INFO(g_logger) << "Fiber main func end." << raw_ptr->getId();

    //回到主协程，虽然也可以只限定 uc_link 来解决。但既然已经封装了，就不用这个了
    raw_ptr->swapOut();

    //永远也不会到这里
    SYLAR_ASSERT2(false, "never reach");
}

//如果use caller 的情况下，不用这个为入口
//就会使得 stop 之后，不能回去继续处理caller后续的逻辑。。蛋疼。。
void Fiber::CallerMainFunc()
{
    Fiber::ptr cur = GetThis();
    SYLAR_ASSERT(cur);

    try
    {
        cur->m_cb();
        cur->m_cb = nullptr; //null ptr 是因为用的是 functional，可能会有智能指针在内部
        cur->m_state = TERM;
    }
    catch(const std::exception& e)
    {
        cur->m_state = EXCEPT;
        SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " << e.what()
            << " fiber_id=" << cur->getId()
            << std::endl
            << sylar::BacktraceToString();
    }
    catch(...)
    {
        //防止打印不出来
        cur->m_state = EXCEPT;
        SYLAR_LOG_ERROR(g_logger) << "Fiber Except";
    }
    
    //解决的就是 + 1 的问题。
    //cur 引起释放之后，它的栈也会被释放掉。继而这里的 cur 等等栈变量，会被自动销毁。
    auto raw_ptr = cur.get();
    cur.reset();

    SYLAR_LOG_DEBUG(g_logger) << "Fiber main func end root_fiber." << raw_ptr->getId();

    //回到主协程，虽然也可以只限定 uc_link 来解决。但既然已经封装了，就不用这个了
    //唯一的区别。。
    raw_ptr->back();

    //永远也不会到这里
    SYLAR_ASSERT2(false, "never reach");
}

}
