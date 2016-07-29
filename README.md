# python-imgsum
A python module written in "C" which calculates row/col sums for YUV420 imgs and writes them to a file.

## Motivation
Written as part of a project that needed to determine the position of a laser dot as it moved against a reasonably 
dark background. Since high time resolution was desired, it was important to process images as quickly as possible.
Therefor, this code was written to process raw YUV420 images as they came from the camera with resulting row/col sums
being written to a file for later processing by other software.

## Usage Examples

### Calculate Sums
Calculate column sums for a number of images and store the sums in an output file. 

Recipe:

1. Begin the batch with `begin_batch`
2. Specify an output file with `save_sums_to`
3. Process each image in the batch with `process_img`
4. End the batch with `end_batch`

Python:
```python
import imgsum

imgsum.begin_batch(imgwidth, imageheight, False, True) # Want col sums but no row sums.
imgsum.save_sums_to("RecordedData.bin")

for img in images():
    imgsum.process_img(img)

imgsum.end_batch()
```

### Calculate Sums and Grand Totals

Same as the previous example, but also calculates grand totals for the sums.

Recipe:

1. Begin the batch with `begin_batch`
2. Enable grand totals with `grand totals`
3. Specify an output file with `save_sums_to`
4. Process each image in the batch with `process_img`
5. End the batch with `end_batch`

Python:
```python
import imgsum

imgsum.begin_batch(imgwidth, imageheight, False, True)
imgsum.grand_totals(True)
imgsum.save_sums_to("RecordedData.bin")

for img in images():
    imgsum.process_img(img)

imgsum.end_batch()
```

### Determine and Ignore Constant Background

Consider a case where the position of a laser dot in some static scene is to be determined. The grand total for a number 
of scene imgages with the laser off provides enough information for imgsum to automatically subtract the background from subsequent scene images with the laser on.

Recipe:

1. Begin the batch with `begin_batch`
2. Enable grand totals with `grand totals`
3. Process a number of background images with `process_img`
4. Request that the accumulated grand total be transformed into background info with `set_bg`
5. Disable further grand totaling with `grand totals`
6. Specify an output file with `save_sums_to`
7. Process a number of images that include the laser dot with `process_img`
8. End the batch with `end_batch`

```python
import imgsum

imgsum.begin_batch(imgwidth, imageheight, False, True)
imgsum.grand_totals(True)

for img in background_images():
    imgsum.process_img(img)

imgsum.set_bg()
imgsum.grand_totals(False)
imgsum.save_sums_to("RecordedData.bin")

for img in laser_images():
    imgsum.process_img(img)

imgsum.end_batch()
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
