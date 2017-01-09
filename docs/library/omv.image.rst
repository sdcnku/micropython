:mod:`image` --- machine vision
===============================

.. module:: image
   :synopsis: machine vision

The ``image`` module is used for machine vision.

Functions
---------

.. function:: rgb_to_lab((r, g, b))

   Returns the LAB tuple for the RGB888 tuple.

   .. note:: RGB888 means 8-bits (0-255) for red, green, and blue. For LAB, L
             goes from 0-100 and a/b go from -128 to 127.

.. function:: lab_to_rgb((l, a, b))

   Returns the RGB888 tuple for the LAB tuple.

   .. note:: RGB888 means 8-bits (0-255) for red, green, and blue. For LAB, L
             goes from 0-100 and a/b go from -128 to 127.

.. function:: rgb_to_grayscale((r, g, b))

   Returns the grayscale value for the RGB888 tuple.

   .. note:: RGB888 means 8-bits (0-255) for red, green, and blue. The grayscale
             values goes between 0-255.

.. function:: grayscale_to_rgb(gs)

   Returns the RGB888 tuple for the grayscale value.

   .. note:: RGB888 means 8-bits (0-255) for red, green, and blue. The grayscale
             values goes between 0-255.

.. function:: load_decriptor(type, path)

   Loads a descriptor object saved by your OpenMV Cam or the OpenMV IDE into
   memory and returns a descriptor object.

   ``type`` is the descriptor type to load which can be either ``image.LBP`` or
   ``image.FREAK``.

   ``path`` is the path to the descriptor file.

   .. note:: This function needs to be reworked for usability. You shouldn't
             have to pass the descriptor type to load one. The descriptor file
             format needs to be updated to support this...

.. function:: save_descriptor(type, path, descriptor)

   Saves the descriptor object ``descriptor`` to disk.

   ``type`` is the descriptor type to save which can be either ``image.LBP`` or
   ``image.FREAK``.

   ``path`` is the path to the descriptor file.

   .. note:: This function needs to be reworked for usability. You shouldn't
             have to pass the descriptor type to save one. The descriptor file
             format needs to be updated to support this...

.. function:: match_descriptor(type, descritor0, descriptor1, threshold=60)

   For LBP descriptors this function returns an integer representing the difference
   between the two descriptors. As of right now, this is rather... hard to use...

   For FREAK descriptors this function returns the center matching area between
   the two sets of keypoints. Threshold (0-100) controls the matching rate where
   a higher threshold results in less false positives but less matches.

   ``type`` is the descriptor type to save which can be either ``image.LBP`` or
   ``image.FREAK``.

   .. note:: Like the other descriptor functions this function needs to be reworked
             to auto detect descriptors and additionally should return bounding
             boxes around matching areas along with the centroid. Anyway, there
             wasn't time to rework this for the initial release so this is what
             we have for right now.

class HaarCascade -- Feature Descriptor
=======================================

The Haar Cascade feature descriptor is used for the ``image.find_features()``
method. It doesn't have any methods itself for you to call.

Constructors
------------

.. class:: image.HaarCascade(path, stages=Auto)

    Loads a Haar Cascade into memory from a Haar Cascade binary file formatted for
    your OpenMV Cam. If you pass "frontalface" instead of a path then this constructor
    will load the built-in frontal face Haar Cascade into memory. Additionally, you
    can also pass "eye" to load a Haar Cascade for eyes into memory. Finally, this
    method returns the loaded Haar Cascade object for use with ``image.find_features()``.

    ``stages`` defaults to the number of stages in the Haar Cascade. However, you
    can specify a lower number of stages to speed up processing the feature detector
    at the cost of a higher rate of false positives.

    .. note:: You can make your own Haar Cascades to use with your OpenMV Cam.
              First, Google for "<thing> Haar Cascade" to see if someone already
              made an OpenCV Haar Cascade for an object you want to detect. If not...
              then you'll have to generate your own (which is a lot of work). If
              so, then see the ``openmv-cascade.py`` script for converting OpenCV
              Haar Cascades into a format your OpenMV Cam can read.

    Q: What is a Haar Cascade?
    A: A Haar Cascade is a series of contrast checks that are used to determine
    if an object is present in the image. The contrast checks are split of into
    stages where a stage is only run if previous stages have already passed. The
    contrast checks are simple things like checking if the center vertical of the
    image is lighter than the edges. Large area checks are performed first in the
    earlier stages followed by more numerous and smaller area checks in later
    stages.

    Q: How are Haar Cascades made?
    A: Haar Cascades are made by training the generator algorithm against positive
    and negative labeled images. For example, you'd train the generator algorithm
    against hundreds of pictures with cats in them that have been labeled as images
    with cats and against hundreds of images with not cat like things labeled differently.
    The generator algorithm will then produce a Haar Cascade that detects cats.

