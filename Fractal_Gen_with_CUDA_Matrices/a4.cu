/*
 * a4.cu
 * Assignment 4 - COSC330
 * Author: Christian Caburian
 *
 * Implements a parallel multithreaded version of the Mandelbrot set
 * program in the CUDA environment.
 * User can specify width and height of the image.
 *
 * Usage: a4 <width> <height>
 *
 * Compile: make
 *
 */

#include <stdio.h>
#include <cuda_runtime.h>
#include "bmpfile.h"
/**
  * Parses console arguments
  * argc: no. of args
  * argv: array of args
  * width and height: locations to store width and height of the image
  */
__host__ int
parse_args(int argc,  char *argv[], double *width, double *height) 
{
    if ((argc != 3) ||
        ((*width = atoi(argv[1])) <= 0) ||
        ((*height = atoi(argv[2])) <= 0)) {

        fprintf(stderr, "Usage: %s width height\n", argv[0]);
        return(-1);
    }
    return 0;
}
/**
  * Copies memory between host and device
  * src: source to be copied
  * dst: destination to copy to
  * size: size of memory to be copied
  * kind: copy direction; from host to device or vice versa
  * err: location of cudaError_t variable
  * msg: announces operation; reports error
  */
__host__ void
cudaMemcp_check_err(double *src,
                    double *dst,
                    size_t size,
                    enum cudaMemcpyKind kind,
                    cudaError_t *err,
                    const char *msg)
{
    printf("Copying %s\n", msg);
    *err = cudaMemcpy(src, dst, size, kind);
    if (*err != cudaSuccess) {
        fprintf(stderr, "Failed to copy %s (error code %s)!\n",
                msg, cudaGetErrorString(*err));
        exit(EXIT_FAILURE);
    }
}
/**
  * Frees CUDA memory, with error checking
  * dev_mat: device matrix to free
  * err: location of cudaError_t variable
  */
__host__ void
cudaFree_check_err(double *dev_mat, cudaError_t *err)
{
    *err = cudaFree(dev_mat);
    if (*err != cudaSuccess) {
        fprintf(stderr, "Failed to free device vector A (error code %s)!\n",
                cudaGetErrorString(*err));
        exit(EXIT_FAILURE);
    }
} 
/**
  * Initializes the pixel reference matrices for potential (x, y) coordinates.
  * x and y: flattened 2D matrices of potential x and y coordinates in the
  * 		 Mandelbrot set.
  * width and height: dimensions of the image.
  */
__host__ void
pixel_ref(double *x, double *y, double width, double height)
{
    // Mandelbrot values
    double resolution = 8700.0;
    double xcenter = -0.55;
    double ycenter = 0.60;

    int i, col, row;
    int xoffset = -(width - 1) /2;
    int yoffset = (height -1) / 2;

    for(col = 0, i = 0; col < width; ++col) {
        for(row = 0; row < height; ++row, ++i) {
            x[i] = xcenter + (xoffset + col) / resolution;
            y[i] = ycenter + (yoffset - row) / resolution;
        }
    }
}
/**
  * Computes the Mandelbrot pixel value
  * x and y: flattened 2D matrices of potential x and y coordinates in the
  * 		 Mandelbrot set.
  * pixel_mat: Result/output matrix of Mandelbrot pixel values
  * N: Number of pixels in the image; width * height
  */
__global__ void
mandelbrot(const double *x, const double *y, double *pixel_mat, int N)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i < N) {

        // Colour values
        double colour_max = 240.0;
        double gradient_colour_max = 230.0;

        //Mandelbrot stuff
        double a = 0;
        double b = 0;
        double aold = 0;
        double bold = 0;
        double zmagsqr = 0;
        int iter = 0;
        int max_iter = 1000;
        float iter_ratio;

        //Check if the x,y coordinates are part of the Mandelbrot set
        while(iter < 1000 && zmagsqr <= 4.0) {
        
            ++iter;
            a = (aold * aold) - (bold * bold) + x[i];
            b = 2.0 * aold*bold + y[i];

            zmagsqr = a*a + b*b;

            aold = a;
            bold = b;
        }

        // Generate the colour of the pixel from iter value
        iter_ratio = (float)iter / (float)max_iter;
        pixel_mat[i] = colour_max - iter_ratio * gradient_colour_max;
    }
}
/**
  * Computes the colour gradient
  * colour: the output vector
  * x: the gradient (beetween 0 and 360)
  * min and max: variation of the RGB channels (Move3D 0 -> 1)
  * Check wiki for more details on the colour science: en.wikipedia.org/wiki/HSL_and_HSV
  */
__host__ void
ground_colour_mix(double* colour, double x, double min, double max)
{
 /*
  * Red = colour[0]
  * Green = colour[1]
  * Blue = colour[2]
  */
    double posSlope = (max-min)/60;
    double negSlope = (min-max)/60;

    if( x < 60 ) {
        colour[0] = max;
        colour[1] = posSlope*x+min;
        colour[2] = min;
        return;
    }
    else if ( x < 120 ) {
        colour[0] = negSlope*x+2.0*max+min;
        colour[1] = max;
        colour[2] = min;
        return;
    }
    else if ( x < 180  ) {
        colour[0] = min;
        colour[1] = max;
        colour[2] = posSlope*x-2.0*max+min;
        return;
    }
    else if ( x < 240  ) {
        colour[0] = min;
        colour[1] = negSlope*x+4.0*max+min;
        colour[2] = max;
        return;
    }
    else if ( x < 300  ) {
        colour[0] = posSlope*x-4.0*max+min;
        colour[1] = min;
        colour[2] = max;
        return;
    }
    else {
        colour[0] = max;
        colour[1] = min;
        colour[2] = negSlope*x+6*max;
        return;
    }
}
/**
  * Converts a matrix of pixels to a BMP file
  * pixel_mat: matrix of pixels.
  * width and height: dimensions of the image.
  */
