#pragma once

// boost::asio 예제에 있는 것입니다.
// http://stackoverflow.com/questions/2893200/does-boostasio-makes-excessive-small-heap-allocations-or-am-i-wrong

#include <boost/aligned_storage.hpp>
#include <boost/noncopyable.hpp>

#ifdef WIN32
	#pragma warning( disable : 4512 )
#endif


class HandlerAllocator : private boost::noncopyable
{
public:
	HandlerAllocator()
		: in_use_(false)
	{
	}

	void* allocate(std::size_t size)
	{
		if (!in_use_ && size < storage_.size)
		{
			in_use_ = true;
			return storage_.address();
		}
		else
		{
			return ::operator new(size);
		}
	}

	void deallocate(void* pointer)
	{
		if (pointer == storage_.address())
		{
			in_use_ = false;
		}
		else
		{
			::operator delete(pointer);
		}
	}

private:
	boost::aligned_storage<16384> storage_;
	bool in_use_;
};

template <typename Handler>
class CustomAllocHandler
{
public:
	CustomAllocHandler(HandlerAllocator& a, Handler h)
		: allocator_(a)
		, handler_(h)
	{
	}

	template <typename Arg1>
	void operator()(Arg1 arg1)
	{
		handler_(arg1);
	}

	template <typename Arg1, typename Arg2>
	void operator()(Arg1 arg1, Arg2 arg2)
	{
		handler_(arg1, arg2);
	}

	template <typename Arg1, typename Arg2, typename Arg3>
	void operator()(Arg1 arg1, Arg2 arg2, Arg3 arg3)
	{
		handler_(arg1, arg2, arg3);
	}

	friend void* asio_handler_allocate(std::size_t size, CustomAllocHandler<Handler>* this_handler)
	{
		return this_handler->allocator_.allocate(size);
	}

	friend void asio_handler_deallocate(void* pointer, std::size_t /*size*/, CustomAllocHandler<Handler>* this_handler)
	{
		this_handler->allocator_.deallocate(pointer);
	}

private:
	HandlerAllocator& allocator_;
	Handler handler_;
};

template <typename Handler>
inline CustomAllocHandler<Handler> make_custom_alloc_handler(HandlerAllocator& a, Handler h)
{
	return CustomAllocHandler<Handler>(a, h);
}