class Image -- Image object
===========================

The image object is the basic object for machine vision operations.

Constructors
------------

.. class:: image.Image(path)

   Creates a new image object from a file at ``path``.

   Supports bmp/pgm/ppm/jpg/jpeg image files.

   .. note:: This constructor is supposed to be used for loading image to do
             template matching. Due to memory constraints you should only load
             images that are small in size. For example, under 80x60.

   Images support "[]" notation. Do ``image[index] = 8/16-bit value`` to assign
   an image pixel or ``image[index]`` to get an image pixel which will be either
   an 8-bit value for grayscale images of a 16-bit RGB565 value for RGB images.

   For JPEG images the "[]" allows you to access the compressed JPEG image blob as a byte-array.

   Images also support read buffer operations. You can pass images to all sorts
   of MicroPython functions like as if the image were a byte-array object.

Methods
-------

.. method:: image.copy(roi=Auto)

   Creates a copy of the image object.

   ``roi`` is the region-of-interest rectangle (x, y, w, h). If not specified,
   it is equal to the image rectangle.

   roi is not applicable when copying jpeg images.

   .. note:: You will run out of memory trying to make copies of images unless
             you keep the copy image sizes tiny. For example, under 80x60.

.. method:: image.save(path, roi=Auto, quality=50)

   Saves a copy of the image to the filesystem at ``path``.

   Supports bmp/pgm/ppm/jpg/jpeg image files.

   ``roi`` is the region-of-interest rectangle (x, y, w, h). If not specified,
   it is equal to the image rectangle.

   roi is not applicable when saving jpeg images.

   ``quality`` is the jpeg compression quality to use to save the image to jpeg
   format if the image is not already compressed.

   .. note:: You cannot save jpeg compressed image to an uncompressed format.

.. method:: image.compress(quality=50)

   JPEG compresses a non-compressed image in place.

   ``quality`` is the compression quality (0-100). Note that adjusting this value
   does not improve quality by much since we're already employing a few quality
   reduction tricks to make JPEG compression faster. On future OpenMV Cam's you'll
   be able to control the image quality more easily.

   .. note:: You should use this function to compress images before hand that
             you want to save to disk or add to an mjpeg. If the image object
             points to the frame buffer then this function can use the additional
             frame buffer space to compress the image at a higher quality. Finally,
             after the frame buffer is compressed it does not have to be compressed
             again to be streamed to the IDE (speeding things up).

.. method:: image.compressed(quality=50)

   Returns a JPEG compressed image - the original image is untouched.

   ``quality`` is the compression quality (0-100). Note that adjusting this value
   does not improve quality by much since we're already employing a few quality
   reduction tricks to make JPEG compression faster. On future OpenMV Cam's you'll
   be able to control the image quality more easily.

.. method:: image.width()

   Returns the image width in pixels.

.. method:: image.height()

   Returns the image height in pixels.

.. method:: image.format()

   Returns ``sensor.GRAYSCALE`` for grayscale images, ``sensor.RGB565`` for RGB
   images and ``sensor.JPEG`` for JPEG images.

.. method:: image.size()

   Returns the image size in bytes.

.. method:: image.get_pixel(x, y)

   For grayscale images: Returns the pixel value at location (x, y).
   For RGB images: Returns the pixel tuple (r, g, b) at location (x, y).

   Not supported on compressed images.

.. method:: image.set_pixel(x, y, pixel)

   For grayscale images: Sets the pixel at location (x, y) to the value ``pixel``.
   For RGB images: Sets the pixel at location (x, y) to the tuple (r, g, b) ``pixel``.

   Not supported on compressed images.

.. method:: image.draw_line((x0, y0, x1, y1), color=White)

   Draws a line from (x0, y0) to (x1, y1).

   ``color`` is a value (0-255) for grayscale images and a RGB888 tuple
   (r, g, b) for RGB images.

   Not supported on compressed images.

.. method:: image.draw_rectangle((x, y, w, h), color=White)

   Draws an unfilled rectangle from (x, y) to (x+w, y+h).

   ``color`` is a value (0-255) for grayscale images and a RGB888 tuple
   (r, g, b) for RGB images.

   Not supported on compressed images.

