## BITrader - Trading and Alering Service for Binance Cryptocurrency Exchange

BITrader - Trading and Alering Service for Binance Cryptocurrency Exchange (http://binance.com):

 * Monitor all '\*BTC' symbols trades and detect pumps
 * Send alerts to Telegram bot
 * Sell/buy stock using heuristics or by trader's interactive decision in Telegram

### Prerequisites

```
sudo apt-get install libjsoncpp-dev libcurl4-nss-dev libwebsockets-dev
sudo apt-get install g++ make binutils cmake libssl-dev libboost-system-dev libboost-iostreams-dev
```

### Building

```
git clone https://github.com/dmikushin/bitrader.git
cd bitrader
mkdir build
cd build/
cmake ..
./bitrader
```

