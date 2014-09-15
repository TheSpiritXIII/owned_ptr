/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Daniel Hrabovcak, Josh Ventura
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
**/
#ifndef OWNED_PTR__HPP_
#define OWNED_PTR__HPP_
#include <ctime>
#include <mutex>
#include <thread>
#include <cassert>
#include <deque>
#include <list>

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
	TypeName(const TypeName&); \
	void operator=(const TypeName&)


template <typename Type> class reader_ptr;

/**
 * @brief  A smart pointer whose data is read from a reader_ptr.
 *
 * This class was made specifically for applications that use dynamic external
 * libraries, where data is deleted as library's heap segment is suddenly
 * removed (the library is unloaded during run-time) and used memory is not
 * tracked or cannot be determined.
 *
 * This class is thread safe.
 */
template <typename Type> class owned_ptr
{
public:

	/**
	 * @brief  Creates an instance without a value.
	 */
	owned_ptr();

	/**
	 * @brief  Creates an instance with the given value.
	 *
	 * The given variable is owned by the owned_ptr instance. It is managed and
	 * deleted by this instance.
	 */
	explicit owned_ptr(Type *value);

	/**
	 * @brief  Deallocates associated data and invalidates all readers.
	 *
	 * If a reader is currently accessing the values, then this function will
	 * wait for the reader to be unlocked before deleting.
	 */
	~owned_ptr();

	/**
	 * @brief  Pairs the reader instance with this instance.
	 *
	 * If the given reader is already locked, then the reader will not be able
	 * to obtain its new value until it makes another call to lock().
	 */
	void get(reader_ptr<Type> &reader);

	/**
	 * @brief  Changes the value type.
	 *
	 * This function is blocked until not readers are using the value. The given
	 * variable is owned by this instance. It is managed and deleted by this
	 * instance.
	 */
	void reset(Type *value);

	/**
	 * @brief  Returns the number of readers.
	 */
	size_t count();

protected:
	friend class reader_ptr<Type>;
	void remove_(reader_ptr<Type> *child);
	void replace_(reader_ptr<Type> *reader, reader_ptr<Type> *with);
	std::mutex mutex_;
	Type *value_;
private:
	DISALLOW_COPY_AND_ASSIGN(owned_ptr<Type>);
	std::deque<reader_ptr<Type>*> children_;
	std::list<reader_ptr<Type>*> update_;
};

/**
 * @brief  A class that reads data from a referenced owned pointer.
 */
template <typename Type> class reader_ptr
{
public:

	/**
	 * @brief  Creates an instance without a referenced owner.
	 */
	reader_ptr();

	/**
	 * @brief  Creates an instance with a referenced owner.
	 */
	explicit reader_ptr(owned_ptr<Type> *owner);

	/**
	 * @brief  Creates an instance with the same owner as the given reader.
	 */
	reader_ptr(reader_ptr<Type> &other);

	/**
	 * @brief  Creates an instance by replacing the given reader.
	 */
	reader_ptr(reader_ptr<Type> &&other);

	/**
	 * @brief  Removes itself from its referenced owner.
	 */
	~reader_ptr();

	/**
	 * @brief  Locks the owner's value.
	 * @return The owner's value if the owner still exists, or nullptr if there
	 *         is no owner, or the owner has been deleted.
	 * @see    unlock();
	 *
	 * Multiple readers may lock simultaneously. If an owner's value has
	 * changed after calling this method, then this method must be called again
	 * to recieve the new value.
	 */
	Type *lock();

	/**
	 * @brief  Unlocks the owner's value.
	 */
	void unlock();

protected:
	friend class owned_ptr<Type>;
	void set_owner_(owned_ptr<Type> *owner);
	Type *value_;
	Type *locked_;
private:
	std::mutex mutex_;
	owned_ptr<Type> *parent_;
};

template <typename Type> owned_ptr<Type>::owned_ptr() : value_(nullptr) {}

template <typename Type> owned_ptr<Type>::owned_ptr(Type *value) :
	value_(value) {}

