#include <iostream>
#include <jsoncpp/json/json.h>
#include <string>
#include <vector>

#include "binance.h"

using namespace binance;
using namespace std;

int main()
{
	Json::Value result;

	Market market;

	// Get all pairs.
	BINANCE_ERR_CHECK(market.getAllPrices(result)); 
	
	// Filter only "*BTC" pairs.
	const string btc = "BTC";
	vector<string> btcPairs;
	for (Json::Value::ArrayIndex i = 0 ; i < result.size() ; i++)
	{
		const string& pair = result[i]["symbol"].asString();
		if (std::equal(btc.rbegin(), btc.rend(), pair.rbegin()))
			btcPairs.push_back(pair);
	}

	bool initial = true;	
	vector<long> candleTime(btcPairs.size());
	vector<double> candleAvgHigh(btcPairs.size());
	
	while (1)
	{
		for (int i = 0, e = btcPairs.size(); i < e; i++)
		{
			const string& pair = btcPairs[i];
	
			// Get Klines / CandleStick for each "*BTC" pair.
			BINANCE_ERR_CHECK(market.getKlines(pair.c_str(), "1m", 10, 0, 0, result));

			// Find update for the current time stamp.
			for (Json::Value::ArrayIndex j = 0 ; j < result.size() ; j++)
			{
				long newCandleTime = result[j][0].asInt64();
				if (newCandleTime <= candleTime[i]) continue;
			
				double newCandleAvgHigh = atof(result[j][4].asString().c_str());
				if ((newCandleAvgHigh >= 1.02 * candleAvgHigh[i]) && !initial)
				{
					cout << pair << " : " << " +" << (newCandleAvgHigh / candleAvgHigh[i] * 100.0 - 100) << "%" << endl;
				}
			
				candleTime[i] = newCandleTime;
				candleAvgHigh[i] = newCandleAvgHigh;
			}
			
			cout << pair << " : " << candleTime[i] << " : " << candleAvgHigh[i] << endl;
		}
		
		initial = false;
	}

	return 0;
}

