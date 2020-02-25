base_name = 'lg';
boundary_file = append(base_name,'_result_outline.txt');
unplug_file   = append(base_name,'lg_result_unplug.txt');
plug_file     = append(base_name,'lg_result_plug.txt');

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
transp_unplug = 0.3;
transp_plug   = 0.8;
color_n = 'green';
color_p = 'yellow';

% plot the unpluged cell
origin = readmatrix(unplug_file);
a = transpose(origin);
[numRows,~] = size(a);
if (numRows>=16)
    nx = a(1:4,:);
    ny = a(5:8,:);
    patch(nx,ny,color_n,'EdgeColor','none', 'FaceAlpha', transp_unplug);
    hold on
    px = a(9:12,:);
    py = a(13:16,:);
    patch(px,py,color_p,'EdgeColor','none', 'FaceAlpha', transp_unplug);
    hold on
end

% plot the pluged cell
origin = readmatrix(plug_file);
a = transpose(origin);
[numRows,~] = size(a);
if (numRows>=16)
    nx = a(1:4,:);
    ny = a(5:8,:);
    patch(nx,ny,color_n,'EdgeColor','none', 'FaceAlpha', transp_plug);
    hold on
    px = a(9:12,:);
    py = a(13:16,:);
    patch(px,py,color_p,'EdgeColor','none', 'FaceAlpha', transp_plug);
end
