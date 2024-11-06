/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include <stdio.h>

#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
  if (M == 64 && N == 64) {
    int i, j, k;
    int a0, a1, a2, a3, a4, a5, a6, a7;
    for (i = 0; i < N; i += 8) {
      for (j = 0; j < M; j += 8) {
        for (k = i; k < i + 4; k++) {
          a0 = A[k][j];
          a1 = A[k][j + 1];
          a2 = A[k][j + 2];
          a3 = A[k][j + 3];
          a4 = A[k][j + 4];
          a5 = A[k][j + 5];
          a6 = A[k][j + 6];
          a7 = A[k][j + 7];

          B[j][k] = a0;
          B[j + 1][k] = a1;
          B[j + 2][k] = a2;
          B[j + 3][k] = a3;

          B[j][k + 4] = a4;
          B[j + 1][k + 4] = a5;
          B[j + 2][k + 4] = a6;
          B[j + 3][k + 4] = a7;
        }

        for (k = j; k < j + 4; k++) {
          a0 = B[k][i + 4];
          a1 = B[k][i + 5];
          a2 = B[k][i + 6];
          a3 = B[k][i + 7];

          a4 = A[i + 4][k];
          a5 = A[i + 5][k];
          a6 = A[i + 6][k];
          a7 = A[i + 7][k];

          B[k][i + 4] = a4;
          B[k][i + 5] = a5;
          B[k][i + 6] = a6;
          B[k][i + 7] = a7;

          B[k + 4][i] = a0;
          B[k + 4][i + 1] = a1;
          B[k + 4][i + 2] = a2;
          B[k + 4][i + 3] = a3;
        }

        for (k = i + 4; k < i + 8; k++) {
          a4 = A[k][j + 4];
          a5 = A[k][j + 5];
          a6 = A[k][j + 6];
          a7 = A[k][j + 7];

          B[j + 4][k] = a4;
          B[j + 5][k] = a5;
          B[j + 6][k] = a6;
          B[j + 7][k] = a7;
        }
      }
    }
    return;
  }
  if (M == 32 && N == 32) {
    int i, j, k, l;
    int a0, a1, a2, a3, a4, a5, a6, a7;
    for (i = 0; i < N; i += 8) {
      for (j = 0; j < M; j += 8) {
        for (k = 0; k < 8; ++k) {
          a0 = A[i + k][j];
          a1 = A[i + k][j + 1];
          a2 = A[i + k][j + 2];
          a3 = A[i + k][j + 3];
          a4 = A[i + k][j + 4];
          a5 = A[i + k][j + 5];
          a6 = A[i + k][j + 6];
          a7 = A[i + k][j + 7];
          B[j + k][i] = a0;
          B[j + k][i + 1] = a1;
          B[j + k][i + 2] = a2;
          B[j + k][i + 3] = a3;
          B[j + k][i + 4] = a4;
          B[j + k][i + 5] = a5;
          B[j + k][i + 6] = a6;
          B[j + k][i + 7] = a7;
        }
        for (k = 0; k < 8; ++k) {
          for (l = 0; l < k; ++l) {
            a0 = B[j + k][i + l];
            B[j + k][i + l] = B[j + l][i + k];
            B[j + l][i + k] = a0;
          }
        }
      }
    }
    return;
  }

  if (M == 61 && N == 67) {
    int i, j, k;
    int a0, a1, a2, a3, a4, a5, a6, a7;

    // Main part: handle complete 24x24 blocks
    for (i = 0; i < 48; i += 24) {
      for (j = 0; j < 48; j += 24) {
        for (k = i; k < i + 24; k++) {
          // First 8 elements
          a0 = A[k][j];
          a1 = A[k][j + 1];
          a2 = A[k][j + 2];
          a3 = A[k][j + 3];
          a4 = A[k][j + 4];
          a5 = A[k][j + 5];
          a6 = A[k][j + 6];
          a7 = A[k][j + 7];
          B[j][k] = a0;
          B[j + 1][k] = a1;
          B[j + 2][k] = a2;
          B[j + 3][k] = a3;
          B[j + 4][k] = a4;
          B[j + 5][k] = a5;
          B[j + 6][k] = a6;
          B[j + 7][k] = a7;

          // Second 8 elements
          a0 = A[k][j + 8];
          a1 = A[k][j + 9];
          a2 = A[k][j + 10];
          a3 = A[k][j + 11];
          a4 = A[k][j + 12];
          a5 = A[k][j + 13];
          a6 = A[k][j + 14];
          a7 = A[k][j + 15];
          B[j + 8][k] = a0;
          B[j + 9][k] = a1;
          B[j + 10][k] = a2;
          B[j + 11][k] = a3;
          B[j + 12][k] = a4;
          B[j + 13][k] = a5;
          B[j + 14][k] = a6;
          B[j + 15][k] = a7;

          // Third 8 elements
          a0 = A[k][j + 16];
          a1 = A[k][j + 17];
          a2 = A[k][j + 18];
          a3 = A[k][j + 19];
          a4 = A[k][j + 20];
          a5 = A[k][j + 21];
          a6 = A[k][j + 22];
          a7 = A[k][j + 23];
          B[j + 16][k] = a0;
          B[j + 17][k] = a1;
          B[j + 18][k] = a2;
          B[j + 19][k] = a3;
          B[j + 20][k] = a4;
          B[j + 21][k] = a5;
          B[j + 22][k] = a6;
          B[j + 23][k] = a7;
        }
      }
    }

    // Handle remaining rows (48-67) in steps of 8 and 4
    for (i = 48; i < 64; i += 8) {  // Step 8 until 64
      for (j = 0; j < M; j += 8) {
        for (k = i; k < i + 8 && k < N; k++) {
          a0 = A[k][j];
          a1 = A[k][j + 1];
          a2 = A[k][j + 2];
          a3 = A[k][j + 3];
          a4 = A[k][j + 4];
          a5 = A[k][j + 5];
          a6 = A[k][j + 6];
          a7 = (j + 7 < M) ? A[k][j + 7] : 0;
          B[j][k] = a0;
          B[j + 1][k] = a1;
          B[j + 2][k] = a2;
          B[j + 3][k] = a3;
          B[j + 4][k] = a4;
          B[j + 5][k] = a5;
          B[j + 6][k] = a6;
          if (j + 7 < M) B[j + 7][k] = a7;
        }
      }
    }
    // Handle last few rows (64-67) in steps of 4
    for (i = 64; i < N; i += 4) {
      for (j = 0; j < M; j += 8) {
        for (k = i; k < i + 4 && k < N; k++) {
          a0 = A[k][j];
          a1 = A[k][j + 1];
          a2 = A[k][j + 2];
          a3 = A[k][j + 3];
          a4 = A[k][j + 4];
          a5 = A[k][j + 5];
          a6 = A[k][j + 6];
          a7 = (j + 7 < M) ? A[k][j + 7] : 0;
          B[j][k] = a0;
          B[j + 1][k] = a1;
          B[j + 2][k] = a2;
          B[j + 3][k] = a3;
          B[j + 4][k] = a4;
          B[j + 5][k] = a5;
          B[j + 6][k] = a6;
          if (j + 7 < M) B[j + 7][k] = a7;
        }
      }
    }

    // Handle remaining columns (48-61) in steps of 8 and 4
    for (i = 0; i < 48; i += 8) {
      for (j = 48; j < 56 && j < M; j += 8) {  // Step 8 until 56
        for (k = i; k < i + 8; k++) {
          a0 = A[k][j];
          a1 = A[k][j + 1];
          a2 = A[k][j + 2];
          a3 = A[k][j + 3];
          a4 = A[k][j + 4];
          a5 = A[k][j + 5];
          a6 = A[k][j + 6];
          a7 = (j + 7 < M) ? A[k][j + 7] : 0;
          B[j][k] = a0;
          B[j + 1][k] = a1;
          B[j + 2][k] = a2;
          B[j + 3][k] = a3;
          B[j + 4][k] = a4;
          B[j + 5][k] = a5;
          B[j + 6][k] = a6;
          if (j + 7 < M) B[j + 7][k] = a7;
        }
      }
      // Handle last few columns (56-61) in steps of 4
      for (j = 56; j < M; j += 4) {
        for (k = i; k < i + 8; k++) {
          a0 = A[k][j];
          a1 = A[k][j + 1];
          a2 = A[k][j + 2];
          a3 = (j + 3 < M) ? A[k][j + 3] : 0;
          B[j][k] = a0;
          B[j + 1][k] = a1;
          B[j + 2][k] = a2;
          if (j + 3 < M) B[j + 3][k] = a3;
        }
      }
    }

    return;
  }
}

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

/*
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N]) {
  int i, j, tmp;

  for (i = 0; i < N; i++) {
    for (j = 0; j < M; j++) {
      tmp = A[i][j];
      B[j][i] = tmp;
    }
  }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions() {
  /* Register your solution function */
  registerTransFunction(transpose_submit, transpose_submit_desc);

  /* Register any additional transpose functions */
  registerTransFunction(trans, trans_desc);
}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N]) {
  int i, j;

  for (i = 0; i < N; i++) {
    for (j = 0; j < M; ++j) {
      if (A[i][j] != B[j][i]) {
        return 0;
      }
    }
  }
  return 1;
}
