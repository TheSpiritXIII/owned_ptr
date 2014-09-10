/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Daniel Hrabovcak
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
#include <cassert>
#include <vector>
#include <mutex>

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
	TypeName(const TypeName&); \
	void operator=(const TypeName&)


template <typename Type> class child_ptr;

/**
 * @brief  A smart pointer that owns children.
 *
 * This class was made specifically for applications that use run-time removable
 * external libraries, where data might need to be deleted when the library's
 * heap segment is removed (the library is unloaded) and used memory is not
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
	 * The given variable is owned by the owned_ptr instance. It is be managed
	 * and deleted by this instance.
	 */
	explicit owned_ptr(Type *value);

	/**
	 * @brief  Deallocates associated data and invalidates all children.
	 *
	 * If a child is currently accessing the values, then this function will
	 * wait for the child to unlock its mutex before deleting.
	 */
	~owned_ptr();

	/**
	 * @brief  Pairs the child instance with this instance.
	 *
	 * A child can only reference another parent if it is not locked. If a child
	 * is not locked, then it will reference this instance and is added into a
	 * list, with the insert O(n).
	 */
	void get(child_ptr<Type> &child);

	/**
	 * @brief  Changes the value type.
	 *
	 * The previous value is deleted. The new value variable is owned by this
	 * instance. It is be managed and deleted by this instance.
	 */
	void set(Type *value);

protected:
	friend class child_ptr<Type>;
	void remove_(child_ptr<Type> *child);
	std::mutex mutex_;
	Type *value_;
private:
	DISALLOW_COPY_AND_ASSIGN(owned_ptr<Type>);
	std::vector<child_ptr<Type>*> children_;
};

/**
 * @brief  A child pointer that references an owned pointer.
 *
 * This class is thread safe.
 */
template <typename Type> class child_ptr
{
public:

	/**
	 * @brief  Creates an instance without a referenced owner.
	 */
	child_ptr();

	/**
	 * @brief  Creates an instance with a referenced owner.
	 */
	child_ptr(owned_ptr<Type> *owner);

	/**
	 * @brief  Removes itself from its referenced owner.
	 */
	~child_ptr();

	/**
	 * @brief  Locks the owner instance.
	 * @return The owner instance's value if the owner still exists, or nullptr
	 *         if the owner does not exist.
	 * @see    unlock();
	 *
	 * While a child is locked, the owner instance is locked as well, preventing
	 * other children from accessing the data.
	 */
	Type *lock();

	/**
	 * @brief  Locks the owner instance with the indicated timeout duration.
	 * @return The owner instance's value if the owner still exists, or nullptr
	 *         if the owner does not exist or the locking timed out.
	 * @see    unlock();
	 *
	 * While a child is locked, the owner instance is locked as well, preventing
	 * other children from accessing the data.
	 */
	Type *try_lock(time_t timeout);

	/**
	 * @brief  Unlocks the owner instance.
	 *
	 * If attempting an already unlocked instance, then the behavior is
	 * undefined.
	 */
	void unlock();

private:

	inline Type *lock_()
	{
		if (parent_ != nullptr)
		{
			locked_ = true;
			return parent_->value_;
		}
		mutex_.unlock();
		return nullptr;
	}

	DISALLOW_COPY_AND_ASSIGN(child_ptr<Type>);
	friend class owned_ptr<Type>;
	std::mutex mutex_;
	owned_ptr<Type> *parent_;
	bool locked_;
};

template <typename Type> owned_ptr<Type>::owned_ptr() : value_(nullptr) {}

template <typename Type> owned_ptr<Type>::owned_ptr(Type *value) :
	value_(value) {}

template <typename Type> owned_ptr<Type>::~owned_ptr()
{
	std::lock_guard<std::mutex> guard(mutex_);
	(void)guard; // Remove 'unused' warnings.
	for (auto &i : children_)
	{
		i->mutex_.lock();
		i->parent_ = nullptr;
		i->mutex_.unlock();
	}
	delete value_;
}

template <typename Type> void owned_ptr<Type>::get(child_ptr<Type> &child)
{
	child.mutex_.lock();
	if (child.parent_ != nullptr)
	{
		child.parent_->remove_(&child);
	}
	child.parent_ = this;
	child.mutex_.unlock();

	std::lock_guard<std::mutex> guard(mutex_);
	(void)guard; // Remove 'unused' warnings.
	children_.push_back(&child);
}

template <typename Type> void owned_ptr<Type>::set(Type *value)
{
	std::lock_guard<std::mutex> guard(mutex_);
	(void)guard; // Remove 'unused' warnings.
	delete value_;
	value_ = value;
}

template <typename Type> void owned_ptr<Type>::remove_(child_ptr<Type> *child)
{
	std::lock_guard<std::mutex> guard(mutex_);
	(void)guard; // Remove 'unused' warnings.

	auto i = children_.begin();
	while (i != children_.end())
	{
		if (*i == child)
		{
			children_.erase(i);
			return;
		}
	}
	assert(true);

}

template <typename Type> child_ptr<Type>::child_ptr() : parent_(nullptr) {}

template <typename Type> child_ptr<Type>::child_ptr(owned_ptr<Type> *owner)
{
	owner->get(*this);
}

template <typename Type> child_ptr<Type>::~child_ptr()
{
	// Causes a wait for child_ptr<Type>::unlock() and prevents more calls.
	std::lock_guard<std::mutex> guard(mutex_);
	(void)guard; // Remove 'unused' warnings.

	if (parent_ != nullptr)
	{
		parent_->remove_(this);
	}
}

template <typename Type> Type *child_ptr<Type>::lock()
{
	mutex_.lock();
	return lock_();
}

template <typename Type> Type *child_ptr<Type>::try_lock(time_t timeout)
{
	time_t now;
	time(&now);
	do
	{
		if (mutex_.try_lock())
		{
			return lock_();
		}
	}
	while (now + timeout < time(nullptr));
	return nullptr;
}

template <typename Type> void child_ptr<Type>::unlock()
{
	if (locked_)
	{
		mutex_.unlock();
		parent_->mutex_.unlock();
		locked_ = false;
	}
}

#endif // OWNED_PTR__HPP_
