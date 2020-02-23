#ifndef TELEGRAM_H
#define TELEGRAM_H

#include <memory>
#include <queue>
#include <string>
#include <tgbot/tgbot.h>

namespace telegram
{
	enum telegramError_t
	{
		telegramSuccess = 0,
		telegramErrorInitializationFailed,
		telegramErrorMissingAccountKeys,
		telegramErrorSendMessageFailed,
	};

	const char* telegramGetErrorString(const telegramError_t err);

	// Token + ChatID keys required
	class Bot
	{
		std::string token;
		unsigned long chatid;

		std::unique_ptr<TgBot::Bot> bot;

		std::queue<std::string> msgQueue;

	public :

		static const std::string default_token_path;
		static const std::string default_chatid_path;

		Bot(const std::string token = "", const unsigned long chatid = 0);

		bool keysAreSet() const;

		telegramError_t initialize();

		telegramError_t sendMessage(std::string message);
	};
}

#endif // TELEGRAM_H

