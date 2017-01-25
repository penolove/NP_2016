#include <iostream> 
#include "Point2D.h"
using namespace std; 

int main() {
    d2d::Point2D p1(10, 10); 

    cout << "(x , y) : (" 
         << p1.x() << ", " 
         << p1.y() << ")"
         << endl; 
 
    return 0; 
} 
