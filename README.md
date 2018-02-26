## BITrader - Trading and Alering Service for Binance Cryptocurrency Exchange

<img src="screenshot.png" width="350"/>

BITrader - Trading and Alering Service for Binance Cryptocurrency Exchange (http://binance.com):

 * Monitor all '\*BTC' symbols trades and detect pumps
 * Send crypto signals to the attached Telegram bot

### Prerequisites

```
sudo apt-get install libjsoncpp-dev libcurl4-nss-dev libwebsockets-dev
sudo apt-get install g++ make binutils cmake libssl-dev libboost-system-dev libboost-iostreams-dev
```

### Building

```
git clone https://github.com/dmikushin/bitrader.git
cd bitrader
git submodule init
git submodule update
mkdir build
cd build/
cmake ..
./bitrader
```

### Liability

Use this program at your own risk. None of the contributors to this project are liable for any loses you may incur. Be wise and always do your own research.
