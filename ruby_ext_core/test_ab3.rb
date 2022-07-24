require "urn_core"
require_relative "../../uranus/trader/ab3"

mode = 'ABC'
pair = 'USD-ETH'
markets = ['Binance', 'Kraken', 'Bittrex', 'FTX', 'Gemini']
mode = mode.chars.uniq
trader = MarketArbitrageTrader.new run_mode:mode, pair:pair, markets:markets
raise "Should be in dry_run mode" unless trader.dry_run
trader.enable_c_urn_core()
trader.prepare()
trader.start()

# Compare C & Ruby version, when error:
#   Save market snapshot, market balance cache
