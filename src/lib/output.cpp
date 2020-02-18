#include <iostream>
using namespace std;
extern "C"{
void kout(double in)
{
	cout << "kout: val is " << in << endl;
}
}
