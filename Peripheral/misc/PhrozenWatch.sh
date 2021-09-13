while :
do
  clear; 
  curl -s http://192.168.1.165/ \
  |grep "div class=\"text\"" \
  |sed   's/.*\> *\(.*\)\<.*/\1/' 
  #extract info in line
  
  sleep 10;
done