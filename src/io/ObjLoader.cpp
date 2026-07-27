#include "ObjLoader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>


io::Mesh io::loadObj(std::string const& file_path)
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
        std::istringstream iss(line.c_str());
        char trash;
        if(line.compare(0, 2, "v ") == 0) // patter matched
        {
            // v 0.11526 0.700717 0.0677257
            iss >> trash; // "v" goes here
            tinymath::Vec3f point_position;
            iss >> point_position.x; // first float value
            iss >> point_position.y; // second float value
            iss >> point_position.z; // third float value

            mesh.vertices.push_back(point_position);
        }
        else if (line.compare(0, 2, "f ")== 0)
        {
            // f 6/1/6 5/2/5 8/3/8
            std::array<int, 3> face_indices;
            int skip_int;
            iss >> trash; // f goes here
            iss >> face_indices[0]; // first int from first triad
            face_indices[0] -= 1; 
            iss >> trash >> skip_int >> trash >> skip_int; // skips: "/" "int" "/" "int"
            iss >> face_indices[1];
            face_indices[1] -= 1;
            iss >> trash >> skip_int >> trash >> skip_int; // skips: "/" "int" "/" "int"
            iss >> face_indices[2];
            face_indices[2] -= 1;

            mesh.faceIndices.emplace_back(face_indices);
        }
    }

    return mesh;
}