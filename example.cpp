#include <iostream>
#include "owned_ptr.hpp"

int main()
{
	// Create an owned pointer, with the value of '3'.
	owned_ptr<int> *owner = new owned_ptr<int>(new int(3));

	// Store a referenced to the owned pointer.
	child_ptr<int> owned_ref;
	owner->get(owned_ref);

	// Another referenced child.
	child_ptr<int> other_owned_ref;
	owner->get(other_owned_ref);

	// Get, validate and return the value.
	int *value = owned_ref.lock();
	other_owned_ref.unlock(); // Nice try, won't work.
	if (value != nullptr)
	{
		// Okay, owner is valid, says above conditional.
		// Should print the value of '3'.
		std::cout << (*value);
		owned_ref.unlock(); // We always need to unlock when done.
	}

	// Delete the owner, therefore deleting children references.
	delete owner;

	// Children value is now invalidated.
	std::cout << (owned_ref.lock() == nullptr); // Returns true.

	return 0;
}
