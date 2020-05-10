base_name = 'sc';
block_file = append(base_name,'_result_outline.txt');
cluster_file   = append(base_name,'_result_cluster.txt');

axis auto equal

% parameters setting transparency of cluster
transp_cluster = 0.3;
color_cluster = 'black';

% plot the cluster
origin = readmatrix(cluster_file);
a = transpose(origin);
[numRows,~] = size(a);
if (numRows>=8)
    nx = a(1:4,:);
    ny = a(5:8,:);
    patch(nx,ny,color_cluster,'LineStyle','--', 'FaceAlpha', transp_cluster);
    hold on
end

% plot the blocks and boundary
origin = readmatrix(block_file);
a = transpose(origin);
[numRows,~] = size(a);
if (numRows>=8)
    boundary_x = a(1:4,1);
    boundary_y = a(5:8,1);
    patch(boundary_x, boundary_y, 'white', 'FaceAlpha', 0)
    hold on
    
    x = a(1:4,2:end);
    y = a(5:8,2:end);
    patch(x,y,'cyan')
    hold on
end


