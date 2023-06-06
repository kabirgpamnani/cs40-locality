/*
 *     ppmtrans.c
 *     by Kabir Pamnani and Alex Shriver, 02/22/2023
 *     HW3: Locality
 *
 *     Summary: Implementation of ppmtrans, a program which performs simple
 *              image transformations on ppm image files.
 *               
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "assert.h"
#include "a2methods.h"
#include "a2plain.h"
#include "a2blocked.h"
#include "pnm.h"
#include "cputiming.h"


void transform_image(A2Methods_mapfun *map, Pnm_ppm new_image, 
                     Pnm_ppm og_image, int width, int height, 
                     A2Methods_applyfun apply, A2Methods_T methods);

/**************************************************
 *******  Transformation Apply Functions  *********
 **************************************************/
void rotate_ninety(int col, int row, A2Methods_UArray2 A2uarray2, 
                                                        void *elem, void *cl);
void rotate_one_eighty(int col, int row, A2Methods_UArray2 A2uarray2, 
                                                        void *elem, void *cl);
void rotate_two_seventy(int col, int row, A2Methods_UArray2 A2uarray2, 
                                                        void *elem, void *cl);
void flip_horizontal(int col, int row, A2Methods_UArray2 A2uarray2, 
                                                        void *elem, void *cl);
void flip_vertical(int col, int row, A2Methods_UArray2 A2uarray2, 
                                                        void *elem, void *cl);
void transpose(int col, int row, A2Methods_UArray2 A2uarray2, 
                                                        void *elem, void *cl);

/************************************
 *******  Timing Functions  *********
 ************************************/

CPUTime_T start_timer();

void stop_timer(CPUTime_T timer, FILE *time_file, A2Methods_T methods, 
                                                        Pnm_ppm og_image);

#define SET_METHODS(METHODS, MAP, WHAT) do {                    \
        methods = (METHODS);                                    \
        assert(methods != NULL);                                \
        map = methods->MAP;                                     \
        if (map == NULL) {                                      \
                fprintf(stderr, "%s does not support "          \
                                WHAT "mapping\n",               \
                                argv[0]);                       \
                exit(1);                                        \
        }                                                       \
} while (false)

/*
 * closure struct used by transformation apply functions.
 * Elements:
 *      A2Methods_T method_suite:   The methods suite to use on the 
 *                      A2Methods_UArray2 instances within the apply function
 *      A2Methods_UArray2 uarray2:  The UArray2_T or UArray2b_T to hold the 
 *                      transformed image.
 */
struct closure {
        A2Methods_T method_suite;
        A2Methods_UArray2 uarray2;
};

/* Global constants used to identify user commanded transformations */
#define HORIZONTAL -1
#define VERTICAL -2
#define TRANSPOSE -3

static void usage(const char *progname)
{
        fprintf(stderr, "Usage: %s [-rotate <angle>] "
                        "[-{row,col,block}-major] [filename]\n",
                        progname);
        exit(1);
}

