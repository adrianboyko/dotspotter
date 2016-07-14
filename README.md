# python-imgsum
A python module written in "C" which calculates row/col sums for YUV420 imgs and writes them to a file.

## Motivation
Written as part of a project that needed to determine the position of a laser dot as it moved against a reasonably 
dark background. Since high time resolution was desired, it was important to process images as quickly as possible.
Therefor, this code was written to process raw YUV420 images as they came from the camera with resulting row sums
being written to a file for later processing by other software.

## Usage

```python
import imgsum

imgsum.start()
while more_images():
    img = get_image()
    imgsum.sum(img)
imgsum.stop()
```
