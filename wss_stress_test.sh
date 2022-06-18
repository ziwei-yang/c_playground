#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

cd $DIR

if [[ $1 == bybitu ]]; then
	export uranus_spider_exchange=bybitu
	$DIR/run.sh byb_ws.c usdt-ada@p usdt-xem@p usdt-bnb@p usdt-aave@p usdt-etc@p usdt-doge@p usdt-eos@p usdt-xrp@p usdt-matic@p usdt-xtz@p usdt-sol@p usdt-comp@p usdt-trx@p usdt-xlm@p usdt-theta@p usdt-avax@p usdt-algo@p usdt-sushi@p usdt-icp@p usdt-ftm@p usdt-atom@p usdt-dydx@p usdt-iost@p usdt-bit@p usdt-ftt@p usdt-link@p usdt-dash@p usdt-shib1000@p usdt-fil@p usdt-near@p usdt-hbar@p usdt-celr@p usdt-one@p usdt-c98@p usdt-alice@p usdt-ksm@p usdt-srm@p usdt-vet@p usdt-enj@p usdt-sand@p usdt-chz@p usdt-crv@p usdt-ltc@p usdt-dot@p usdt-mana@p usdt-gala@p usdt-ens@p usdt-woo@p usdt-iotx@p usdt-lrc@p usdt-chr@p usdt-slp@p usdt-keep@p usdt-tlm@p usdt-egld@p usdt-storj@p usdt-imx@p usdt-ankr@p usdt-flow@p usdt-bal@p usdt-uni@p usdt-ilv@p usdt-bat@p usdt-gtc@p usdt-lit@p usdt-rsr@p usdt-rndr@p usdt-cvc@p usdt-sc@p usdt-axs@p usdt-stx@p usdt-iota@p usdt-cro@p usdt-kava@p usdt-1inch@p usdt-dent@p usdt-audio@p usdt-waves@p usdt-cream@p usdt-ape@p usdt-coti@p usdt-sxp@p usdt-grt@p usdt-dusk@p usdt-knc@p usdt-kda@p usdt-anc@p usdt-hot@p usdt-zec@p usdt-rose@p usdt-celo@p usdt-zrx@p usdt-klay@p usdt-jst@p usdt-neo@p usdt-bsw@p usdt-bake@p usdt-glmr@p usdt-zil@p usdt-spell@p usdt-gmt@p usdt-astr@p usdt-jasmy@p usdt-fxs@p usdt-bnx@p usdt-1000btt@p usdt-cvx@p usdt-gal@p usdt-ar@p usdt-ren@p
elif [[ $1 == 'ftx' ]]; then
	$DIR/run.sh ftx_ws.c doge link ltc trx uni usdt-aave usdt-bal usd-ltc usd-1inch usd-cvc usd-aave usd-bal usd-bat usd-comp usd-link usd-storj usd-uni usd-zrx usd-ftt usd-fxs usdt-cream usdt-ftt usdt-avax usdt-comp usdt-doge usdt-eth usdt-ltc usdt-link usdt-trx usdt-uni usdt-xrp usd-trx usd-cro usd-boba usd-usdt usd-btt usd-ape usd-imx usd-gmt usd-slp usd-cream usd-sos usd-ksos xrp eth usdt-btc usd-btc usd-eth usd-xrp usd-doge usd-avax usd-atom usdt-atom usd-btc@p usd-bnb@p usd-ada@p usd-eth@p usd-neo@p usd-grt@p usd-imx@p usd-kshib@p usd-step usd-dent usd-cvc@p usd-xmr@p usd-stx@p usd-sc@p usd-1inch@p usd-axs@p usd-audio@p usd-celo@p usd-ltc@p usd-bat@p usd-ar@p usd-xlm@p usd-slp@p usd-algo@p usd-ren@p usd-icx@p usd-one@p usd-ftm@p usd-icp@p usd-comp@p usd-eos@p usd-doge@p usd-trx@p usd-matic@p usd-alcx@p usd-kava@p usd-rndr@p usd-clv@p usd-okb@p usd-scrt@p usd-uni@p usd-anc@p usd-rose@p usd-spell@p usd-gal@p usd-c98@p usd-hbar@p usd-dent@p usd-alice@p usd-enj@p usd-cro@p usd-flow@p usd-perp@p usd-flm@p usd-hnt@p usd-sand@p usd-chz@p usd-crv@p usd-dot@p usd-srm@p usd-ens@p usd-zec@p usd-chr@p usd-iotx@p usd-btt@p usd-kbtt@p usd-lrc@p usd-boba@p usd-tlm@p usd-gala@p usd-hot@p usd-reef@p usd-waves@p usd-cream@p usd-ape@p usd-sushi@p usd-knc@p usd-glmr@p usd-zil@p usd-jasmy@p usd-ksos@p usd-xtz@p usd-mana@p usd-egld@p usd-tru@p usd-agld@p usd-sol@p usd-vet@p usd-theta@p usd-badger@p usd-avax@p usd-fil@p usd-link@p usd-dydx@p usd-bit@p usd-near@p usd-aave@p usd-atom@p usd-storj@p usd-bal@p usd-iota@p usd-xem@p usd-zrx@p usd-pundix@p usd-xrp@p usd-rsr@p usd-dash@p usd-paxg@p usd-mina@p usd-sxp@p usd-gmt@p usd-fxs@p usd-cvx@p usd-step@p
