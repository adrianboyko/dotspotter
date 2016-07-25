# python-imgsum
A python module written in "C" which calculates row/col sums for YUV420 imgs and writes them to a file.

## Motivation
Written as part of a project that needed to determine the position of a laser dot as it moved against a reasonably 
dark background. Since high time resolution was desired, it was important to process images as quickly as possible.
Therefor, this code was written to process raw YUV420 images as they came from the camera with resulting row/col sums
being written to a file for later processing by other software.

## Usage
Sums for a number of images are calculated and stored in a single output file. Begin the batch with `start`, process each image in the batch with `sum`, and end the batch with `stop`.
```python
import imgsum

# Start by spec'ing width/height of imgs, output file name, and whether row and/or col sums are desired.
imgsum.start(imgwidth, imageheight, "RecordedData.bin", False, True)

# Process a number of images.
while more_images():
    img = get_image()
    imgsum.sum(img)

# Clean up when done.
imgsum.stop()
```
## Output File Format
Binary file containing a sequence of 32 bit integers. If the images processed have height H and row summing is selected
but col summing is not, then the first H integers in the output file represent the row sums for the first image, the next
H integers represent the row sums for the second image, etc. As an example, the data for a batch of 1920x1080 images could 
be loaded into Mathematica as follows:

```mathematica
data = BinaryReadList["RecordedData.bin", "Integer32"];
data = Partition[data, 1080];
```
`Length(data)` would then be the number of images in the batch.

If row and column summing is requested for images of height H and width W, then the output will alternate between row sums
and col sums: 
* H integers (row sums for image #1)
* W integers (col sums for image #1)
* H integers (row sums for image #2)
* W integers (col sums for image #2)
* ...