int main(int argc, char *argv[]) 
{
        char *time_file_name = NULL;
        int   rotation       = 0;
        int   i;
        FILE *input_stream = NULL;

        /* default to UArray2 methods */
        A2Methods_T methods = uarray2_methods_plain; 
        assert(methods);

        /* default to best map */
        A2Methods_mapfun *map = methods->map_default; 
        assert(map);

        for (i = 1; i < argc; i++) {
                if (strcmp(argv[i], "-row-major") == 0) {
                        SET_METHODS(uarray2_methods_plain, map_row_major, 
                                    "row-major");
                } else if (strcmp(argv[i], "-col-major") == 0) {
                        SET_METHODS(uarray2_methods_plain, map_col_major, 
                                    "column-major");
                } else if (strcmp(argv[i], "-block-major") == 0) {
                        SET_METHODS(uarray2_methods_blocked, map_block_major,
                                    "block-major");
                } else if (strcmp(argv[i], "-rotate") == 0) {
                        if (!(i + 1 < argc)) {      /* no rotate value */
                                usage(argv[0]);
                        }
                        char *endptr;
                        rotation = strtol(argv[++i], &endptr, 10);
                        if (!(rotation == 0 || rotation == 90 ||
                            rotation == 180 || rotation == 270)) {
                                fprintf(stderr, 
                                        "Rotation must be 0, 90 180 or 270\n");
                                usage(argv[0]);
                        }
                        if (!(*endptr == '\0')) {    /* Not a number */
                                usage(argv[0]);
                        }
                } else if (strcmp(argv[i], "-time") == 0) {
                        time_file_name = argv[++i];
                } else if (strcmp(argv[i], "-flip") == 0) {
                        i++;
                        if (strcmp(argv[i], "horizontal") == 0) {
                                rotation = HORIZONTAL;
                        } else if (strcmp(argv[i], "vertical") == 0) {
                                rotation = VERTICAL;
                        } else {   /* Not a possible flip */
                                fprintf(stderr, "%s: unknown option '%s'\n", 
                                                        argv[0], argv[i++]);
                                usage(argv[0]);
                        }
                } else if (strcmp(argv[i], "-transpose") == 0) {
                        rotation = TRANSPOSE;
                } else if (*argv[i] == '-') {
                        fprintf(stderr, "%s: unknown option '%s'\n", argv[0],
                                argv[i]);
                        usage(argv[0]);
                } else if (argc - i > 1) {
                        fprintf(stderr, "Too many arguments\n");
                        usage(argv[0]);
                } else {
                        break;
                }
        }

        if (i < argc) {
                input_stream = fopen(argv[i], "r");
        } else {
                input_stream = stdin;
        }
        if (input_stream == NULL) {
                fprintf(stderr, "file: %s could not be opened. Terminating.\n",
                                                                argv[i]);
                return EXIT_FAILURE;
        }

        /* Instantiates all potentially necessary objects */
        Pnm_ppm og_image = Pnm_ppmread(input_stream, methods);
        Pnm_ppm new_image = malloc(sizeof(struct Pnm_ppm));
        CPUTime_T timer = NULL;
        FILE *time_file = NULL;
        
        if (time_file_name != NULL) {
                time_file = fopen(time_file_name, "w");
                assert(time_file != NULL);
                timer = start_timer();
        }

        /* Performs the commanded transformation */
        if (rotation == 90) {
                transform_image(map, new_image, og_image, og_image->height, 
                                og_image->width, rotate_ninety, methods);
        } else if (rotation == 180) {
                transform_image(map, new_image, og_image, og_image->width, 
                                og_image->height, rotate_one_eighty, methods);
        } else if (rotation == 270) {
                transform_image(map, new_image, og_image, og_image->height, 
                                og_image->width, rotate_two_seventy, methods);
        } else if (rotation == HORIZONTAL) {
                transform_image(map, new_image, og_image, og_image->width, 
                                og_image->height, flip_horizontal, methods);
        } else if (rotation == VERTICAL) {
                transform_image(map, new_image, og_image, og_image->width, 
                                og_image->height, flip_vertical, methods);
        } else if (rotation == TRANSPOSE) {
                transform_image(map, new_image, og_image, og_image->height, 
                                og_image->width, transpose, methods);
        }

        /* writes the transformed image to stdout */
        if (rotation == 0) {
                Pnm_ppmwrite(stdout, og_image);
                free(new_image);
        } else {
                Pnm_ppmwrite(stdout, new_image);
                Pnm_ppmfree(&new_image);
        }

        if (time_file != NULL) {
                stop_timer(timer, time_file, methods, og_image);
        }
        
        Pnm_ppmfree(&og_image);
        fclose(input_stream);

        return EXIT_SUCCESS;
}


