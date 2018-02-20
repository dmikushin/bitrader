#include "telegram.h"

#include <string>

using namespace std;
using namespace telegram;

#define TELEGRAM_CASE_STR(err) case err : { static const string str_##err = #err; return str_##err.c_str(); }

const char* telegram::telegramGetErrorString(const telegramError_t err)
{
	switch (err)
	{
	TELEGRAM_CASE_STR(telegramSuccess);
	TELEGRAM_CASE_STR(telegramErrorInitializationFailed);
	TELEGRAM_CASE_STR(telegramErrorMissingAccountKeys);
	TELEGRAM_CASE_STR(telegramErrorSendMessageFailed);
	}
}