.. method:: image.draw_circle(x, y, radius, color=White)

   Draws an unfilled circle at (x, y) with integer radius ``radius``.

   ``color`` is a value (0-255) for grayscale images and a RGB888 tuple
   (r, g, b) for RGB images.

   Not supported on compressed images.

.. method:: image.draw_string(x, y, text, color=White)

   Draws 8x10 text starting at (x, y) using text ``text``.

   ``\n``, ``\r``, and ``\r\n`` line endings move the cursor to the next line.

   ``color`` is a value (0-255) for grayscale images and a RGB888 tuple
   (r, g, b) for RGB images.

   Not supported on compressed images.

.. method:: image.draw_cross(x, y, size=5, color=White)

   Draws a cross at (x, y) whose sides are ``size`` long.

   ``color`` is a value (0-255) for grayscale images and a RGB888 tuple
   (r, g, b) for RGB images.

   Not supported on compressed images.

.. method:: image.draw_keypoints(keypoints, size=10, color=White)

   If ``keypoints`` is a keypoints object then this method draws a number of
   circles with diameter ``size`` for each keypoint in ``keypoints``.

   If ``keypoints`` a list of tuples [(x, y, angle), (x, y, angle), ...] then
   this method draws a number of circles with diameter ``size`` and angle lines
   with length ``size`` for each tuple in the list of tuples ``keypoints``.

   Angle is a floating point number in radians.

   ``color`` is a value (0-255) for grayscale images and a RGB888 tuple
   (r, g, b) for RGB images.

   Not supported on compressed images.

.. method:: image.binary(thresholds, invert=False)

   For grayscale images ``thresholds`` is a list of (lower, upper) grayscale
   pixel thresholds to segment the image by. Segmentation converts all pixels
   within the thresholds to 1 (white) and all pixels outside to 0 (black).

   For RGB images ``thresholds`` is a list of (l_lo, l_hi, a_lo, a_hi, b_lo, b_hi)
   LAB pixel thresholds to segment the image by. Segmentation converts all pixels
   within the thresholds to 1 (white) and all pixels outside to 0 (black).

   Lo/Hi thresholds being swapped is automatically handled.

   ``invert`` inverts the outcome of the segmentation operation.

   Not supported on compressed images.

.. method:: image.invert()

   Inverts the binary image 0 (black) pixels go to 1 (white) and 1 (white)
   pixels go to 0 (black).

   Not supported on compressed images.

.. method:: image.and(image)

   Logically ANDs this image with another image.

   ``image`` can either be an image object or a path to an uncompressed image
   file (bmp/pgm/ppm).

   Both images must be the same size and the same type (grayscale/rgb).

   Not supported on compressed images.

   .. note:: You can use this function to mask out parts of an image you don't
             want processed for things like frame differencing. For example,
             you can create a mask image on your computer, save it as a BMP
             file, and then use that file with this method. You'd set all areas
             you'd like to mask to black and all unmasked areas to white.

.. method:: image.nand(image)

   Logically NANDs this image with another image.

   ``image`` can either be an image object or a path to an uncompressed image
   file (bmp/pgm/ppm).

   Both images must be the same size and the same type (grayscale/rgb).

   Not supported on compressed images.

.. method:: image.or(image)

   Logically ORs this image with another image.

   ``image`` can either be an image object or a path to an uncompressed image
   file (bmp/pgm/ppm).

   Both images must be the same size and the same type (grayscale/rgb).

   Not supported on compressed images.

   .. note:: You can use this function to mask out parts of an image you don't
             want processed for things like frame differencing. For example,
             you can create a mask image on your computer, save it as a BMP
             file, and then use that file with this method. You'd set all areas
             you'd like to mask to white and all unmasked areas to black.

.. method:: image.nor(image)

   Logically NORs this image with another image.

   ``image`` can either be an image object or a path to an uncompressed image
   file (bmp/pgm/ppm).

   Both images must be the same size and the same type (grayscale/rgb).

   Not supported on compressed images.

.. method:: image.xor(image)

   Logically XORs this image with another image.

   ``image`` can either be an image object or a path to an uncompressed image
   file (bmp/pgm/ppm).

   Both images must be the same size and the same type (grayscale/rgb).

   Not supported on compressed images.

.. method:: image.xnor(image)

   Logically XNORs this image with another image.

   ``image`` can either be an image object or a path to an uncompressed image
   file (bmp/pgm/ppm).

   Both images must be the same size and the same type (grayscale/rgb).

   Not supported on compressed images.