/**********transform_image********
 *
 * Creates a new A2Methods_UArray2 instance to hold the transformed image. 
 * Calls the mapping function with the argued transformation apply function to 
 * populate this new A2Methods_UArray2 with the transformed image. 
 * Initializes the elements of the new_image Pnm_ppm struct with the argued
 * dimensions and the newly populated A2Methods_UArray2 (new_uarray2).
 * 
 * Inputs:
 *              A2Methods_mapfun *map: The mapping function used to map each
 *                      pixel in the original image to transform to the new
 *                      image
 *              Pnm_ppm new_image: The Pnm_ppm struct representing the 
 *                      transformed image.
 *              Pnm_ppm og_image: The original image is represented by this 
 *                      struct. Its pixels will be mapped and transformed to 
 *                      the new_image.
 *              int width: The width of the new image (number of pixels in the 
 *                      transformed image's rows)
 *              int height: The height of the new image (number of pixels in
 *                      the transformed image's columns)
 *              A2Methods_applyfun apply: The function to be applied to each 
 *                      pixel in the original image
 *              A2Methods_T methods: The methods suite to perform all 
 *                      manipulations on the involved A2Methods_UArray2 
 *                      instances
 *                      
 * Return: N/A (void function)
 * Expects:
 *      * Expects a og_image Pnm_ppm struct with a populated pixels element.
 *      * Expects defined functions for mapping and applying
 * Notes:
 *      * The client must free heap allocated memory using the free function in
 *              the methods suite at some point
 *      * It is a checked runtime error if any of the following is true:
 *              * width < 0
 *              * height < 0
 *              * map is a nullptr
 *              * apply is a nullptr
 ************************/
void transform_image(A2Methods_mapfun *map, Pnm_ppm new_image, 
                Pnm_ppm og_image, int width, int height, 
                A2Methods_applyfun apply, A2Methods_T methods)
{
        assert(width >= 0);
        assert(height >= 0);
        assert(map != NULL);
        assert(apply != NULL);
        A2Methods_UArray2 new_uarray2 = methods->new(width, height, 
                                                sizeof(struct Pnm_rgb));
        struct closure cl = {methods, new_uarray2};
        map(og_image->pixels, apply, &cl);
        new_image->width = width;
        new_image->height = height;
        new_image->denominator = og_image->denominator;
        new_image->pixels = new_uarray2;
        new_image->methods = methods;
}


/**************************************************
 *******  Transformation Apply Functions  *********
 **************************************************/

/**********rotate_ninety********
 *
 * Rotates each pixel in a given A2Methods_UArray2 90 degrees clockwise
 * Inputs:
 *              int col: the column value of the current position in the 
 *                       A2Methods_UArray2 A2uarray2
 *              int row: the row value of the current position in the 
 *                       A2Methods_UArray2 A2uarray2
 *              A2Methods_UArray2 A2uarray2: An A2Methods_UArray2 
 *                        representing the image to be rotated
 *              void *elem: A pointer to the element in the A2Methods_UArray2 
 *                          at position (col, row)
 *              void *cl: The closing argument of this apply function. In this
 *                          case, this will be a closure struct instance, 
 *                          containing the A2Methods_UArray2 instance holding
 *                          the newly rotated image and the methods suite
 *                          to be used throughout this function.
 * Return: N/A 
 * Expects:
 *      None
 * Notes:
 *      This function is an apply function that will be passed into the map 
 *      function specified by the command line, so that it is called on every
 *      cell in the image
 ************************/ 
void rotate_ninety(int col, int row, A2Methods_UArray2 A2uarray2, void *elem, 
                                                                void *cl)
{
        A2Methods_T methods = ((struct closure *)cl)->method_suite;
        Pnm_rgb rgb = methods->at(((struct closure *)cl)->uarray2, 
                                methods->height(A2uarray2) - row - 1, col);
        *rgb = *(Pnm_rgb)elem;
}

/**********rotate_one_eighty********
 *
 * Rotates each pixel in a given A2Methods_UArray2 180 degrees clockwise
 * Inputs:
 *              int col: the column value of the current position in the 
 *                       A2Methods_UArray2 A2uarray2
 *              int row: the row value of the current position in the 
 *                       A2Methods_UArray2 A2uarray2
 *              A2Methods_UArray2 A2uarray2: An A2Methods_UArray2 
 *                        representing the image to be rotated
 *              void *elem: A pointer to the element in the A2Methods_UArray2 
 *                          at position (col, row)
 *              void *cl: The closing argument of this apply function. In this
 *                          case, this will be a closure struct instance, 
 *                          containing the A2Methods_UArray2 instance holding
 *                          the newly rotated image and the methods suite
 *                          to be used throughout this function.
 * Return: N/A 
 * Expects:
 *      None
 * Notes:
 *      This function is an apply function that will be passed into the map 
 *      function specified by the command line, so that it is called on every
 *      cell in the image
 ************************/
