axis auto equal

origin = readmatrix('lg_result_outline.txt');
a = transpose(origin);
x = a(1:4,:);
y = a(5:8,:);
patch(x,y,'white')

hold on

origin = readmatrix('lg_result_unplug.txt');
a = transpose(origin);
x = a(1:4,:);
y = a(5:8,:);
patch(x,y,'green','EdgeColor','none')