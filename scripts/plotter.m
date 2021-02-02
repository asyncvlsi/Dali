axis auto equal

longnet = true;
base_name = 'gb';
block_file = append(base_name,'_result.txt');

origin = readmatrix(block_file);
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

if longnet
    hold on;
    net_file   = append(base_name,'_longnet.txt');
    origin = readmatrix(net_file);
    a = transpose(origin);

    X1 = a(1,:);
    Y1 = a(2,:);
    X2 = a(3,:);
    Y2 = a(4,:);
    line([X1; X2], [Y1; Y2], 'Color', 'red'); % line or plot, both works
end