void rotate_one_eighty(int col, int row, A2Methods_UArray2 A2uarray2, 
                                                        void *elem, void *cl)
{
        A2Methods_T methods = ((struct closure *)cl)->method_suite;
        Pnm_rgb rgb = methods->at(((struct closure *)cl)->uarray2, 
                                methods->width(A2uarray2) - col - 1, 
                                methods->height(A2uarray2) - row - 1);
        *rgb = *(Pnm_rgb)elem;
}

/**********rotate_two_seventy********
 *
 * Rotates each pixel in a given A2Methods_UArray2 270 degrees clockwise
 * Inputs:
 *              int col: the column value of the current position in the 
 *                       A2Methods_UArray2 A2uarray2
 *              int row: the row value of the current position in the 
 *                       A2Methods_UArray2 A2uarray2
 *              A2Methods_UArray2 A2uarray2: An A2Methods_UArray2 
 *                        representing the image to be rotated
 *              void *elem: A pointer to the element in the A2Methods_UArray2 
 *                          at position (col, row)
 *              void *cl: The closing argument of this apply function. In this
 *                          case, this will be a closure struct instance, 
 *                          containing the A2Methods_UArray2 instance holding
 *                          the newly rotated image and the methods suite
 *                          to be used throughout this function.
 * Return: N/A 
 * Expects:
 *      None
 * Notes:
 *      This function is an apply function that will be passed into the map 
 *      function specified by the command line, so that it is called on every
 *      cell in the image
 ************************/
void rotate_two_seventy(int col, int row, A2Methods_UArray2 A2uarray2, 
                                                        void *elem, void *cl)
{
        A2Methods_T methods = ((struct closure *)cl)->method_suite;
        Pnm_rgb rgb = methods->at(((struct closure *)cl)->uarray2, row, 
                                        methods->width(A2uarray2) - col - 1);
        *rgb = *(Pnm_rgb)elem;
}

/**********flip_horizontal********
 *
 * Flips each pixel in a given A2Methods_UArray2 horizontally. Thus the 
 * transformed image is a mirror of the original from left to right.
 *
 * Inputs:
 *              int col: the column value of the current position in the 
 *                       A2Methods_UArray2 A2uarray2
 *              int row: the row value of the current position in the 
 *                       A2Methods_UArray2 A2uarray2
 *              A2Methods_UArray2 A2uarray2: An A2Methods_UArray2 
 *                        representing the image to be flipped
 *              void *elem: A pointer to the element in the A2Methods_UArray2 
 *                          at position (col, row)
 *              void *cl: The closing argument of this apply function. In this
 *                          case, this will be a closure struct instance, 
 *                          containing the A2Methods_UArray2 instance holding
 *                          the newly flipped image and the methods suite
 *                          to be used throughout this function.
 * Return: N/A 
 * Expects:
 *      None
 * Notes:
 *      This function is an apply function that will be passed into the map 
 *      function specified by the command line, so that it is called on every
 *      cell in the image
 ************************/ 
void flip_horizontal(int col, int row, A2Methods_UArray2 A2uarray2, 
                                                        void *elem, void *cl)
{
        A2Methods_T methods = ((struct closure *)cl)->method_suite;
        Pnm_rgb rgb = methods->at(((struct closure *)cl)->uarray2, 
                                methods->width(A2uarray2) - col - 1, row);
        *rgb = *(Pnm_rgb)elem;
}

/**********flip_vertical********
 *
 * Flips each pixel in a given A2Methods_UArray2 vertically. Thus the 
 * transformed image is a mirror of the original from top to bottom.
 * Inputs:
 *              int col: the column value of the current position in the 
 *                       A2Methods_UArray2 A2uarray2
 *              int row: the row value of the current position in the 
 *                       A2Methods_UArray2 A2uarray2
 *              A2Methods_UArray2 A2uarray2: An A2Methods_UArray2 
 *                        representing the image to be rotated
 *              void *elem: A pointer to the element in the A2Methods_UArray2 
 *                          at position (col, row)
 *              void *cl: The closing argument of this apply function. In this
 *                          case, this will be a closure struct instance, 
 *                          containing the A2Methods_UArray2 instance holding
 *                          the newly rotated image and the methods suite
 *                          to be used throughout this function.
 * Return: N/A 
 * Expects:
 *      None
 * Notes:
 *      This function is an apply function that will be passed into the map 
 *      function specified by the command line, so that it is called on every
 *      cell in the image
 ************************/
