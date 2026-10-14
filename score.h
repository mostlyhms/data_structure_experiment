#ifndef SCORE_H
#define SCORE_H

#include <string>

#include "error.h"

// Parses a user-typed score; rounds half up to one decimal on the string, then checks the range.
ErrorCode parse_input_score(const std::string& text, double& out);

// Parses a score read from the data file; only "I.d" with 1~3 integer digits and one decimal digit.
ErrorCode parse_file_score(const std::string& text, double& out);

// Returns the integer tenths value of a score, computed as lround(score * 10).
long long to_tenths(double score);

// Rounds a finite score half up to one decimal place; returns the stored form tenths / 10.0.
double round_score(double score);

// Returns true if the score is finite and lies in [0.0, 100.0], checked in tenths.
bool is_score_in_range(double score);

// Compares two scores in tenths; returns -1, 0 or 1.
int compare_score(double a, double b);

// Formats a score with exactly one decimal place, e.g. 90 becomes "90.0".
std::string format_score(double score);

#endif
