require "urn_core"
require_relative "../../uranus/trader/ab3"

mode = 'ABC'
pair = 'USDT-ADA'
markets = ['Binance', 'Kraken', 'Bittrex']
mode = mode.chars.uniq
trader = MarketArbitrageTrader.new run_mode:mode, pair:pair, markets:markets
raise "Should be in dry_run mode" unless trader.dry_run
trader.enable_c_urn_core()
trader.prepare()
trader.start()
