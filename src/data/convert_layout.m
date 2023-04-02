im = imread('man1.bmp');

sz = 64;

# Output order left, right, up, down
input_order = [2,3,0,1];

output_im = uint8(zeros(64*7, 32*3, 3));
I=1:64;
J=1:32;
for i=1:4,
  for j=0:2,
    output_im(I + (64 * (i - 1)), J + (32*j), :) = im(I + (64 * (3 * input_order(i) + j)), J, :);
  end
end

for i=1:3,
  for j=0:2,
    output_im(I + (64 * (i - 1) + 4 * 64), J + (32*j), :) = im(I + (64 * (3 * (i - 1) + j)), J + 32, :);
  end
end
imshow(output_im);