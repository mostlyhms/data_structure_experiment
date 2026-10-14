#ifndef STUDENT_H
#define STUDENT_H

#include <string>

// One student record. id is a digit string, name is stored as raw bytes,
// score is in [0.0, 100.0] and always equals (integer tenths) / 10.0.
struct Student {
    std::string id;
    std::string name;
    double score = 0.0;
};

#endif
