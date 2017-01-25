#include <stdio.h>
#include <string.h>
#include <iostream>
using namespace std;
struct Books {
   char  title[50];
   char  author[50];
   char  subject[100];
   int   book_id;
};
 
int main() {
    cout<<"helloworld"<<endl;
    struct Books BookArray[10];        /* Declare Book1 array of type Book */
    for (int i=0;i<10;i++){
        BookArray[i].book_id=i;
        cout<<" current book id is : "<<BookArray[i].book_id<<endl;
        cout<<" current book size is : "<<sizeof(BookArray[i])<<endl;
    }
    return 0;
}
