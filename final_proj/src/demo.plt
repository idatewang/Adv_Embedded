set yrange [0:1]
set xrange [*:*]
plot "data.txt" using 1:2 with lines
pause 1
reread
