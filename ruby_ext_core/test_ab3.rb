require "urn_core"
require_relative "../../uranus/trader/ab3"

def check_c_data(res)
  # Check data is valid to use
  if res.is_a?(Hash)
    puts "check_c_data hash"
    res.each { |k, v|
      check_c_data(k)
      check_c_data(v)
    }
  elsif res.is_a?(Array)
    puts "check_c_data array"
    res.each { |r| check_c_data(r) }
  elsif res.is_a?(String)
    puts "check_c_data string"
    puts res
    res_d = res.dup
  elsif res.is_a?(Symbol)
    puts "check_c_data symbol"
    res_d = res.to_s.dup
  elsif res.nil?
    ;
  elsif res == true
    ;
  elsif res == false
    ;
  else
    puts "check_c_data number"
    puts res
    res_d = res+1 # check number functions
  end
  res_j = res.to_json # Check data structure
  puts res_j
end

def compare_ab3_chances(cata, m, cr, cc)
  puts "chances_by_mkt   #{cata} / #{m} /:"
  # Must be exactly same.
  match = true
  [:cata, :price, :type, :market, :child_type].each { |k|
    if cr[k] == cc[k]
      puts "\t#{k.to_s.ljust(24)} #{cr[k]}"
    else
      puts "\t#{k.to_s.ljust(24)} #{cr[k]} RUBY #{cc[k]} C XX".red
      match = false
    end
  }
  # Could be slightly different.
  [
    [:child_price_threshold, 0.001],
    [:ideal_profit, 0.3],
    [:p_real, 0.001],
    [:suggest_size, 0.3]
  ].each { |k, error|
    if cr[k] == 0 && cc[k] == 0
      puts "\t#{k.to_s.ljust(24)} #{cr[k]}"
    elsif diff(cr[k], cc[k]) <= error
      puts "\t#{k.to_s.ljust(24)} #{cr[k]} RUBY #{cc[k]} C"
    else
      puts "\t#{k.to_s.ljust(24)} #{cr[k]} RUBY #{cc[k]} C XX".red
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
        puts "\t#{k.to_s.ljust(24)} #{ok.ljust(8)} #{o_r[ok]}"
      else
        puts "\t#{k.to_s.ljust(24)} #{ok.ljust(8)} #{o_r[ok]} RUBY #{o_c[ok]} C XX".red
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
        puts "\t#{k.to_s.ljust(24)} #{ok.ljust(8)} #{o_r[ok]} RUBY #{o_c[ok]} C XX".red
        match = false
      end
    }
  }
  return match
end

# Check Object.STR_XXX defined as XXX
[
  "sell", "buy", "T", "p", "s", "pair", "market", "p_take", "i",
  "status", "executed", "remained", "v", "remained_v",
  "executed_v", "t", "_alive"
].each { |s|
  sym = "STR_#{s}".to_sym
  raise "Object::STR_#{s} not defined" unless Object.const_defined?(sym)
  cst = Object.const_get(sym)
  raise "Object::STR_#{s} #{cst} != #{s}" unless cst == s
}

mode = 'ABC'
# pair = 'USDT-BAL'
# markets = ['Binance', 'Bittrex', 'FTX']
# pair = 'BTC-LINK'
# markets = ['Binance', 'Bittrex', 'FTX', 'Kraken', 'Gemini']
pair = 'USD-ETH'
markets = ['Binance', 'Bittrex', 'FTX', 'Kraken', 'Gemini']
mode = mode.chars.uniq
trader = MarketArbitrageTrader.new run_mode:mode, pair:pair, markets:markets
raise "Should be in dry_run mode" unless trader.dry_run

trader.enable_debug()
trader.define_singleton_method(:inspect_c_data) { |data|
  puts "check_c_data called"
  check_c_data(data)
}

trader.define_singleton_method(:compare_urncore_result) { |chances_by_mkt_r, chances_by_mkt_c, detect_c_t|
  return unless @debug
  print "\n"
  f = "/Volumes/RAMDisk/test_ab3.#{pair}_snapshot.json"
  File.open(f, "w") { |f|
    f.write(JSON.pretty_generate([
        @market_snapshot,
        (markets.map { |m| [m, market_client(m).balance_cache()] })
    ]))
  }
  puts "F -> #{f}"
  match = true
  missing_spike_ct = 0 # Freq control: Allow <= market.size*2 missing spikes
  all_catas = (chances_by_mkt_r.keys | chances_by_mkt_c.keys)
  cata_by_m = {}
  all_catas.each { |cata|
    chances_by_mkt_r[cata] ||= {}
    chances_by_mkt_c[cata] ||= {}
    (chances_by_mkt_r[cata].keys | chances_by_mkt_c[cata].keys).each { |m|
      cr = chances_by_mkt_r[cata][m]
      cc = chances_by_mkt_c[cata][m]
      cata_by_m[m] ||= {}
      cata_by_m[m][cata] = 1
      if (cr.nil? && cc != nil)
        puts "Hey check this!".red
        puts "RUBY nil but C has cata #{cata} at #{m}: #{JSON.pretty_generate(cc)}"
        if cata =~ /^S(B|C)[2-9]$/
          missing_spike_ct += 1 # maybe it is okay
          next
        end
        match = false
      elsif (cc.nil? && cr != nil)
        puts "Hey check this!".red
        puts "C nil but Ruby has cata #{cata} at #{m}: #{JSON.pretty_generate(cr)}"
        if cata =~ /^S(B|C)[2-9]$/
          missing_spike_ct += 1 # maybe it is okay
          next
        end
        match = false
      else
        ret = compare_ab3_chances(cata, m, cr, cc)
        next if ret
        puts "Hey check above!".red
        match = false
      end
    }
  }
  if missing_spike_ct > 2*(markets.size*2)
    puts "Too many missing spikes #{missing_spike_ct}"
    match = false
  end
  raise "Not match" unless match
  puts "detect_c_t #{detect_c_t} cata #{cata_by_m.map { |m_cv| "#{m_cv[0]}/#{m_cv[1].keys.join(',')}" }.join(' ')}"
}
trader.prepare()
trader.start()

# Compare C & Ruby version, when error:
#   Save market snapshot, market balance cache
