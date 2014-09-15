#include <iostream>
#include "owned_ptr.hpp"

int main()
{
	// Create an owned pointer, with the value of '1204'.
	owned_ptr<int> *owner = new owned_ptr<int>(new int(1204));

	// Store a reader to the owned pointer.
	reader_ptr<int> owned_ref;
	owner->get(owned_ref);

	// Another reader. Get value from constructor.
	reader_ptr<int> other_owned_ref(owner);

	// Get, validate and return the value.
	int *value = owned_ref.lock();
	other_owned_ref.unlock(); // Nice try, doesn't do anything to above lock.

	if (value != nullptr)
	{
		// Okay, owner is valid, according to above conditional.
		// Should print the value of '1204'.
		std::cout << "Value: " << (*value) << std::endl;
		owned_ref.unlock(); // We always need to unlock when done.
	}

	// Again, more reader pointers.
	reader_ptr<int> copied_owned_ref(owned_ref);
	(void)copied_owned_ref; // Ignore 'unused' warnings.

	// We now have 3 readers here.
	std::cout << "Count: " << owner->count() << std::endl;

	// Change the value to something else and test.
	owner->reset(new int(326));
	std::cout << "New Value: " << (*owned_ref.lock()) << std::endl;
	owned_ref.unlock();

	// Delete the owner, therefore deleting children references.
	delete owner;

	// Children value is now invalidated.
	// Returns true.
	std::cout << ((owned_ref.lock() == nullptr) ? "Invalid" : "Valid");
	std::cout << std::endl;

	return 0;
}
