#pragma once

#include <QtGlobal>

namespace mixxx {

enum HttpStatusCodes {
    Invalid = 0,
    Ok = 200,
    Created = 201,
    NoContent = 204,
};

typedef int HttpStatusCode;

inline
int HttpStatusCode_isValid(
        int statusCode) {
    return statusCode >= 100 && statusCode < 600;
}

inline
int HttpStatusCode_isInformational(
        int statusCode) {
    return statusCode >= 100 && statusCode < 200;
}

inline
int HttpStatusCode_isSuccess(
        int statusCode) {
    return statusCode >= 200 && statusCode < 300;
}

inline
int HttpStatusCode_isRedirection(
        int statusCode) {
    return statusCode >= 300 && statusCode < 400;
}

inline
int HttpStatusCode_isClientError(
        int statusCode) {
    return statusCode >= 400 && statusCode < 500;
}

inline
int HttpStatusCode_isServerError(
        int statusCode) {
    return statusCode >= 400 && statusCode < 500;
}

} // namespace mixxx

Q_DECLARE_METATYPE(mixxx::HttpStatusCode);