template <typename Type> owned_ptr<Type>::~owned_ptr()
{
	mutex_.lock();
	for (auto &child : children_)
	{
		child->value_ = nullptr;
		child->set_owner_(nullptr);
		while (child->locked_ != nullptr)
		{
			std::this_thread::yield();
		}
	}
	delete value_;
	mutex_.unlock();
}

template <typename Type> void owned_ptr<Type>::get(reader_ptr<Type> &reader)
{
	mutex_.lock();
	reader.value_ = value_;
	reader.set_owner_(this);
	children_.push_back(&reader);
	mutex_.unlock();
}

template <typename Type> void owned_ptr<Type>::reset(Type *value)
{
	mutex_.lock();
	for (auto &child : children_)
	{
		child->value_ = value;
		if (child->locked_ != nullptr)
		{
			update_.push_back(child);
		}
	}
	while (!update_.empty())
	{
		for (auto child = update_.begin(); child != update_.end(); ++child)
		{
			if ((*child)->locked_ == nullptr || (*child)->locked_ == value)
			{
				update_.erase(child);
			}
		}
	}
	delete value_;
	value_ = value;
	mutex_.unlock();
}

template <typename Type> size_t owned_ptr<Type>::count()
{
	std::lock_guard<std::mutex> guard(mutex_);
	(void)guard; // Remove 'unused' warnings.
	return children_.size();
}

template <typename Type> void owned_ptr<Type>::remove_(reader_ptr<Type> *child)
{
	std::lock_guard<std::mutex> guard(mutex_);
	(void)guard; // Remove 'unused' warnings.

	auto i = children_.begin();
	while (i != children_.end())
	{
		if (*i == child)
		{
			while ((*i)->locked_ == nullptr)
			{
				std::this_thread::yield();
			}
			children_.erase(i);
			return;
		}
	}
	assert(true);
}

template <typename Type> void owned_ptr<Type>::replace_(
	reader_ptr<Type> *reader, reader_ptr<Type> *with)
{
	std::lock_guard<std::mutex> guard(mutex_);
	(void)guard; // Remove 'unused' warnings.

	auto i = children_.begin();
	while (i != children_.end())
	{
		if (*i == reader)
		{
			*i = with;
			return;
		}
	}
	assert(true);
}

template <typename Type> reader_ptr<Type>::reader_ptr() : value_(nullptr),
	locked_(nullptr), parent_(nullptr) {}

template <typename Type> reader_ptr<Type>::reader_ptr(owned_ptr<Type> *owner) :
	reader_ptr<Type>()
{
	owner->get(*this);
}

template <typename Type> reader_ptr<Type>::reader_ptr(reader_ptr<Type> &other) :
	reader_ptr<Type>()
{
	other.mutex_.lock();
	if (other.parent_ != nullptr)
	{
		other.parent_->get(*this);
	}
	other.mutex_.unlock();
}

template <typename Type> reader_ptr<Type>::reader_ptr(reader_ptr<Type> &&other)
	: reader_ptr<Type>()
{
	other.mutex_.lock();
	if (other.parent_ != nullptr)
	{
		other.parent_->replace_(other, this);
	}
	other.mutex_.unlock();
}

template <typename Type> reader_ptr<Type>::~reader_ptr()
{
	mutex_.lock();
	if (parent_ != nullptr)
	{
		parent_->remove_(this);
	}
	mutex_.unlock();
}

template <typename Type> Type *reader_ptr<Type>::lock()
{
	return locked_ = value_;
}

template <typename Type> void reader_ptr<Type>::unlock()
{
	locked_ = nullptr;
}

template <typename Type> void reader_ptr<Type>::set_owner_(owned_ptr<Type> *owner)
{
	mutex_.lock();
	if (owner != nullptr && parent_ != nullptr)
	{
		parent_->remove_(this);
	}
	parent_ = owner;
	mutex_.unlock();
}

#endif // OWNED_PTR__HPP_
