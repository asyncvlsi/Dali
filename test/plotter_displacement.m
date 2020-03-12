axis auto equal

a = readmatrix('lg_displacement.txt');

x = a(1,:);
y = a(2,:);
u = a(3,:);
v = a(4,:);
quiver(x,y,u,v)