.. method:: image.erode(size, threshold=Auto)

   Removes pixels from the edges of segmented areas.

   This method works by convolving a kernel of ((size*2)+1)x((size*2)+1) pixels
   across the image and zeroing the center pixel of the kernel if the sum of
   the neighbor pixels set is not greater than ``threshold``.

   This method works like the standard erode method if threshold is not set. If
   ``threshold`` is set then you can specify erode to only erode pixels that
   have, for example, less than 2 pixels set around them with a threshold of 2.

   Not supported on compressed images.

   .. note:: This method is designed to work on binary images.

.. method:: image.dilate(size, threshold=Auto)

   Adds pixels from the edges of segmented areas.

   This method works by convolving a kernel of ((size*2)+1)x((size*2)+1) pixels
   across the image and setting the center pixel of the kernel if the sum of
   the neighbor pixels set is greater than ``threshold``.

   This method works like the standard dilate method if threshold is not set. If
   ``threshold`` is set then you can specify dilate to only dilate pixels that
   have, for example, more than 2 pixels set around them with a threshold of 2.

   Not supported on compressed images.

   .. note:: This method is designed to work on binary images.

.. method:: image.negate()

   Numerically inverts pixel values for each color channel. E.g. (255-pixel).

   Not supported on compressed images.

.. method:: image.difference(image)

   Subtracts another image from this image. E.g. for each color channel each
   pixel is replaced with ABS(this.pixel-image.pixel).

   ``image`` can either be an image object or a path to an uncompressed image
   file (bmp/pgm/ppm).

   Both images must be the same size and the same type (grayscale/rgb).

   Not supported on compressed images.

   .. note:: This function is used for frame differencing which you can then use
             to do motion detection. You can then mask the resulting image using
             AND/OR before running statistics functions on the image.

.. method:: image.replace(image)

   Replace this image with ``image`` (this is much faster than blend for this).

   ``image`` can either be an image object or a path to an uncompressed image
   file (bmp/pgm/ppm).

   Both images must be the same size and the same type (grayscale/rgb).

   Not supported on compressed images.

.. method:: image.blend(image, alpha=128)

   Blends another image ``image`` into this image.

   ``image`` can either be an image object or a path to an uncompressed image
   file (bmp/pgm/ppm).

   ``alpha`` controls the transparency. 256 for an opaque overlay. 0 for none.

   Both images must be the same size and the same type (grayscale/rgb).

   Not supported on compressed images.

.. method:: image.morph(size, kernel, mul=Auto, add=0)

   Convolves the image by a filter kernel.

   ``size`` controls the size of the kernel which must be
   ((size*2)+1)x((size*2)+1) pixels big.

   ``kernel`` is the kernel to convolve the image by. It can either be a tuple
   or a list of [-128:127] values.

   ``mul`` is number to multiply the convolution pixel result by. When not set
   it defaults to a value that will prevent scaling in the convolution output.

   ``add`` is a value to add to each convolution pixel result.

   .. note:: ``mul`` basically allows you to do a global contrast adjustment and
             ``add`` allows you to do a global brightness adjustment.

.. method:: image.statistics(roi=Auto)

   Computes basic color channel statistics for the image and returns a tuple
   containing the stats:

   ``roi`` is the region-of-interest rectangle (x, y, w, h). If not specified,
   it is equal to the image rectangle.

   For grayscale images:

     - [0]: Grayscale Mean
     - [1]: Grayscale Median (50% value)
     - [2]: Grayscale Mode
     - [3]: Grayscale Standard Deviation
     - [4]: Grayscale Minimum
     - [5]: Grayscale Maximum
     - [6]: Grayscale Lower Quartile (25% value)
     - [7]: Grayscale Upper Quartile (75% value)

   For rgb images:

     - [0]: L Mean
     - [1]: L Median (50% value)
     - [2]: L Mode
     - [3]: L Standard Deviation
     - [4]: L Minimum
     - [5]: L Maximum
     - [6]: L Lower Quartile (25% value)
     - [7]: L Upper Quartile (75% value)
     - [8]: A Mean
     - [9]: A Median (50% value)
     - [10]: A Mode
     - [11]: A Standard Deviation
     - [12]: A Minimum
     - [13]: A Maximum
     - [14]: A Lower Quartile (25% value)
     - [15]: A Upper Quartile (75% value)
     - [16]: B Mean
     - [17]: B Median (50% value)
     - [18]: B Mode
     - [19]: B Standard Deviation
     - [20]: B Minimum
     - [21]: B Maximum
     - [22]: B Lower Quartile (25% value)
     - [23]: B Upper Quartile (75% value)

   In the future we'll make this method return an object instead of a tuple...

   Not supported on compressed images.

   .. note:: This method is your basic work horse for quickly determining what's
             going on in the image. For example, if you need to determine motion
             after frame differencing you can use this method to see if the pixels
             in the image aren't close to zero (which they should be for no motion).
             You can also use this method for automatically updating binary/find_blobs
             threshold settings.

