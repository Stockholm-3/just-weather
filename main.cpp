#include <iostream>

using namespace std;

class program
{
public:
    string name;
    string surname;
    string age;
    string gender;

    string space = " ";
    string newline = "\n";
}; 

int main() {
    program p;
    cout << "Name and Surname: ";
    cin >> p.name;
    cin >> p.surname;

    cout << p.name << p.space << p.surname << p.newline;


}