//
// Created by troplo on 10/14/26.
//

#ifndef NOMADARCHIVES_ERRORS_H
#define NOMADARCHIVES_ERRORS_H

#include <system_error>
#include <type_traits>

enum class ErrorCodes {
    Success = 0,
    InvalidFAT = 1,
    ErrorReadingFile = 2,
    UnknownError = 3,
    DecompressionError = 4
};

namespace std {
    template <> struct is_error_code_enum<ErrorCodes> : true_type {};
} // namespace std

class BFToolsError : public std::error_category {
  public:
    const char *name() const noexcept override { return "BFTools"; }

    [[nodiscard]] std::string message(int ev) const override {
        switch (static_cast<ErrorCodes>(ev)) {
        case ErrorCodes::Success:
            return "Operation completed successfully!";
        case ErrorCodes::ErrorReadingFile:
            return "There was an error reading the file.";
        case ErrorCodes::InvalidFAT:
            return "The FAT you are trying to load is unsupported by this "
                   "version of "
                   "BFTools.";
        case ErrorCodes::UnknownError:
            return "An unknown error occurred.";
        case ErrorCodes::DecompressionError:
            return "There was an error decompressing the file.";
        default:
            return "Unknown error.";
        }
    }
};

inline const BFToolsError &bftools_error() {
    static BFToolsError instance;
    return instance;
}

inline std::error_code make_error_code(ErrorCodes e) noexcept {
    return {static_cast<int>(e), bftools_error()};
}

#endif // NOMADARCHIVES_ERRORS_H
