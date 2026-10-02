// Test that format_from_dict localizes dates according to the locale.
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <locale>
#include <string>
#include "format_from_dict.h"

int main()
{
  // Use the Swedish locale. Skip the test (exit code 77) if not installed.
  const char *locname = "sv_SE.UTF-8";
  // format_from_dict uses the environment locale, as paps does.
  setenv("LC_ALL", locname, 1);
  if (!setlocale(LC_ALL, "")) {
    fprintf(stderr, "Locale %s not available. Skipping.\n", locname);
    return 77;
  }

  // 2026-10-02 is a Friday.
  std::tm tm = {};
  tm.tm_year = 2026 - 1900;
  tm.tm_mon = 9;
  tm.tm_mday = 2;
  tm.tm_hour = 12;
  tm.tm_isdst = -1;
  paps_time_t t{std::mktime(&tm)};

  dict_t dict;
  dict["date"] = t;

  std::string res = format_from_dict("{date:%A}", dict);
  if (res != "fredag") {
    fprintf(stderr, "FAIL: expected 'fredag', got '%s'\n", res.c_str());
    return 1;
  }

  res = format_from_dict("{date:%c}", dict);
  if (res.find("fre") == std::string::npos) {
    fprintf(stderr, "FAIL: %%c not localized: '%s'\n", res.c_str());
    return 1;
  }
  return 0;
}
