#include "logflow/util/Timestamp.hpp"

#include <array>
#include <cstdio>

namespace logflow {
namespace {

constexpr std::array<const char*, 12> kMonths = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(int year, int month) {
    static constexpr std::array<int, 12> kDays = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && isLeapYear(year) ? 29 : kDays[month - 1];
}

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's algorithm).
long long daysFromCivil(long long y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

// Inverse of daysFromCivil.
void civilFromDays(long long z, long long& y, unsigned& m, unsigned& d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y = static_cast<long long>(yoe) + era * 400 + (m <= 2);
}

// Reads `count` decimal digits starting at `pos`. Returns -1 if any is not a digit.
int readDigits(const std::string& s, std::size_t pos, std::size_t count) {
    int value = 0;
    for (std::size_t i = pos; i < pos + count; ++i) {
        if (s[i] < '0' || s[i] > '9') return -1;
        value = value * 10 + (s[i] - '0');
    }
    return value;
}

}  // namespace

std::optional<Timestamp> parseClfTimestamp(const std::string& text) {
    // dd/Mon/yyyy:HH:MM:SS +zzzz
    // 0123456789012345678901234 5
    if (text.size() != 26 || text[2] != '/' || text[6] != '/' || text[11] != ':' ||
        text[14] != ':' || text[17] != ':' || text[20] != ' ' ||
        (text[21] != '+' && text[21] != '-')) {
        return std::nullopt;
    }

    int month = 0;
    for (std::size_t i = 0; i < kMonths.size(); ++i) {
        if (text.compare(3, 3, kMonths[i]) == 0) month = static_cast<int>(i) + 1;
    }

    const int day = readDigits(text, 0, 2);
    const int year = readDigits(text, 7, 4);
    const int hour = readDigits(text, 12, 2);
    const int minute = readDigits(text, 15, 2);
    const int second = readDigits(text, 18, 2);
    const int offsetHours = readDigits(text, 22, 2);
    const int offsetMinutes = readDigits(text, 24, 2);

    if (month == 0 || day < 1 || year < 0 || hour < 0 || hour > 23 || minute < 0 ||
        minute > 59 || second < 0 || second > 59 || offsetHours < 0 || offsetHours > 23 ||
        offsetMinutes < 0 || offsetMinutes > 59 || day > daysInMonth(year, month)) {
        return std::nullopt;
    }

    const long long offsetSeconds =
        (text[21] == '-' ? -1 : 1) * (offsetHours * 3600LL + offsetMinutes * 60LL);
    const long long localSeconds =
        daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * 86400LL +
        hour * 3600LL + minute * 60LL + second;

    // Local time = UTC + offset, so UTC = local time - offset.
    return Timestamp(std::chrono::seconds(localSeconds - offsetSeconds));
}

std::string formatIso8601(Timestamp timestamp) {
    const long long total =
        std::chrono::floor<std::chrono::seconds>(timestamp.time_since_epoch()).count();
    long long days = total / 86400;
    long long secondsOfDay = total % 86400;
    if (secondsOfDay < 0) {
        secondsOfDay += 86400;
        --days;
    }

    long long year = 0;
    unsigned month = 0;
    unsigned day = 0;
    civilFromDays(days, year, month, day);

    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%04lld-%02u-%02uT%02lld:%02lld:%02lldZ", year, month, day,
                  secondsOfDay / 3600, secondsOfDay % 3600 / 60, secondsOfDay % 60);
    return buffer;
}

}  // namespace logflow
