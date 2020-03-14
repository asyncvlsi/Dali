axis auto equal

a = readmatrix('disp_result.txt');

x = a(:,1);
y = a(:,2);
u = a(:,3);
v = a(:,4);
% quiver(x,y,u,v,'AutoScale','off','LineWidth',2);
quiver(x,y,u,v,'AutoScale','off');

pbaspect([1 1 1])