void flip_vertical(int col, int row, A2Methods_UArray2 A2uarray2, 
                                                        void *elem, void *cl)
{
        A2Methods_T methods = ((struct closure *)cl)->method_suite;
        Pnm_rgb rgb = methods->at(((struct closure *)cl)->uarray2, col, 
                                        methods->height(A2uarray2) - row - 1);
        *rgb = *(Pnm_rgb)elem;
}

/**********transpose********
 *
 * Transposes each image in a given A2Methods_UArray2. Thus the image is 
 * transposed across the UL-to-LR axis.
 * Inputs:
 *              int col: the column value of the current position in the 
 *                       A2Methods_UArray2 A2uarray2
 *              int row: the row value of the current position in the 
 *                       A2Methods_UArray2 A2uarray2
 *              A2Methods_UArray2 A2uarray2: An A2Methods_UArray2 
 *                        representing the image to be transposed
 *              void *elem: A pointer to the element in the A2Methods_UArray2 
 *                          at position (col, row)
 *              void *cl: The closing argument of this apply function. In this
 *                          case, this will be a closure struct instance, 
 *                          containing the A2Methods_UArray2 instance holding
 *                          the newly transposed image and the methods suite
 *                          to be used throughout this function.
 * Return: N/A 
 * Expects:
 *      None
 * Notes:
 *      This function is an apply function that will be passed into the map 
 *      function specified by the command line, so that it is called on every
 *      cell in the image
 ************************/
void transpose(int col, int row, A2Methods_UArray2 A2uarray2, 
                                                        void *elem, void *cl)
{
        A2Methods_T methods = ((struct closure *)cl)->method_suite;
        Pnm_rgb rgb = methods->at(((struct closure *)cl)->uarray2, row, col);
        *rgb = *(Pnm_rgb)elem;
        (void) A2uarray2;
}


/************************************
 *******  Timing Functions  *********
 ************************************/

/**********start_timer********
 *
 * Allocates, initializes and returns a new CPUTime_T instance so that it can
 * be used to store a time value
 * Inputs:
 *              None
 * Return: A new CPUTime_T instance that will be used to keep track of the time
 * Expects:
 *      * N/A
 * Notes:
 *      * The client must free heap allocated memory using CPUTime_Free
 *      * This function also calls CPUTime_Start on the newly created instance
 *      to begin the timer
 ************************/
CPUTime_T start_timer() 
{
        CPUTime_T timer = CPUTime_New();
        CPUTime_Start(timer);
        return timer;
}

/**********stop_timer********
 *
 * Stops the timer and prints information including the total time taken for 
 * the operation, the total number of pixels in the image, and the time taken
 * per pixel.
 *  
 * Inputs:
 *              CPUTime_T timer: A timer instance that is keeping track of the
 *                               the time taken to complete the operation
 *              File *time_file: A file pointer to a time file that the timing
 *                               data will be written to
 *              A2Methods_T methods: A method suite to manipulate the original
 *                                   image
 *              Pnm_ppm og_image: A Pnm_ppm struct that holds the original 
 *                                image
 * Return: N/A 
 * Expects:
 *      * width and height to be nonnegative
 *      * size to be positive
 *      * blocksize to be positive
 * Notes:
 *      * The time file that holds the data is closed after the data is written 
 *      to it
 *      * The timer (of type CPUTime_T) is freed in this function using 
 *      CPUTime_Free
 ************************/
void stop_timer(CPUTime_T timer, FILE *time_file, A2Methods_T methods, 
                                                        Pnm_ppm og_image)
{
        double time = CPUTime_Stop(timer);
        fprintf(time_file, "It took: %lf nanoseconds in total\n", time);
        int num_pixels = methods->height(og_image->pixels) * 
                                        methods->width(og_image->pixels);
        fprintf(time_file, "Number of pixels: %i\n", num_pixels);
        double tpp = time / num_pixels;
        fprintf(time_file, "Time per pixel: %f nanoseconds\n", tpp);
        fclose(time_file);
        CPUTime_Free(&timer);
}