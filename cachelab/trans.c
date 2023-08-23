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
void trans(int M, int N, int A[N][M], int B[M][N]);
void trans_block_32x32(int M, int N, int A[N][M], int B[M][N]);
void trans_block_64x64(int M, int N, int A[N][M], int B[M][N]);
void trans_block8_61x67(int M, int N, int A[N][M], int B[M][N]);
void trans_block16_61x67(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if (M == 32 && N == 32)
        trans_block_32x32(M, N, A, B);
    else if (M == 64 && N == 64)
        trans_block_64x64(M, N, A, B);
    else if (M == 61 && N == 67)
        trans_block16_61x67(M, N, A, B);
    else
        trans(M, N, A, B);
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/* 
 * trans_block8 - Split the matrix into 8x8 blocks.
 */
char trans_block8_desc[] = "8x8 block scan transpose";
void trans_block8(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp, brow, bcol;
    int bsize = 8;
    for (brow = 0; brow < N; brow += bsize)
        for (bcol = 0; bcol < M; bcol += bsize)
        {
            for (i = 0; i < bsize; i++) {
                for (j = 0; j < bsize; j++) {
                    tmp = A[brow + i][bcol + j];
                    B[bcol + j][brow + i] = tmp;
                }
            }
        }
}

/* 
 * trans_blcok_32x32 - Split the matrix into 8x8 blocks. 
 * The blocks at the diagonal should be treated specially.
 * The element at the diagonal should be last copied to B, for one 8-element line in A.
 */
char trans_block_32x32_desc[] = "Block scan transpose for 32x32 matrix";
void trans_block_32x32(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp, brow, bcol;
    int bsize = 8;
    for (brow = 0; brow < N; brow += bsize)
        for (bcol = 0; bcol < M; bcol += bsize)
        {
            if (brow == bcol)
                continue;
            for (i = 0; i < bsize; i++) {
                for (j = 0; j < bsize; j++) {
                    tmp = A[brow + i][bcol + j];
                    B[bcol + j][brow + i] = tmp;
                }
            }
        }
    for (brow = 0; brow < N; brow += bsize)
    {
        for (i = 0; i < bsize; i++) {
            for (j = 0; j < bsize; j++) {
                if (i == j)
                    continue;
                tmp = A[brow + i][brow + j];
                B[brow + j][brow + i] = tmp;
            }
            tmp = A[brow + i][brow + i];
            B[brow + i][brow + i] = tmp;
        }
    }
}

/* 
 * trans_blcok_64x64 - Split the matrix into 8x8 blocks
 * and split the blocks into 4x4.
 * In each 8x8 block, the first four lines will map to the same cache sets as the last four lines.
 * The blocks(8x8) at the diagonal should be treated specially.
 * Use local variables to avoid some cache miss.
 */
char trans_block_64x64_desc[] = "Block scan transpose for 64x64 matrix";
void trans_block_64x64(int M, int N, int A[N][M], int B[M][N])
{
    const int bsize = 8;
    int i, j, brow, bcol;
    int a1, a2, a3, a4, a5, a6, a7, a8;
    for (brow = 0; brow < N; brow += bsize)
        for (bcol = 0; bcol < M; bcol += bsize)
        {
            if (brow == bcol)
                continue;

            a1 = A[brow][bcol + 4];
            a2 = A[brow][bcol + 5];
            a3 = A[brow][bcol + 6];
            a4 = A[brow][bcol + 7];
            a5 = A[brow + 1][bcol + 4];
            a6 = A[brow + 1][bcol + 5];
            a7 = A[brow + 1][bcol + 6];
            a8 = A[brow + 1][bcol + 7];

            for (i = 0; i < bsize; i++) {
                for (j = 0; j < bsize / 2; j++) {
                    B[bcol + j][brow + i] = A[brow + i][bcol + j];
                }
            }
            for (i = bsize - 1; i > 1; i--) {
                for (j = bsize / 2; j < bsize; j++) {
                    B[bcol + j][brow + i] = A[brow + i][bcol + j];
                }
            }
            B[bcol + 4][brow] = a1;
            B[bcol + 5][brow] = a2;
            B[bcol + 6][brow] = a3;
            B[bcol + 7][brow] = a4;
            B[bcol + 4][brow + 1] = a5;
            B[bcol + 5][brow + 1] = a6;
            B[bcol + 6][brow + 1] = a7;
            B[bcol + 7][brow + 1] = a8;
        }

    for (brow = 0; brow < N; brow += bsize)
    {
        // left-top
        a1 = A[brow + 2][brow];
        a2 = A[brow + 2][brow + 1];
        a3 = A[brow + 2][brow + 2];
        a4 = A[brow + 2][brow + 3];
        a5 = A[brow + 3][brow];
        a6 = A[brow + 3][brow + 1];
        a7 = A[brow + 3][brow + 2];
        a8 = A[brow + 3][brow + 3];

        for (i = 0; i < 2; i++) {
            for (j = 0; j < bsize / 2; j++) {
                if (i == j)
                    continue;
                B[brow + j][brow + i] = A[brow + i][brow + j];
            }
            B[brow + i][brow + i] = A[brow + i][brow + i];
        }
        
        B[brow][brow + 2] = a1;
        B[brow + 1][brow + 2] = a2;
        B[brow + 2][brow + 2] = a3;
        B[brow + 3][brow + 2] = a4;
        B[brow][brow + 3] = a5;
        B[brow + 1][brow + 3] = a6;
        B[brow + 2][brow + 3] = a7;
        B[brow + 3][brow + 3] = a8;

        // left-bottom
        a1 = A[brow + 4 + 2][brow];
        a2 = A[brow + 4 + 2][brow + 1];
        a3 = A[brow + 4 + 2][brow + 2];
        a4 = A[brow + 4 + 2][brow + 3];
        a5 = A[brow + 4 + 3][brow];
        a6 = A[brow + 4 + 3][brow + 1];
        a7 = A[brow + 4 + 3][brow + 2];
        a8 = A[brow + 4 + 3][brow + 3];

        for (i = 0; i < 2; i++) {
            for (j = 0; j < bsize / 2; j++) {
                if (i == j)
                    continue;
                B[brow + j][brow + 4 + i] = A[brow + 4 + i][brow + j];
            }
            B[brow + i][brow + 4 + i] = A[brow + 4 + i][brow + i];
        }
        
        B[brow][brow + 4 + 2] = a1;
        B[brow + 1][brow + 4 + 2] = a2;
        B[brow + 2][brow + 4 + 2] = a3;
        B[brow + 3][brow + 4 + 2] = a4;
        B[brow][brow + 4 + 3] = a5;
        B[brow + 1][brow + 4 + 3] = a6;
        B[brow + 2][brow + 4 + 3] = a7;
        B[brow + 3][brow + 4 + 3] = a8;

        //right-bottom
        a1 = A[brow + 4 + 2][brow + 4];
        a2 = A[brow + 4 + 2][brow + 4 + 1];
        a3 = A[brow + 4 + 2][brow + 4 + 2];
        a4 = A[brow + 4 + 2][brow + 4 + 3];
        a5 = A[brow + 4 + 3][brow + 4];
        a6 = A[brow + 4 + 3][brow + 4 + 1];
        a7 = A[brow + 4 + 3][brow + 4 + 2];
        a8 = A[brow + 4 + 3][brow + 4 + 3];

        for (i = 0; i < 2; i++) {
            for (j = 0; j < bsize / 2; j++) {
                if (i == j)
                    continue;
                B[brow + 4 + j][brow + 4 + i] = A[brow + 4 + i][brow + 4 + j];
            }
            B[brow + 4 + i][brow + 4 + i] = A[brow + 4 + i][brow + 4 + i];
        }
        
        B[brow + 4][brow + 4 + 2] = a1;
        B[brow + 4 + 1][brow + 4 + 2] = a2;
        B[brow + 4 + 2][brow + 4 + 2] = a3;
        B[brow + 4 + 3][brow + 4 + 2] = a4;
        B[brow + 4][brow + 4 + 3] = a5;
        B[brow + 4 + 1][brow + 4 + 3] = a6;
        B[brow + 4 + 2][brow + 4 + 3] = a7;
        B[brow + 4 + 3][brow + 4 + 3] = a8;

        //right-top
        a1 = A[brow + 2][brow + 4];
        a2 = A[brow + 2][brow + 4 + 1];
        a3 = A[brow + 2][brow + 4 + 2];
        a4 = A[brow + 2][brow + 4 + 3];
        a5 = A[brow + 3][brow + 4];
        a6 = A[brow + 3][brow + 4 + 1];
        a7 = A[brow + 3][brow + 4 + 2];
        a8 = A[brow + 3][brow + 4 + 3];

        for (i = 0; i < 2; i++) {
            for (j = 0; j < bsize / 2; j++) {
                if (i == j)
                    continue;
                B[brow + 4 + j][brow + i] = A[brow + i][brow + 4 + j];
            }
            B[brow + 4 + i][brow + i] = A[brow + i][brow + 4 + i];
        }
        
        B[brow + 4][brow + 2] = a1;
        B[brow + 4 + 1][brow + 2] = a2;
        B[brow + 4 + 2][brow + 2] = a3;
        B[brow + 4 + 3][brow + 2] = a4;
        B[brow + 4][brow + 3] = a5;
        B[brow + 4 + 1][brow + 3] = a6;
        B[brow + 4 + 2][brow + 3] = a7;
        B[brow + 4 + 3][brow + 3] = a8;

    }
}

/* 
 * trans_block8_61x67 - A: 67x61 -> B: 61x67
 * Split the matrix into 8x8 blocks and the rest is simply transposed.
 */
char trans_block8_61x67_desc[] = "Block scan transpose for 61x67 matrix";
void trans_block8_61x67(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp, brow, bcol;
    int a1, a2, a3, a4, a5;
    int bsize = 8;
    for (brow = 0; brow < N - bsize; brow += bsize)
    {
        for (bcol = 0; bcol < M; bcol += bsize)
        {
            if (bcol < M - bsize)
                for (i = 0; i < bsize; i++) {
                    for (j = 0; j < bsize; j++) {
                        tmp = A[brow + i][bcol + j];
                        B[bcol + j][brow + i] = tmp;
                    }
                }
            else
            {
                for (i = 0; i < bsize; i++) {
                    a1 = A[brow + i][bcol];
                    a2 = A[brow + i][bcol + 1];
                    a3 = A[brow + i][bcol + 2];
                    a4 = A[brow + i][bcol + 3];
                    a5 = A[brow + i][bcol + 4];
                    B[bcol][brow + i] = a1;
                    B[bcol + 1][brow + i] = a2;
                    B[bcol + 2][brow + i] = a3;
                    B[bcol + 3][brow + i] = a4;
                    B[bcol + 4][brow + i] = a5;
                }
            }
        }
    }
        
    
    // bottom 3 lines
    for (bcol = 0; bcol < M; bcol += bsize)
    {
        if (bcol < M - bsize)
        {
            for (i = brow; i < N; i++) {
                for (j = 0; j < bsize; j++) {
                    tmp = A[i][bcol + j];
                    B[bcol + j][i] = tmp;
                }
            }
        }
        else
        {
            for (i = brow; i < N; i++) {
                a1 = A[i][bcol];
                a2 = A[i][bcol + 1];
                a3 = A[i][bcol + 2];
                a4 = A[i][bcol + 3];
                a5 = A[i][bcol + 4];
                B[bcol][i] = a1;
                B[bcol + 1][i] = a2;
                B[bcol + 2][i] = a3;
                B[bcol + 3][i] = a4;
                B[bcol + 4][i] = a5;
            }
        }
    }
    
}

/* 
 * trans_block16_61x67 - A: 67x61 -> B: 61x67
 * Split the matrix into 16x16 blocks.
 */
char trans_block16_61x67_desc[] = "Block scan transpose for 61x67 matrix";
void trans_block16_61x67(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp, brow, bcol;
    int bsize = 16;
    for (brow = 0; brow < N; brow += bsize)
    {
        for (bcol = 0; bcol < M; bcol += bsize)
        {
            for (i = 0; i < bsize && brow + i < N; i++) {
                for (j = 0; j < bsize && bcol + j < M; j++) {
                    tmp = A[brow + i][bcol + j];
                    B[bcol + j][brow + i] = tmp;
                }
            }
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
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    // registerTransFunction(trans, trans_desc); 
    // registerTransFunction(trans_block8, trans_block8_desc);
    // registerTransFunction(trans_block_32x32, trans_block_32x32_desc);
    // registerTransFunction(trans_block_64x64, trans_block_64x64_desc);
    // registerTransFunction(trans_block8_61x67, trans_block_61x67_desc);
    // registerTransFunction(trans_block16_61x67, trans_block16_61x67_desc);

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
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

