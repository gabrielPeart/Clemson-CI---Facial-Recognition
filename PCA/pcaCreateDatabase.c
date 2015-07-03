
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <stdint.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <time.h>

#include "matrix.h"

int file_select(const struct dirent *entry);

int main (int argc, char **argv) {

	char databasePath[] = "..//test_images//";
	char filenameFilePath[] = "filenamesDB.dat";
	char projectedImagesFilePath[] = "testDB.dat";
	char *path = (char *) malloc (200 * sizeof (char));
	char header[4];
	clock_t start, end;
	uint64_t i;
	uint64_t imgWidth, imgHeight, pixMaxSize;
	uint64_t numImages, numPixels;
	
	/* Get # of images and image names from directory */
	struct dirent **imageList;
    numImages = scandir(databasePath, &imageList, file_select, alphasort);
	if (numImages <= 0) {
        perror("scandir");
		exit (1);
    }
	
	/* Get the size of the images */
	sprintf (path, "%s%s", databasePath, imageList[0]->d_name);
	FILE * sampleImage = fopen (path, "r");
	fscanf (sampleImage, "%s %" PRIu64 " %" PRIu64 " %" PRIu64 "", header, &imgHeight, &imgWidth, &pixMaxSize);
	fclose (sampleImage);
	assert (strcmp (header, "P6") == 0 && pixMaxSize == 255);
	
	/* Calculate number of pixels per image */
	numPixels = imgWidth * imgHeight;
	
	/* Allocate the Image array T */
	matrix_t *T = initializeMatrix (UNDEFINED, numPixels,numImages);
	
	// Load images into the matrix
	unsigned char *pixels = (unsigned char *) malloc (3 * numPixels * sizeof (unsigned char));
	if ( pixels == NULL) {
		printf ("malloc error with pixels\n");
	}
	start = clock();
	for (i = 0; i < numImages; i++) {
		// Load image
		sprintf (path, "%s%s", databasePath, imageList[i]->d_name);
		loadPPMtoMatrixCol (path, T, i, pixels);
	}
	end = clock();
	printf("time to load images, time=%g\n",
            ((double)(end-start))/CLOCKS_PER_SEC);

	// Calculate the mean face
	start = clock();
	matrix_t *m = calcMeanCol (T);
	end = clock();
	printf("time to calc mean face, time=%g\n",
            ((double)(end-start))/CLOCKS_PER_SEC);

	/* Subtract the mean face from the regular images to produce normalized matrix A */
	matrix_t *A = T; // To keep naming conventions
	
	start = clock();
	for (i = 0; i < numImages; i++) {
		subtractMatrixColumn (A, i, m, 0);
	}
	end = clock();
	printf("time to calc A, time=%g\n",
            ((double)(end-start))/CLOCKS_PER_SEC);
	
	/* Calculate the surrogate matrix L */
	/* ----- L = (A')*A ----- */
	start = clock();
	matrix_t *L = calcSurrogateMatrix (A);
	end = clock();
	printf("time to calc surrogate matrix L, time=%g\n",
            ((double)(end-start))/CLOCKS_PER_SEC);

	
	/* Calculate eigenvectors for L */
	start = clock();
	matrix_t *L_eigenvectors = calcEigenvectorsSymmetric (L);
	end = clock();
	printf("time to calc eigenvectors, time=%g\n",
            ((double)(end-start))/CLOCKS_PER_SEC);

	freeMatrix (L);
	
	/* Calculate Eigenfaces */
	/* ----- Eigenfaces = A * L_eigenvectors ----- */
	start = clock();
	matrix_t *eigenfaces = matrixMultiply (A, NOT_TRANSPOSED, L_eigenvectors, NOT_TRANSPOSED, 0);
	end = clock();
	printf("time to calc eigenfaces, time=%g\n",
            ((double)(end-start))/CLOCKS_PER_SEC);

	freeMatrix (L_eigenvectors);

	/* Transpose eigenfaces */
	start = clock();
	matrix_t *transposedEigenfaces = transposeMatrix (eigenfaces);
	end = clock();
	printf("time to transpose eigenfaces, time=%g\n",
            ((double)(end-start))/CLOCKS_PER_SEC);
	freeMatrix (eigenfaces);
	eigenfaces = NULL;

	/* Calculate Projected Images */
	/* ----- ProjectedImages = eigenfaces' * A ----- */
	start = clock();
	matrix_t *projectedImages = matrixMultiply (transposedEigenfaces, NOT_TRANSPOSED, A, NOT_TRANSPOSED, A->numCols);
	end = clock();
	printf("time to calc projectedImages, time=%g\n",
            ((double)(end-start))/CLOCKS_PER_SEC);

	freeMatrix (A);
	A = NULL;

	// Print the Projected Image matrix and the mean image
	FILE * out = fopen (projectedImagesFilePath, "w");
	/*fprintMatrix (out, projectedImages);
	fprintMatrix (out, eigenfaces);
	fprintMatrix (out, m); */
	
	fwriteMatrix (out, projectedImages);
	fwriteMatrix (out, transposedEigenfaces);
	fwriteMatrix (out, m);


	// Write the filenames corresponding to each column
	FILE *filenamesFile = fopen (filenameFilePath, "w");
	for (i = 0; i < numImages; i++) {
		fprintf (filenamesFile, "%s\n", imageList[i]->d_name);
		free (imageList[i]);
	}
	free (imageList);

	// Save the mean image	-> this is more of for fun and to make sure
	//						-> the function I wrote worked
	writePPMgrayscale("meanImage.ppm", m, 0, imgHeight, imgWidth);
	
	/* COMMENT - could move these up to help memory */
	freeMatrix (projectedImages);
	freeMatrix (transposedEigenfaces);
	freeMatrix (m);

	
	fclose (out);
	fclose (filenamesFile);

	free (path);
	free (pixels);

	return 0;
}


int file_select(const struct dirent *entry){
	
    if (strstr(entry->d_name, ".ppm") != NULL) {
        return 1;
    } else {
        return 0;
    }
}