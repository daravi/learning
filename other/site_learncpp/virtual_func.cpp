#include <iostream>

class Base
{
public:
	virtual const char* getName() { return "Base"; }

};

class Derived: public Base
{
public:
	virtual const char* getName() { return "Derived"; }
	
};

int main(int argc, char const *argv[])
{
	Derived derived;
	Base &rBase = derived;
	std::cout << "rBase is a " << rBase.getName() << '\n';

	return 0;
}