disp_file = 'disp_result.txt';
origin = readmatrix(disp_file);
a = transpose(origin);

X = a(1,:);
Y = a(2,:);
U = a(3,:);
V = a(4,:);
axis auto equal
quiver(X, Y, U, V, 0, 'Color', 'black');
