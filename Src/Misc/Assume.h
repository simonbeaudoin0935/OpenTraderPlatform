#pragma once

/**
 * @brief Assertion macros for debugging and runtime checks.
 *
 * This header provides a set of ASSUME macros that can be used to assert
 * conditions in the code. In debug builds, these macros will trigger
 * assertions if the conditions are not met, providing detailed error
 * messages. In release builds (when QT_NO_DEBUG is defined and QT_FORCE_ASSERTS
 * is not), these macros become no-ops to avoid performance overhead.
 *
 * The macros come in two variants:
 * - OBJ_ versions: Use the Object's name (this->objectName()) in error messages.
 * - Non-OBJ versions: Use the function name (__FUNCTION__) in error messages.
 *
 * Available macros:
 * - ASSUME_TRUE(cond), OBJ_ASSUME_TRUE(cond): Assert that condition is true.
 * - ASSUME_FALSE(cond), OBJ_ASSUME_FALSE(cond): Assert that condition is false.
 * - ASSUME_EQUAL(left, right), OBJ_ASSUME_EQUAL(left, right): Assert equality.
 * - ASSUME_DIFF(left, right), OBJ_ASSUME_DIFF(left, right): Assert inequality.
 * - ASSUME_GT(left, right), OBJ_ASSUME_GT(left, right): Assert greater than.
 * - ASSUME_GTE(left, right), OBJ_ASSUME_GTE(left, right): Assert greater than or equal.
 * - ASSUME_LT(left, right), OBJ_ASSUME_LT(left, right): Assert less than.
 * - ASSUME_LTE(left, right), OBJ_ASSUME_LTE(left, right): Assert less than or equal.
 */

#include <QtGlobal>
#include <QDebug>

#if defined (QT_NO_DEBUG) && !defined(QT_FORCE_ASSERTS)

#define OBJ_ASSUME_TRUE(cond)          qt_noop()
#define ASSUME_TRUE(cond)              qt_noop()

#define OBJ_ASSUME_FALSE(cond)         qt_noop()
#define ASSUME_FALSE(cond)             qt_noop()

#define OBJ_ASSUME_EQUAL(left, right)  qt_noop()
#define ASSUME_EQUAL(left, right)      qt_noop()

#define OBJ_ASSUME_DIFF(left, right)   qt_noop()
#define ASSUME_DIFF(left, right)       qt_noop()

#define OBJ_ASSUME_GT(left, right)     qt_noop()
#define ASSUME_GT(left, right)         qt_noop()

#define OBJ_ASSUME_GTE(left, right)    qt_noop()
#define ASSUME_GTE(left, right)        qt_noop()

#define OBJ_ASSUME_LT(left, right)     qt_noop()
#define ASSUME_LT(left, right)         qt_noop()

#define OBJ_ASSUME_LTE(left, right)    qt_noop()
#define ASSUME_LTE(left, right)        qt_noop()

#else

static inline QString debugToString(auto &&value)
{
    QString s;
    QDebug(&s).nospace().noquote() << value;
    return s;
}

#define OBJ_ASSUME_TRUE(cond) ASSUME_TRUE_IMPL(cond, qPrintable(this->objectName()))
#define ASSUME_TRUE(cond)     ASSUME_TRUE_IMPL(cond, __FUNCTION__)

#define ASSUME_TRUE_IMPL(cond, where) \
    do \
    { \
        if (!(cond)) [[unlikely]] { \
            const char * _what = "Assumed true condition is false: " #cond; \
            qt_assert_x(where, _what, __FILE__, __LINE__); \
        } \
    } while (false)


#define OBJ_ASSUME_FALSE(cond) ASSUME_FALSE_IMPL(cond, qPrintable(this->objectName()))
#define ASSUME_FALSE(cond)     ASSUME_FALSE_IMPL(cond, __FUNCTION__)

#define ASSUME_FALSE_IMPL(cond, where)                                     \
    do                                                                     \
    {                                                                      \
        if (cond) [[unlikely]]                                             \
        {                                                                  \
            const char *_what = "Assumed false condition is true: " #cond; \
            qt_assert_x(where, _what, __FILE__, __LINE__);                 \
        }                                                                  \
    } while (false)

#define OBJ_ASSUME_EQUAL(left, right) ASSUME_EQUAL_IMPL(left, right, qPrintable(this->objectName()))
#define ASSUME_EQUAL(left, right)     ASSUME_EQUAL_IMPL(left, right, __FUNCTION__)

