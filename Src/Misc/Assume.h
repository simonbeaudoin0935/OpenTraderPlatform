#pragma once

#include <QtGlobal>
#include <QDebug>

#if defined (QT_NO_DEBUG) && !defined(QT_FORCE_ASSERTS)
#define OBJ_ASSUME_TRUE(cond)          qt_noop()
#define OBJ_ASSUME_FALSE(cond)         qt_noop()
#define OBJ_ASSUME_EQUAL(left, right)  qt_noop()
#else

static inline QString debugToString(auto &&value)
{
    QString s;
    QDebug(&s).nospace().noquote() << value;
    return s;
}

#define OBJ_ASSUME_TRUE(cond)                                                  \
    do                                                                         \
    {                                                                          \
        if (!(cond)) [[unlikely]] {                                            \
            const char * _where = qPrintable(this->objectName());              \
            const char * _what = "Assumed true condition is false: " #cond;    \
            qt_assert_x(_where, _what, __FILE__, __LINE__);                    \
        }                                                                      \
    } while (false)

#define OBJ_ASSUME_FALSE(cond)                                                 \
    do                                                                         \
    {                                                                          \
        if (cond) [[unlikely]]                                                 \
        {                                                                      \
            const char *_where = qPrintable(this->objectName());               \
            const char *_what = "Assumed false condition is true: " #cond;     \
            qt_assert_x(_where, _what, __FILE__, __LINE__);                    \
        }                                                                      \
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