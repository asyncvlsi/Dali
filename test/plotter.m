axis auto equal

origin = readmatrix('sc_result.txt');
a = transpose(origin);

boundary_x = a(1:4,:);
boundary_y = a(5:8,:);
tmp_color = a(9:11,:);
[m,n] = size(tmp_color);
tmp_color = transpose(tmp_color);

color=zeros(n,1,3);
for i=1:n
   color(i,1,1) = tmp_color(i,1);
   color(i,1,2) = tmp_color(i,2);
   color(i,1,3) = tmp_color(i,3);
end

patch(boundary_x, boundary_y, color);