#define ASSUME_EQUAL_IMPL(left, right, where)                                  \
    do                                                                         \
    {                                                                          \
        auto _left_val = (left);                                               \
        auto _right_val = (right);                                             \
        if (_left_val != _right_val) [[unlikely]]                              \
        {                                                                      \
            QString what = QStringLiteral("Assumed equality failed:\n")        \
             + QStringLiteral(#left) + " = " + debugToString(_left_val) + "\n" \
             + QStringLiteral(#right) + " = " + debugToString(_right_val) + "\n";     \
            qt_assert_x(where, qPrintable(what), __FILE__, __LINE__);          \
        }                                                                      \
    } while (false)

#define OBJ_ASSUME_DIFF(left, right) ASSUME_DIFF_IMPL(left, right, qPrintable(this->objectName()))
#define ASSUME_DIFF(left, right) ASSUME_DIFF_IMPL(left, right, __FUNCTION__)

#define ASSUME_DIFF_IMPL(left, right, where)                                                                                                                                                                  \
    do                                                                                                                                                                                                         \
    {                                                                                                                                                                                                          \
        auto _left_val = (left);                                                                                                                                                                               \
        auto _right_val = (right);                                                                                                                                                                             \
        if (_left_val == _right_val) [[unlikely]]                                                                                                                                                              \
        {                                                                                                                                                                                                      \
            QString what = QStringLiteral("Assumed inequality failed:\n") + QStringLiteral(#left) + " = " + debugToString(_left_val) + "\n" + QStringLiteral(#right) + " = " + debugToString(_right_val) + "\n"; \
            qt_assert_x(where, qPrintable(what), __FILE__, __LINE__);                                                                                                                                          \
        }                                                                                                                                                                                                      \
    } while (false)

#define OBJ_ASSUME_GT(left, right) ASSUME_GT_IMPL(left, right, qPrintable(this->objectName()))
#define ASSUME_GT(left, right) ASSUME_GT_IMPL(left, right, __FUNCTION__)

#define ASSUME_GT_IMPL(left, right, where)                                                                                                                                                           \
    do                                                                         \
    {                                                                          \
        auto _left_val = (left);                                               \
        auto _right_val = (right);                                             \
        if (!(_left_val > _right_val)) [[unlikely]]                               \
        {                                                                      \
            QString what = QStringLiteral("Assumed greater than failed:\n")    \
             + QStringLiteral(#left) + " = " + debugToString(_left_val) + "\n" \
             + QStringLiteral(#right) + " = " + debugToString(_right_val) + "\n";     \
            qt_assert_x(where, qPrintable(what), __FILE__, __LINE__);          \
        }                                                                      \
    } while (false)

#define OBJ_ASSUME_GTE(left, right) ASSUME_GTE_IMPL(left, right, qPrintable(this->objectName()))
#define ASSUME_GTE(left, right) ASSUME_GTE_IMPL(left, right, __FUNCTION__)

#define ASSUME_GTE_IMPL(left, right, where)                                                                                                                                                           \
    do                                                                         \
    {                                                                          \
        auto _left_val = (left);                                               \
        auto _right_val = (right);                                             \
        if (!(_left_val >= _right_val)) [[unlikely]]                               \
        {                                                                      \
            QString what = QStringLiteral("Assumed greater than or equal failed:\n")    \
             + QStringLiteral(#left) + " = " + debugToString(_left_val) + "\n" \
             + QStringLiteral(#right) + " = " + debugToString(_right_val) + "\n";     \
            qt_assert_x(where, qPrintable(what), __FILE__, __LINE__);          \
        }                                                                      \
    } while (false)

#define OBJ_ASSUME_LT(left, right) ASSUME_LT_IMPL(left, right, qPrintable(this->objectName()))
#define ASSUME_LT(left, right) ASSUME_LT_IMPL(left, right, __FUNCTION__)

#define ASSUME_LT_IMPL(left, right, where)                                                                                                                                                           \
    do                                                                         \
    {                                                                          \
        auto _left_val = (left);                                               \
        auto _right_val = (right);                                             \
        if (!(_left_val < _right_val)) [[unlikely]]                               \
        {                                                                      \
            QString what = QStringLiteral("Assumed less than failed:\n")    \
             + QStringLiteral(#left) + " = " + debugToString(_left_val) + "\n" \
             + QStringLiteral(#right) + " = " + debugToString(_right_val) + "\n";     \
            qt_assert_x(where, qPrintable(what), __FILE__, __LINE__);          \
        }                                                                      \
    } while (false)

#define OBJ_ASSUME_LTE(left, right) ASSUME_LTE_IMPL(left, right, qPrintable(this->objectName()))
#define ASSUME_LTE(left, right) ASSUME_LTE_IMPL(left, right, __FUNCTION__)

#define ASSUME_LTE_IMPL(left, right, where)                                                                                                                                                           \
    do                                                                         \
    {                                                                          \
        auto _left_val = (left);                                               \
        auto _right_val = (right);                                             \
        if (!(_left_val <= _right_val)) [[unlikely]]                               \
        {                                                                      \
            QString what = QStringLiteral("Assumed less than or equal failed:\n")    \
             + QStringLiteral(#left) + " = " + debugToString(_left_val) + "\n" \
             + QStringLiteral(#right) + " = " + debugToString(_right_val) + "\n";     \
            qt_assert_x(where, qPrintable(what), __FILE__, __LINE__);          \
        }                                                                      \
    } while (false)




#endif // QT_NO_DEBUG && !QT_FORCE_ASSERTS