.. method:: image.midpoint(size, bias=0.5)

   Runs the midpoint filter on the image.

   ``size`` is the kernel size. Use 1 (3x3 kernel), 2 (5x5 kernel), or higher.

   ``bias`` controls the min/max mixing. 0 for min filtering only, 1.0 for max
   filtering only.

   Not supported on compressed images.

   .. note:: By using the ``bias`` you can min/max filter the image.

.. method:: image.mean(size)

   Standard mean blurring filter (faster than using morph for this).

   ``size`` is the kernel size. Use 1 (3x3 kernel), 2 (5x5 kernel), or higher.

   Not supported on compressed images.

.. method:: median(size, percentile=0.5)

   Runs the median filter on the image. The median filter is the best filter for
   smoothing surfaces while preserving edges... but, it's slow...

   ``size`` is the kernel size. Use 1 (3x3 kernel) or 2 (5x5 kernel).

   ``percentile`` control the percentile of the value used in the kernel. By
   default each pixel is replace with the 50th percentile (center) of it's
   neighbors. You can set this to 0 for a min filter, 0.25 for a lower quartile
   filter, 0.75 for an upper quartile filter, and 1.0 for a max filter.

   Not supported on compressed images.

.. method:: image.mode(size)

   Runs the mode filter on the image by replacing each pixel with the mode of
   their neighbors. This method works great on grayscale images. However, on
   RGB images it creates a lot of artifacts on edges because of the non-linear
   nature of the operation.

   ``size`` is the kernel size. Use 1 (3x3 kernel) or 2 (5x5 kernel).

   Not supported on compressed images.

.. method:: image.histeq()

   Runs the histogram equalization algorithm on the image. Histogram
   equalization normalizes the contrast and brightness in the image.

   Not supported on compressed images.

.. method:: image.find_blobs(thresholds, invert=False, roi=Auto, feature_filter=Auto)

   Finds all "blobs" (connected pixel regions that pass a threshold test) in the
   image and returns a list of them along with basic features about each one.

   Each blob returned is a tuple with the following fields:

     - [0]: x position of bounding rectangle
     - [1]: y position of bounding rectangle
     - [2]: width of bounding rectangle
     - [3]: height of bounding rectangle
     - [4]: number of pixels that passed the threshold test in the blob
     - [5]: x position center of mass
     - [6]: y position center of mass
     - [7]: angle of rotation in radians (is float - others are ints)
     - [8]: count of blobs that make of this blob (see ``find_markers()``)
     - [9]: 1 << (index of threshold tuple used to find this blob) - this is the color code which lets you know what threshold tuple produced this blob.

   This method returns a list of the above tuples. If no blobs are found the
   returned list is empty.

   For grayscale images ``thresholds`` is a list of (lower, upper) grayscale
   pixel thresholds to threshold the image by.

   For RGB images ``thresholds`` is a list of (l_lo, l_hi, a_lo, a_hi, b_lo, b_hi)
   LAB pixel thresholds to threshold the image by.

   Lo/Hi thresholds being swapped is automatically handled.

   ``invert`` inverts the threshold boundaries.

   ``roi`` is the region-of-interest rectangle (x, y, w, h). If not specified,
   it is equal to the image rectangle.

   ``feature_filter`` is a python function which is passed the image object and
   the blob object and should return True (to keep the blob) or False (to throw
   away the blob). If no ``feature_filter`` is specified this method will
   automatically filter out blobs less than 1/1000th of the image pixels.

   Not supported on compressed images.

   .. note:: Yes, you can call image methods on the image object passed to
             the ``feature_filter`` function. However, your OpenMV Cam does not
             have an infinite amount of memory so don't abuse the feature...

