#pragma once

#include "Vec4.h"


namespace tinymath
{
    /*
        Matrix structure, contains data as [4][4] float array data.
        data[row][column] layout.

        Conventions — all four are load-bearing, none are visible in the layout above:

        - Column vectors. A point is a column, so transforms apply as M * v.
        - Right to left composition. In A * B, B is applied to the vector first.
          viewport * perspective * lookAt therefore runs lookAt first.
        - Translation lives in the fourth column: data[0][3], data[1][3], data[2][3].
        - An orientation goes in as ROWS, not columns. Row 0 is the x axis of the
          target space, row 1 the y axis, row 2 the z axis, so output x is the dot
          of row 0 with the input. Writing the same basis into the columns builds
          the transpose, which for an orthonormal basis is the inverse — a valid
          rotation the wrong way round. It renders a plausible image and nothing
          about it looks broken. Confirmed on screen (lesson 5).
    */
    struct Matrix4x4
    {
        
        float data[4][4];
        
        Matrix4x4()
        {
            for (int i=0; i < 4;  i++)
            {
                for (int j=0; j < 4; j++)
                {
                    if (i == j)
                    {
                        data[i][j] = 1.0f;
                    }
                    else
                    {
                        data[i][j] = 0.0f;
                    }
                }
            }
        }
        

        Matrix4x4 operator *(const Matrix4x4& b) const
        {
            Matrix4x4 m;

            for (int i_row_a = 0; i_row_a < 4; i_row_a++)
            {
                for(int i_column_b = 0; i_column_b < 4; i_column_b++)
                {
                    float dot_result = 0.0f;
                    for (int element = 0; element < 4; element++)
                    {
                        dot_result += data[i_row_a][element] * b.data[element][i_column_b];
                    }
                    m.data[i_row_a][i_column_b] = dot_result;
                }
            }

            return m;
        }
        

        Vec4f operator *(const Vec4f& v) const
        {
            return {
                data[0][0] * v.x + data[0][1] * v.y + data[0][2] * v.z + data[0][3] * v.w, 
                data[1][0] * v.x + data[1][1] * v.y + data[1][2] * v.z + data[1][3] * v.w,
                data[2][0] * v.x + data[2][1] * v.y + data[2][2] * v.z + data[2][3] * v.w,
                data[3][0] * v.x + data[3][1] * v.y + data[3][2] * v.z + data[3][3] * v.w,
            };
        }
    };

    inline Matrix4x4 transpose(const Matrix4x4& matrix)
    {
        Matrix4x4 transpose_matrix;
        for (int i = 0; i < 4; i++)
        {
            for(int j = 0; j < 4; j++)
            {
                transpose_matrix.data[j][i] = matrix.data[i][j];
            }
        }

        return transpose_matrix;
    }
    
} // end of tinymath namespace
