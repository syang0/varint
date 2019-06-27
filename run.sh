make varint -j10
clear
stdbuf -o 0 ./varint | tee results.txt
