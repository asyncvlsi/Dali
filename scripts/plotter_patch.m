base_name = 'scw';
boundary_file = append(base_name,'_outline.txt');
unplug_file   = append(base_name,'_unplug.txt');
p_file        = append(base_name,'_pwell.txt');
n_file        = append(base_name,'_nwell.txt');

axis auto equal
% plot the placement boundary
origin = readmatrix(boundary_file);
a = transpose(origin);
[numRows,~] = size(a);
if (numRows>=8)
    x = a(1:4,:);
    y = a(5:8,:);
    patch(x,y,'white')
    hold on
end

% parameters setting transparency of unplug well and plug well
% color of n well and p well
transp_unplug = 0.5;
transp_plug   = 0.8;
transp_well   = 0.2;
color_n = 'green';
color_p = 'yellow';

% plot the unpluged cell
origin = readmatrix(unplug_file);
a = transpose(origin);
[numRows,~] = size(a);
if (numRows>=16)
    nx = a(1:4,:);
    ny = a(5:8,:);
    patch(nx,ny,color_n, 'EdgeColor','black', 'FaceAlpha',transp_unplug, 'LineStyle', ':');
    hold on
    px = a(9:12,:);
    py = a(13:16,:);
    patch(px,py,color_p, 'EdgeColor','black', 'FaceAlpha',transp_unplug, 'LineStyle', ':');
    hold on
end

% plot the p-well
origin = readmatrix(p_file);
a = transpose(origin);
[numRows,~] = size(a);
if (numRows>=8)
    nx = a(1:4,:);
    ny = a(5:8,:);
    patch(nx,ny,color_p,'EdgeColor','none', 'FaceAlpha', transp_well);
    hold on
end


% plot the n-well
origin = readmatrix(n_file);
a = transpose(origin);
[numRows,~] = size(a);
if (numRows>=8)
    nx = a(1:4,:);
    ny = a(5:8,:);
    patch(nx,ny,color_n,'EdgeColor','none', 'FaceAlpha', transp_well);
    hold on
end