.. method:: image.find_markers(blobs, margin=2, feature_filter=Auto)

   After using ``find_blobs`` to find all blobs in an image you can use this
   method to merge blobs that overlap by ``margin`` pixels. This method then
   returns the new list of merged blobs.

   Merged blob tuples have the following fields:

     - [0]: x position of bounding rectangle (surrounds merged blobs)
     - [1]: y position of bounding rectangle (surrounds merged blobs)
     - [2]: width of bounding rectangle (surrounds merged blobs)
     - [3]: height of bounding rectangle (surrounds merged blobs)
     - [4]: number of pixels that passed the threshold test for all blobs merged
     - [5]: x position center of mass of merged blobs
     - [6]: y position center of mass of merged blobs
     - [7]: angle of rotation in radians of merged blobs (is float - others are ints)
     - [8]: count of blobs that make of this blob
     - [9]: logical OR of all the color codes of merged blobs

   This method is called find_markers because it can find color markers in the
   image which are objects painted with two slightly interlocked colors.

   ``margin`` specifies how many pixels to grow the blob rectangles by before
   the intersection test between two blobs to check if they overlap.

   ``feature_filter`` is a python function which is passed the image object and
   the merged blob object and should return True (to keep the blob) or False (to
   throw away the blob). If no ``feature_filter`` is specified this method will
   not filter out any blobs.

   Not supported on compressed images.

   .. note:: Yes, you can call image methods on the image object passed to
             the ``feature_filter`` function. However, your OpenMV Cam does not
             have an infinite amount of memory so don't abuse the feature...

.. method:: image.find_features(cascade, threshold=0.5, scale=1.5)

   This method searches the image for all areas that match the passed in Haar
   Cascade and returns a list of bounding box rectangles around those features.
   Returns an empty list if no features are found.

   ``cascade`` is a Haar Cascade object. See ``image.HaarCascade()`` for more
   details.

   ``threshold`` is a threshold (0.0-1.0) where a smaller value increase the
   detection rate while raising the false positive rate. Conversely, a higher
   value decreases the detection rate while lowering the false positive rate.

   ``scale`` is a scale factor which changes the size of features that can be
   detected. A scale smaller than 1.0 detects smaller objects while a scale
   greater than 1.0 will detect larger objects.

   Not supported on compressed images.

.. method:: image.find_eye((x, y, w, h))

   Searches for the pupil in a region-of-interest around an eye. Returns a tuple
   with the (x, y) location of the pupil in the image. Returns (0,0) if no pupils
   are found.

   To use this function first use ``image.find_features`` with the frontalface
   HaarCascade to find someone's face. Then use ``image.find_features`` with the
   eye HaarCascade to find the eyes on the face. Finally, call this method on
   each eye roi returned by ``image.find_features`` to get the pupil coordinates.

   Not supported on compressed images.

.. method:: image.find_template(template, threshold)

   Tries to find the first location in the image where template matches using
   Normalized Cross Correlation. Returns a bounding box tuple (x, y, w, h) for
   the matching location.

   ``template`` is a small image object that is matched against this image object.

   ``threshold`` is floating point number (0.0-1.0) where a higher threshold prevents
   false positives while lowering the detection rate while a lower threshold does
   the opposite.

   .. note:: This method needs to be reworked and will change in future
             OpenMV Cams. It's not powerful/usable right now. We ran out of
             time to rework this method before release.

   Not supported on compressed images.

.. method:: image.find_lbp((x, y, w, h))

   Extracts a local binary patterns descriptor from the region-of-interest
   tuple. You can use then use the descriptor matching functions to match
   the LBP descriptor against a known descriptor.

   .. note:: LBP descriptors suck right now compared to FAST/FREAK descriptors.
             We will be re-working support for them to make them much better.

.. method:: image.find_keypoints(roi=Auto, threshold=32, normalized=False)

   Extracts FAST/FREAK keypoints from the region-of-interest tuple. You can use
   then use the descriptor matching functions to match the FAST/FREAK descriptor
   against a known descriptor.

   ``roi`` is the region-of-interest rectangle (x, y, w, h). If not specified,
   it is equal to the image rectangle.

   ``threshold`` is a number between (unbounded) which control how many keypoints
   to extract. A larger threshold results in more keypoints being extracted from
   the region-of-interest. More keypoints == more memory == you run out of RAM.

   ``normalized`` if True makes the keypoints not rotation invariant.

   .. note:: This method also needs some more re-working. It's usable right now...
             but, can sometimes use up all the RAM out of nowhere causing the script
             to crash if the scene becomes too complex (lots of edges)... Of course,
             this method is still cool because it can learn descriptors for generic
             objects on the fly.

   Not supported on compressed images.

Constants
---------

.. data:: image.LBP

   Switch for descriptor function to use LBP code.

.. data:: image.FREAK

   Switch for descriptor function to use FREAK code.