__host__ void
pixel_mat_to_bmp(double *pixel_mat, double width, double height)
{
    const char *filename = "my_mandelbrot_fractal.bmp";
    bmpfile_t *bmp;
    rgb_pixel_t pixel = {0, 0, 0, 0};
    double colour[3];
    double colour_depth = 255.0;

    // Create BMP
    printf("Writing result to BMP file...\n");
    if ((bmp = bmp_create(width, height, 32)) == NULL) {
        fprintf(stderr, "Failed to create BMP!\n");
        exit(EXIT_FAILURE);
    }

    // Convert pixel matrix to BMP file
    int col, row;
    for(col = 0; col < width; ++col){
        for(row = 0; row < height; ++row){

            ground_colour_mix(colour,
                              pixel_mat[col*(int)height+row],
                              1,
                              colour_depth);
                              
            pixel.red = colour[0];
            pixel.green = colour[1];
            pixel.blue = colour[2];
            bmp_set_pixel(bmp, col, row, pixel);
        }
    }

    // Save BMP file
    if (!bmp_save(bmp, filename)) {
        fprintf(stderr, "Failed to save BMP!\n");
        exit(EXIT_FAILURE);
    }
    bmp_destroy(bmp);
    printf("File saved.\n");
}
/* Mandelbrot Set Image Demonstration
 *
 * This is a simple parallel multithread implementation
 * that computes a Mandelbrot set and produces a corresponding
 * Bitmap image. The program demonstrates the use of a colour
 * gradient
 *
 * This program uses the algorithm outlined in:
 *   "Building Parallel Programs: SMPs, Clusters And Java", Alan Kaminsky
 *
 * This program requires libbmp for all bitmap operations.
 *
 */
int
main(int argc, char *argv[])
{
    // Error code to check return values for CUDA calls
    cudaError_t err = cudaSuccess;

    // Image size to be used
    double width, height;
    if (parse_args(argc, argv, &width, &height) < 0) exit(EXIT_FAILURE);
    printf("Width: %.01f, Height: %.01f\n", width, height);
    int N = (int)width*(int)height;
    size_t size = N * sizeof(double);

    // Allocate memory and verify the allocations succeeded
    // Host matrices
    double *x = (double *)malloc(size);
    double *y = (double *)malloc(size);
    double *pixel_mat = (double *)malloc(size);
    if (pixel_mat == NULL || x == NULL || y == NULL) {
        fprintf(stderr, "Failed to allocate host pixel_mat!\n");
        exit(EXIT_FAILURE);
    }
    // Device matrices
    double *d_x = NULL;
    err = cudaMalloc((void **)&d_x, size);
    if (err != cudaSuccess) {
        fprintf(stderr, "Failed to allocate device x (error code %s)!\n", 
                cudaGetErrorString(err));
        exit(EXIT_FAILURE);
    }
    double *d_y = NULL;
    err = cudaMalloc((void **)&d_y, size);
    if (err != cudaSuccess) {
        fprintf(stderr, "Failed to allocate device y (error code %s)!\n", 
                cudaGetErrorString(err));
        exit(EXIT_FAILURE);
    }
    double *d_pixel_mat = NULL;
    err = cudaMalloc((void **)&d_pixel_mat, size);
    if (err != cudaSuccess) {
        fprintf(stderr, "Failed to allocate device pixel_mat (error code %s)!\n", 
                cudaGetErrorString(err));
        exit(EXIT_FAILURE);
    }

    //Determine where in the Mandelbrot set, the pixel is referencing
    pixel_ref(x, y, width, height);

    // Copy the pixel reference host matrices x and y to the device
    cudaMemcp_check_err(d_x, x, size, cudaMemcpyHostToDevice, &err, 
                        "matrix X from host to device...");
    cudaMemcp_check_err(d_y, y, size, cudaMemcpyHostToDevice, &err, 
                        "matrix Y from host to device...");

    // Launch kernel to determine Mandelbrot set
    int threadsPerBlock = 256;
    int blocksPerGrid = (N+threadsPerBlock-1)/threadsPerBlock;
    printf("Calling kernel with threadsPerBlock = %d, blocksPerGrid = %d...\n",
            threadsPerBlock, blocksPerGrid);
    mandelbrot<<<blocksPerGrid, threadsPerBlock>>>(d_x, d_y, d_pixel_mat, N);
    err = cudaGetLastError();
    if (err != cudaSuccess) {
        fprintf(stderr, "Failed to launch Mandelbrot kernel (error code %s)!\n", 
                cudaGetErrorString(err));
        exit(EXIT_FAILURE);
    }

    // Copy the device result matrix in device memory to the host result matrix
    // in host memory.
    cudaMemcp_check_err(pixel_mat, d_pixel_mat, size, 
                        cudaMemcpyDeviceToHost, &err, 
                        "result from device to host...");

    // Write the pixel_mat to a BMP file
    pixel_mat_to_bmp(pixel_mat, width, height);

    // Free device global memory
    cudaFree_check_err(d_x, &err);
    cudaFree_check_err(d_y, &err);
    cudaFree_check_err(d_pixel_mat, &err);

    // Free host memory
    free(pixel_mat);
    free(x);
    free(y);

    // Reset the device and exit
    err = cudaDeviceReset();
    if (err != cudaSuccess) {
        fprintf(stderr, "Failed to deinitialize the device! error=%s\n", 
                cudaGetErrorString(err));
        exit(EXIT_FAILURE);
    }
    return 0;
}
