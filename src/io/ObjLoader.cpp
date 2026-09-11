#include "ObjLoader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>


Mesh io::loadObj(std::string const& file_path)
{
    std::ifstream file(file_path);
    if (!file)
    {
        throw std::runtime_error("unable to load file: " + file_path);
    }

    std::string line;
    Mesh mesh;
    
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        char trash;

        // parsing Vertex
        if(line.compare(0, 2, "v ") == 0) // pattern matched
        {
            // v 0.11526 0.700717 0.0677257
            iss >> trash; // "v" goes here
            tinymath::Vec3f point_position;
            iss >> point_position.x; // first float value
            iss >> point_position.y; // second float value
            iss >> point_position.z; // third float value

            mesh.vertices.push_back(point_position);
        }

        // parsing Normals
        else if (line.compare(0, 3, "vn ") == 0)
        {
            // vn  0.001 0.482 -0.876
            iss >> trash >> trash; // "v", "n" goes here  
            tinymath::Vec3f vertex_normal;
            iss >> vertex_normal.x; // first float value
            iss >> vertex_normal.y; // second float value
            iss >> vertex_normal.z; // third float value

            mesh.normals.push_back(vertex_normal);
        }

        // parsing Texture Coordinates uvw
        else if (line.compare(0, 3, "vt ") == 0)
        {
            // vt  0.644 0.154 0.599
            iss >> trash >> trash; // "v", "t" goes here  
            tinymath::Vec3f vertex_texture_coordinates;
            iss >> vertex_texture_coordinates.x; // first float value
            iss >> vertex_texture_coordinates.y; // second float value
            iss >> vertex_texture_coordinates.z; // third float value

            mesh.textureCoordinates.push_back(vertex_texture_coordinates);
        }

        // parsing Face indices
        else if (line.compare(0, 2, "f ") == 0)
        {
            //   v t n v t n v t n  (V:vertex index, t: texture coord index, n normal index)
            // f 6/1/6 5/2/5 8/3/8
            std::array<int, 3> face_indices;
            std::array<int, 3> normal_indices;
            std::array<int, 3> texture_coordinates_indices;

            iss >> trash; // f goes here
            
            iss >> face_indices[0]; // first int from first triad
            iss >> trash; // skips: "/"
            iss >> texture_coordinates_indices[0];
            iss >> trash; // skips: "/"
            iss >> normal_indices[0];

            iss >> face_indices[1]; // first int from second triad
            iss >> trash; // skips: "/"
            iss >> texture_coordinates_indices[1];
            iss >> trash; // skips: "/"
            iss >> normal_indices[1];
            
            iss >> face_indices[2]; // first int from third triad
            iss >> trash;  // skips: "/"
            iss >> texture_coordinates_indices[2];
            iss >> trash; // skips: "/"
            iss >> normal_indices[2];
            
            /*
                Because the obj format saves indexes starting at 1.
                we need to offset them back to match c++ array/vector indexing.
            */
            for (int i = 0; i < 3; i++)
            {
                face_indices[i] -= 1;
                normal_indices[i] -= 1;
                texture_coordinates_indices[i] -= 1;
            }

            mesh.faceIndices.emplace_back(face_indices);
            mesh.faceNormalIndices.emplace_back(normal_indices);
            mesh.faceTextureCoordinateIndices.emplace_back(texture_coordinates_indices);
        }
    }

    return mesh;
}