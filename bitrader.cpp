#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <limits>
#include <set>
#include <streambuf>
#include <string>
#include <tgbot/tgbot.h>
#include <vector>
#include <wordexp.h>

#include "binance.h"

// Pumping threshold
#define THRESHOLD 1.02
#define THRESHOLD_ROCKET 1.04

using namespace binance;
using namespace std;

int main()
{
	cout << "Initializing ..." << endl;

	// Read telegram token.
	string token;
	{
		wordexp_t p;
		char** w;
		wordexp("$HOME/.bitrader/telegrambot/token", &p, 0);
		w = p.we_wordv;
		ifstream telegrambot(w[0]);
		telegrambot >> token;
		telegrambot.close();
		wordfree(&p);
	}
	
	// Read telegram chat id.
	unsigned long chatid;
	{
		wordexp_t p;
		char** w;
		wordexp("$HOME/.bitrader/telegrambot/chatid", &p, 0);
		w = p.we_wordv;
		ifstream telegrambot(w[0]);
		telegrambot >> chatid;
		telegrambot.close();
		wordfree(&p);
	}
	
	Json::Value result;

	// Get all pairs.
	Market market;
	BINANCE_ERR_CHECK(market.getAllPrices(result)); 
	
	// Filter only "*BTC" pairs.
	const string btc = "BTC";
	vector<string> btcPairs;
	for (Json::Value::ArrayIndex i = 0; i < result.size() ; i++)
	{
		const string& pair = result[i]["symbol"].asString();
		if (std::equal(btc.rbegin(), btc.rend(), pair.rbegin()))
			btcPairs.push_back(pair);
	}
	btcPairs.push_back("BNB_BTC");

	cout << "Finding current positions ..." << endl;
	
	// Get account info.
	Account account;
	BINANCE_ERR_CHECK(account.getInfo(result));

	struct Position
	{
		// Amount in currency.
		double amount;
		
		// The purchase value of amount, according to trading history.
		double value;
	};

	map<string, Position> positions;
	
	// Get amounts for all positions in accout.
	const Json::Value balances = result["balances"];
	for (Json::Value::ArrayIndex i = 0, e = balances.size(); i < e; i++)
	{
		const string currency = balances[i]["asset"].asString();
		const string symbol = currency + "BTC";
		double amount = atof(balances[i]["free"].asString().c_str());

		if (std::find(btcPairs.begin(), btcPairs.end(), currency + "_BTC") != btcPairs.end())
			positions[currency].amount = amount;
	}

	BINANCE_ERR_CHECK(account.getOpenOrders(result));

	// Adding open orders to position amount.
	for (Json::Value::ArrayIndex i = 0, e = result.size(); i < e; i++)
	{
		const Json::Value& order = result[i];

		const string symbol = order["symbol"].asString();
		const string currency(symbol.c_str(), symbol.size() - 3);

		const string side = order["side"].asString();
		if (side != "SELL") continue;
	
		const double origQty = atof(order["origQty"].asString().c_str());
		const double executedQty = atof(order["executedQty"].asString().c_str());
	
		positions[currency].amount += origQty - executedQty;
	}

	for (map<string, Position>::iterator i = positions.begin(), ie = positions.end(); i != ie; i++)
		cout << i->first << " : " << i->second.amount << endl;

	cout << "Finding positions purchase values ..." << endl;
	
	// Get all orders for currencies we are in position for
	// and calculate the purchase price (in BTC) of the available amount.
	double totalValue = 0;
	for (map<string, Position>::iterator i = positions.begin(), ie = positions.end(); i != ie; i++)
	{
		const string& currency = i->first;
		const string symbol = currency + "BTC";
		const double amount = i->second.amount;
		
		if (amount == 0) continue;
				
		// TODO limited to 500 transactions
		account.getAllOrders(result, symbol.c_str());

		struct Purchase
		{
			double price;
			double amount;

			bool operator<(const Purchase& other) const
			{
			    return price * amount < other.price * other.amount;
			}
			
			Purchase(const double price_, const double amount_) : price(price_), amount(amount_) { }
		};

		// Run all orders, balancing buys and sells.
		set<Purchase> purchases;
		for (Json::Value::ArrayIndex j = 0, je = result.size(); j < je; j++)
		{
			const Json::Value& order = result[j];
			
			const string side = order["side"].asString();
			if (side == "BUY")
			{
				const double price = atof(order["price"].asString().c_str());
				const double amount = atof(order["executedQty"].asString().c_str());
				
				purchases.insert(Purchase(price, amount));	
			}
			else if (side == "SELL")
			{
				const double price = atof(order["price"].asString().c_str());
				double amount = atof(order["executedQty"].asString().c_str());

				// Substract from purchases until done.
				while (1)
				{				
					// Find purchase with the largest (worst) price.
					set<Purchase>::iterator maxPurchase = purchases.begin();
					for (set<Purchase>::iterator k = purchases.begin(), ke = purchases.end(); k != ke; k++)
					{
						if ((k->price > maxPurchase->price) && (maxPurchase->amount > 0))
							maxPurchase = k;
					}
					
					if (maxPurchase->amount < amount)
					{
						amount -= maxPurchase->amount;
						purchases.erase(maxPurchase);
					}
					else
					{
						int npurchases = purchases.size();
						purchases.insert(Purchase(maxPurchase->price, maxPurchase->amount - amount));
						if (npurchases < purchases.size())
							purchases.erase(maxPurchase);
						break;
					}
				}
			}
		}
		
		// Finally, calculate the purchase price of unsold amount.
		double value = 0;
		double totalAmount = 0;
		for (set<Purchase>::iterator k = purchases.begin(), ke = purchases.end(); k != ke; k++)
		{	
			value += k->price * k->amount;
			totalAmount += k->amount;
		}
		
		if (fabs(totalAmount - positions[currency].amount) > numeric_limits<double>::epsilon())
		{
			// Do not check BNB: it could be used also used to pay comissions,
			// which are not accounted into the trade data.
			if (currency != "BNB")
			{
				fprintf(stderr, "Reported and calculated position amounts mismatch: %f != %f\n",
					totalAmount, positions[currency].amount);
				exit(1);
			}
		}
		
		positions[currency].value = value;
		
		cout << currency << " : " << value << " BTC" << endl;
		
		totalValue += value;
	}
	
	cout << "Total account value : " << totalValue << " BTC" << endl;
	cout << "OK!" << endl << endl;
	
	try
	{
		bool initial = true;	
		vector<long> candleTime(btcPairs.size());
		vector<double> candleAvgHigh(btcPairs.size());

		TgBot::Bot bot(token.c_str());

		while (1)
		{
			for (int i = 0; i < btcPairs.size(); i++)
			{
				const string& pair = btcPairs[i];
	
				// Get Klines / CandleStick for each "*BTC" pair.
				if (market.getKlines(pair.c_str(), "1m", 10, 0, 0, result) != binanceSuccess)
					continue;

				// Find update for the current time stamp.
				for (Json::Value::ArrayIndex j = 0; j < result.size() ; j++)
				{
					long newCandleTime = result[j][0].asInt64();
					if (newCandleTime <= candleTime[i]) continue;

					double newCandleAvgHigh = atof(result[j][4].asString().c_str());
					if ((newCandleAvgHigh >= THRESHOLD * candleAvgHigh[i]) && !initial)
					{
						stringstream msg;
						const string currency(pair.c_str(), pair.size() - 3);
						const string symbol = currency + "_BTC";
						msg << "<a href=\"https://www.binance.com/tradeDetail.html?symbol=" << symbol << "\">" << pair << "</a> +" <<
							(newCandleAvgHigh / candleAvgHigh[i] * 100.0 - 100) << "%";

						// Rocket high?
						if (newCandleAvgHigh >= THRESHOLD_ROCKET * candleAvgHigh[i])
							 msg << " 🚀";
							
						// Add a note, if we are in posiion for this currency.
						if (positions.find(currency) != positions.end())
						{
							double amount = positions[currency].amount;
							if (amount != 0)
							{
								msg << " POSITION: " << amount;
								
								if (newCandleAvgHigh * amount > THRESHOLD_ROCKET * positions[currency].value)
								{
									double profit = newCandleAvgHigh * amount / positions[currency].value * 100 - 100;
									msg << " RECOM: SELL +" << profit << "%";
								}
								else
									msg << " RECOM: HOLD";
							}
						}
						bot.getApi().sendMessage(chatid, msg.str(), false, 0, TgBot::GenericReply::Ptr(), "HTML");
					}

					candleTime[i] = newCandleTime;
					candleAvgHigh[i] = newCandleAvgHigh;
				}
			
				cout << pair << " : " << candleTime[i] << " : " << candleAvgHigh[i] << endl;
			}
		
			initial = false;
		}
	}
	catch (TgBot::TgException& e)
	{
        fprintf(stderr, "error: %s\n", e.what());
    }

	return 0;
}

