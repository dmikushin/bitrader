#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "binance.h"
#include "telegram.h"

// Pumping threshold
#define THRESHOLD 1.02
#define THRESHOLD_ROCKET 1.04

using namespace binance;
using namespace std;
using namespace telegram;

int main()
{
	cout << "Initializing ..." << endl;

	Server server;

	Account account(server);
	if (!account.keysAreSet())
	{
		fprintf(stderr, "\nCannot find the api/secret keys pair for Binance account!\n");
		fprintf(stderr, "The user should either provide them to Account constructor,\n");
		fprintf(stderr, "or in the following files: %s, %s\n\n",
			binance::Account::default_api_key_path.c_str(),
			binance::Account::default_secret_key_path.c_str());

		exit(1);
	}

	Bot telegram;
	if (!telegram.keysAreSet())
	{
		fprintf(stderr, "\nCannot find the token/chatid keys pair for Telegram account!\n");
		fprintf(stderr, "The user should either provide them to Telegram constructor,\n");
		fprintf(stderr, "or in the following files: %s, %s\n\n",
			telegram::Bot::default_token_path.c_str(),
			telegram::Bot::default_chatid_path.c_str());

		exit(1);
	}

	Market market(server);
	
	cout << "Getting all *BTC pairs ..." << endl;

	Json::Value result;

	// Get all pairs.
	BINANCE_ERR_CHECK(market.getAllPrices(result)); 
	
	// Filter only "*BTC" pairs.
	const string btc = "BTC";
	vector<string> btcPairs;
	for (Json::Value::ArrayIndex i = 0; i < result.size(); i++)
	{
		const string& pair = result[i]["symbol"].asString();
		if (std::equal(btc.rbegin(), btc.rend(), pair.rbegin()))
			btcPairs.push_back(pair);
	}

	cout << "Finding current positions ..." << endl;
	
	// Get account info.
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

		if (amount <= 0) continue;

		if (std::find(btcPairs.begin(), btcPairs.end(), currency + "BTC") != btcPairs.end())
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

		// TODO Quickly escape currencies purchases not for BTC.
		if (currency == "OST") continue;

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

#if 0
		// Some balances could be seeded, e.g. by new coins.
		// So, we should expect some positions could appear by other means than trading.		
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
#endif
		
		positions[currency].value = value;
		
		cout << currency << " : " << value << " BTC" << endl;
		
		totalValue += value;
	}
	
	cout << "Total account purchase value : " << totalValue << " BTC" << endl;

	cout << "Finding positions actual values ..." << endl;

	double totalActualValue = 0;
	BINANCE_ERR_CHECK(market.getAllPrices(result)); 
	for (Json::Value::ArrayIndex i = 0; i < result.size(); i++)
	{
		const string& pair = result[i]["symbol"].asString();

		if (!std::equal(btc.rbegin(), btc.rend(), pair.rbegin()))
			continue;

		const string currency(pair.c_str(), pair.size() - 3);
		if (positions.find(currency) != positions.end())
		{
			double price = atof(result[i]["price"].asString().c_str());
			double value = positions[currency].amount * price;
			
			double ratio = value / positions[currency].value * 100 - 100;

			cout << currency << " : " << value << " BTC (";
			if (ratio > 0) cout << "+";
			cout << ratio << "%)" << endl;

			totalActualValue += value;
		}
	}

	cout << "Total account actual value : " << totalActualValue << " BTC (";
	{
		double ratio = totalActualValue / totalValue * 100 - 100;
		if (ratio > 0) cout << "+";
		cout << ratio << "%)" << endl;
	}
	cout << "OK!" << endl << endl;
	
	struct TradingFrame
	{
		long idMax;
		double totalQty;
		double avgPrice;
		bool hot;
	};
	
	bool initial = true;	
	vector<TradingFrame> frames(btcPairs.size());

	while (1)
	{
		#pragma omp parallel for num_threads(2)
		for (int i = 0; i < btcPairs.size(); i++)
		{
			const string& pair = btcPairs[i];

			if (pair == "BNB_BTC") continue;

			// Use thread-private result container.
			Json::Value result;

			// Get last 500 trades for each "*BTC" pair.
			while (1)
			{
				binanceError_t status = account.getTrades(result, pair.c_str());
				
				if (status == binanceSuccess) break;
				
				fprintf(stderr, "%s\n", binanceGetErrorString(status));
			}
			
			// Get the newest id across recent trades.
			long idMax = 0, timeMax;
			for (Json::Value::ArrayIndex j = 0; j < result.size(); j++)
			{
				long id = result[j]["id"].asInt64();
				if (id > idMax)
				{
					idMax = id;
					timeMax = result[j]["time"].asInt64();
				}
			}

			// Get the average price across last minute trades.
			double avgPrice = 0, totalQty = 0;
			for (Json::Value::ArrayIndex j = 0; j < result.size(); j++)
			{
				long time = result[j]["time"].asInt64();
				if (timeMax - 60 * 1000 > time) continue;

				double price = atof(result[j]["price"].asString().c_str());
				double qty = atof(result[j]["qty"].asString().c_str());

				totalQty += qty;
				avgPrice += price * qty;
			}
			
			if (totalQty > 0)
				avgPrice /= totalQty;
			
			// If we are on initial step, just record the result.
			if (initial)
			{
				frames[i] = { idMax, totalQty, avgPrice, false };

				cout << pair << " : " << frames[i].idMax << " : " << frames[i].avgPrice << endl;

				continue;
			}
			
			// Re-calculate the average price, accounting only trades
			// that took place after the last seen frame's idMax.
			idMax = 0;
			long idMin = frames[i].idMax;
			avgPrice = 0; totalQty = 0;
			bool nonzero = false;
			for (Json::Value::ArrayIndex j = 0; j < result.size(); j++)
			{
				long id = result[j]["id"].asInt64();
				if (id <= idMin) continue;

				idMax = max(id, idMax);

				double price = atof(result[j]["price"].asString().c_str());
				double qty = atof(result[j]["qty"].asString().c_str());

				totalQty += qty;
				avgPrice += price * qty;
				nonzero = true;
			}

			if (!nonzero) continue;
			
			avgPrice /= totalQty;
			
			// Find update for the current time stamp.
			int buy = -1;
			bool hot = false;
			if (avgPrice >= THRESHOLD * frames[i].avgPrice)
			{
				buy++;

				stringstream msg;
				const string currency(pair.c_str(), pair.size() - 3);
				const string symbol = currency + "_BTC";

				msg << "<a href=\"https://www.binance.com/tradeDetail.html?symbol=" << symbol << "\">" << pair << "</a> +" <<
					(avgPrice / frames[i].avgPrice * 100.0 - 100) << "% 📈";

				// Rocket high?
				if (avgPrice >= THRESHOLD_ROCKET * frames[i].avgPrice)
					 msg << " 🚀";
					
				// Add a note, if we are in position for this currency.
				if (positions.find(currency) != positions.end())
				{
					double amount = positions[currency].amount;
					if (amount != 0)
					{
						msg << " POSITION: " << amount;
						
						if (avgPrice * amount > THRESHOLD * positions[currency].value)
						{
							double profit = avgPrice * amount / positions[currency].value * 100 - 100;
							msg << " RECOM: SELL +" << profit << "%";
						}
						else
							msg << " RECOM: HOLD";
					}
				}
				else
				{
					if (frames[i].hot) buy++;
					if (buy >= 1) msg << " RECOM: <b>BUY</b>";
				}

				// Make BUY on the next round more attractive if the currently seen value
				// is above the threshold (i.e. a hot candle).
				hot = true;

				// Communicate the result over the Telegram.
				telegram.sendMessage(msg.str());
			}
			else
			{
				if (avgPrice < frames[i].avgPrice)
				{
					buy = -INT_MAX;
					hot = false;
				}
			}

			frames[i].idMax = idMax;
			frames[i].totalQty = totalQty;
			frames[i].avgPrice = avgPrice;
			frames[i].hot = hot;
		
			cout << pair << " : " << frames[i].idMax << " : " << frames[i].avgPrice << endl;
		}
	
		initial = false;
	}

	return 0;
}

