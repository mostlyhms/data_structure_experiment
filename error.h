#ifndef ERROR_H
#define ERROR_H

// Error codes shared by all modules of the student management program.
enum class ErrorCode {
    OK,
    NOT_INITIALIZED,
    ID_EMPTY,
    ID_TOO_LONG,
    ID_INVALID_CHAR,
    ID_LEADING_ZERO,
    ID_DUPLICATE,
    ID_NOT_FOUND,
    NAME_EMPTY,
    NAME_TOO_LONG,
    NAME_INVALID_CHAR,
    SCORE_INVALID_FORMAT,
    SCORE_OUT_OF_RANGE,
    FILE_NOT_FOUND,
    FILE_FORMAT_ERROR,
    FILE_IO_ERROR
};

#endif