elif [[ $1 == 'binance' ]]; then
	$DIR/run.sh bnn_ws.c 1inch aave ada ardr ark atom bal bat bnb comp cvc dash dnt doge eos eth firo iota link lsk ltc matic sc storj strax trx uni xem xmr xrp zec zrx busd-1inch busd-aave busd-ada busd-alcx busd-algo busd-alice busd-anc busd-ape busd-ar busd-astr busd-atom busd-audio busd-avax busd-axs busd-badger busd-bake busd-bal busd-bat busd-bnb busd-bnx busd-bsw busd-btc busd-btt busd-c98 busd-celo busd-celr busd-chr busd-chz busd-clv busd-comp busd-coti busd-cream busd-crv busd-cvc busd-cvx busd-dash busd-dent busd-dnt busd-doge busd-dot busd-dusk busd-dydx busd-egld busd-enj busd-ens busd-eos busd-etc busd-eth busd-fil busd-flm busd-flow busd-ftm busd-ftt busd-fxs busd-gala busd-glmr busd-gmt busd-grt busd-gtc busd-hbar busd-hnt busd-hot busd-icp busd-icx busd-ilv busd-imx busd-iost busd-iota busd-iotx busd-jasmy busd-jst busd-kava busd-kda busd-keep busd-klay busd-knc busd-ksm busd-link busd-lit busd-lrc busd-lsk busd-ltc busd-mana busd-matic busd-mina busd-near busd-neo busd-nu busd-one busd-paxg busd-perp busd-pundix busd-ren busd-rndr busd-rose busd-rsr busd-sand busd-sc busd-scrt busd-shib busd-slp busd-sol busd-spell busd-srm busd-storj busd-stx busd-sushi busd-sxp busd-theta busd-tlm busd-tru busd-trx busd-uni busd-usdc busd-vet busd-woo busd-xlm busd-xmr busd-xrp busd-xtz busd-zec busd-zil busd-zrx eth-aave eth-ada eth-atom eth-bat eth-dash eth-eos eth-iota eth-link eth-lsk eth-ltc eth-sc eth-storj eth-strax eth-trx eth-uni eth-xem eth-xmr eth-xrp eth-zec eth-zrx trx-btt usdt-1inch usdt-aave usdt-ada usdt-alcx usdt-algo usdt-alice usdt-anc usdt-ape usdt-ar usdt-ardr usdt-astr usdt-atom usdt-audio usdt-avax usdt-axs usdt-badger usdt-bake usdt-bal usdt-bat usdt-bnb usdt-bnx usdt-bsw usdt-btc usdt-btt usdt-bttc usdt-busd usdt-c98 usdt-celo usdt-celr usdt-chr usdt-chz usdt-clv usdt-comp usdt-coti usdt-crv usdt-cvc usdt-cvx usdt-dash usdt-dent usdt-dnt usdt-doge usdt-dot usdt-dusk usdt-dydx usdt-egld usdt-enj usdt-ens usdt-eos usdt-etc usdt-eth usdt-fil usdt-firo usdt-flm usdt-flow usdt-ftm usdt-ftt usdt-fxs usdt-gala usdt-glmr usdt-gmt usdt-grt usdt-gtc usdt-hbar usdt-hnt usdt-hot usdt-icp usdt-icx usdt-ilv usdt-imx usdt-iost usdt-iota usdt-iotx usdt-jasmy usdt-jst usdt-kava usdt-kda usdt-keep usdt-klay usdt-knc usdt-ksm usdt-link usdt-lit usdt-lrc usdt-lsk usdt-ltc usdt-mana usdt-matic usdt-mina usdt-near usdt-neo usdt-nu usdt-one usdt-paxg usdt-perp usdt-pundix usdt-ren usdt-rndr usdt-rose usdt-rsr usdt-sand usdt-sc usdt-scrt usdt-shib usdt-slp usdt-sol usdt-spell usdt-srm usdt-storj usdt-stx usdt-sushi usdt-sxp usdt-theta usdt-tlm usdt-tru usdt-trx usdt-uni usdt-usdc usdt-vet usdt-woo usdt-xem usdt-xlm usdt-xmr usdt-xrp usdt-xtz usdt-zec usdt-zil usdt-zrx
elif [[ $1 == 'bnum' ]]; then
	export uranus_spider_exchange=BNUM
	$DIR/run.sh bnn_ws.c busd-ada@p busd-anc@p busd-ape@p busd-avax@p busd-doge@p busd-eth@p busd-ftm@p busd-ftt@p busd-gal@p busd-gmt@p busd-near@p busd-sol@p busd-trx@p busd-xrp@p usdt-1000shib@p usdt-1inch@p usdt-ada@p usdt-algo@p usdt-ankr@p usdt-ape@p usdt-ar@p usdt-atom@p usdt-avax@p usdt-bake@p usdt-bal@p usdt-bnb@p usdt-bnx@p usdt-celr@p usdt-coti@p usdt-cvc@p usdt-dent@p usdt-doge@p usdt-dusk@p usdt-etc@p usdt-eth@p usdt-flm@p usdt-flow@p usdt-ftm@p usdt-ftt@p usdt-gal@p usdt-gala@p usdt-gmt@p usdt-grt@p usdt-hbar@p usdt-iost@p usdt-iota@p usdt-jasmy@p usdt-kava@p usdt-klay@p usdt-knc@p usdt-link@p usdt-mana@p usdt-matic@p usdt-near@p usdt-nu@p usdt-one@p usdt-reef@p usdt-ren@p usdt-sand@p usdt-sol@p usdt-spell@p usdt-storj@p usdt-sushi@p usdt-sxp@p usdt-trx@p usdt-waves@p usdt-xem@p usdt-xmr@p usdt-xrp@p usdt-zec@p usdt-zil@p usdt-zrx@p
elif [[ $1 == 'bncm' ]]; then
	export uranus_spider_exchange=BNCM
	$DIR/run.sh bnn_ws.c usd-ada@p usd-bnb@p usd-doge@p usd-eos@p usd-sol@p usd-xrp@p
else
	echo "Not supported $@"
	exit 1
fi
