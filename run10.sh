
LOOPS=$1-1
REPEAT=$2

export LD_LIBRARY_PATH=`pwd`
echo LOOP:$LOOPS X REPEAT:$REPEAT

for ((i = 1; i <= $LOOPS; i++)); do
  ./visSele $REPEAT&
done

time ./visSele $REPEAT
