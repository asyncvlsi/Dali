sz0 = 1;
sz1 = 1;
a = dlmread('nodes.txt');
scatter(a(:,1),a(:,2),sz0,'blue','o'); 
% plot each node with size 'sz0', color 'blue', shape 'o' circle
hold on;
b = dlmread('terminal.txt');
scatter(b(:,1),b(:,2),sz1,'black','square');
% plot each terminal edges with size 'sz1', color 'black', shape 'square'

% hold on
% b = dlmread('grid_bin_not_all_terminal.txt');
% scatter(b(:,1),b(:,2),sz1,'black','square');
% hold on
% b = dlmread('grid_bin_all_terminal.txt');
% scatter(b(:,1),b(:,2),sz1,'red','square');

% hold on
% b = dlmread('grid_bin_not_overfill.txt');
% scatter(b(:,1),b(:,2),sz1,'black','square');
% hold on
% b = dlmread('grid_bin_overfill.txt');
% scatter(b(:,1),b(:,2),sz1,'red','square');

% hold on
% b = dlmread('first_grid_bin_cluster.txt');
% scatter(b(:,1),b(:,2),sz1,'red','square');
% hold on
% b = dlmread('all_grid_bin_clusters.txt');
% scatter(b(:,1),b(:,2),sz1,'red','square');

% hold on
% b = dlmread('first_bounding_box.txt');
% scatter(b(:,1),b(:,2),sz1,'red','square');
% hold on
% b = dlmread('first_cell_bounding_box.txt');
% scatter(b(:,1),b(:,2),sz1,'green','square');

axis equal
