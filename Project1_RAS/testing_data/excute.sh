for ((c=1;c<=9;c++))
do
    ./client 127.0.0.1 9081 Lab_test/test$c.txt > myown/Lab_test$c.txt;
done
