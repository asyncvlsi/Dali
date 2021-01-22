base_name = 'cg';
Nframes = 30;

h = figure;
axis auto equal % this ensures that getframe() returns a consistent size
filename = 'testAnimated.gif';
for num = 0:1:Nframes
    % Draw the plot
    block_file = append(base_name, '_result_');
    block_file = append(block_file, int2str(num));
    block_file = append(block_file, '.txt');
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
    drawnow 
    
      % Capture the plot as an image 
      frame = getframe(h); 
      im = frame2im(frame); 
      [imind,cm] = rgb2ind(im,256); 
      % Write to the GIF File 
      if num == 0
          imwrite(imind,cm,filename,'gif', 'Loopcount',inf); 
      else 
          imwrite(imind,cm,filename,'gif','WriteMode','append'); 
      end 
end