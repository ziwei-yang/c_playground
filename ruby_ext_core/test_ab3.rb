require "urn_core"
require_relative "../../uranus/trader/ab3"

def compare_ab3_chances(cata, m, cr, cc)
  puts "chances_by_mkt   #{cata} / #{m} /:"
  # Must be exactly same.
  match = true
  [:cata, :price, :type, :market, :child_type].each { |k|
    if cr[k] == cc[k]
#       puts "\t#{k.to_s.ljust(24)} #{cr[k]}"
    else
      puts "\t#{k.to_s.ljust(24)} #{cr[k]} RUBY #{cc[k]} C".red
      match = false
    end
  }
  # Could be slightly different.
  [
    [:child_price_threshold, 0.001],
    [:ideal_profit, 0.2],
    [:p_real, 0.001],
    [:suggest_size, 0.2]
  ].each { |k, error|
    if cr[k] == 0 && cc[k] == 0
#       puts "\t#{k.to_s.ljust(24)} #{cr[k]}"
    elsif diff(cr[k], cc[k]) <= error
      puts "\t#{k.to_s.ljust(24)} #{cr[k]} RUBY #{cc[k]} C"
    else
      puts "\t#{k.to_s.ljust(24)} #{cr[k]} RUBY #{cc[k]} C".red
      match = false
    end
  }
  # Order num should be same
  if cr[:main_orders].size != cc[:main_orders].size
    k = :main_orders
    puts "\t#{k.to_s.ljust(24)} #{JSON.pretty_generate(cr[:main_orders])}".yellow
    puts "\t#{k.to_s.ljust(24)} #{JSON.pretty_generate(cr[:main_orders])}".green
    return false
  end
  cr[:main_orders].each_with_index { |o_r, i|
    o_c = cc[:main_orders][i]
    k = "main_orders / #{i}"
    # Must be exactly same
    ['pair', 'market', 'T'].each { |ok|
      if o_c[ok] == o_r[ok]
#         puts "\t#{k.to_s.ljust(24)} #{ok.ljust(8)} #{o_r[ok]}"
      else
        puts "\t#{k.to_s.ljust(24)} #{ok.ljust(8)} #{o_r[ok]} RUBY #{o_c[ok]} C".red
        match = false
      end
    }
    # Could be slightly different.
    [ ['p', 0], ['s', 0.2] ].each { |ok, error|
      if diff(o_c[ok], o_r[ok]) == 0
        puts "\t#{k.to_s.ljust(24)} #{ok.ljust(8)} #{o_r[ok]}"
      elsif diff(o_c[ok], o_r[ok]) <= error
        puts "\t#{k.to_s.ljust(24)} #{ok.ljust(8)} #{o_r[ok]} RUBY #{o_c[ok]} C"
      else
        puts "\t#{k.to_s.ljust(24)} #{ok.ljust(8)} #{o_r[ok]} RUBY #{o_c[ok]} C".red
        match = false
      end
    }
  }
  return match
end

mode = 'ABC'
# pair = 'USDT-AAVE'
# markets = ['Binance', 'Bittrex', 'FTX']
pair = 'BTC-LINK'
markets = ['Binance', 'Bittrex', 'FTX', 'Kraken', 'Gemini']
mode = mode.chars.uniq
trader = MarketArbitrageTrader.new run_mode:mode, pair:pair, markets:markets
raise "Should be in dry_run mode" unless trader.dry_run
trader.enable_c_urn_core()
# trader.enable_debug()
trader.define_singleton_method(:compare_urncore_result) { |chances_by_mkt_r, chances_by_mkt_c|
#   return
  print "\n"
#   root = ENV['URANUS_RAMDISK']
  f = "/Volumes/RAMDisk/test_ab3.#{pair}_snapshot.json"
  File.open(f, "w") { |f|
    f.write(JSON.pretty_generate([
        @market_snapshot,
        (markets.map { |m| [m, market_client(m).balance_cache()] })
    ]))
  }
  puts "F -> #{f}"
  next # skip checking
  (chances_by_mkt_r.keys | chances_by_mkt_c.keys).each { |cata|
    chances_by_mkt_r[cata] ||= {}
    chances_by_mkt_c[cata] ||= {}
    (chances_by_mkt_r[cata].keys | chances_by_mkt_c[cata].keys).each { |m|
      cr = chances_by_mkt_r[cata][m] || {}
      cc = chances_by_mkt_c[cata][m] || {}
      ret = compare_ab3_chances(cata, m, cr, cc)
      raise "Hey check this!" if ret != true
    }
  }
}
trader.prepare()
trader.start()

# Compare C & Ruby version, when error:
#   Save market snapshot, market balance cache
