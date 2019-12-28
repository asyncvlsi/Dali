axis auto equal

origin = readmatrix('lg_result_outline.txt');
a = transpose(origin);

boundary_x = a(1:4,1);
boundary_y = a(5:8,1);
patch(boundary_x, boundary_y, 'white')
hold on

x = a(1:4,2:end);
y = a(5:8,2:end);
patch(x,y,'cyan')
