require "urn_core"
require "json"
require "date"
require "colorize"

require_relative "../../uranus/common/bootstrap" # source env.sh first
require_relative "../../uranus/common/old"

class Base
  def self.make_samples(num)
    pairs = ["USD-BTC", "USD-ETH", "USD-LTC", "USD-XRP", "USD-BCH"]
    statuses = ["new", "canceled"]
    types = ["buy", "sell"]
    markets = ["Gemini", "Binance", "OKX", "HashkeyG", "Bybit"]

    samples = []

    num.times do |i|
      s = (rand * 10).round(8)
      order = {
        "i" => "id_#{sprintf('%05d', i+1)}",
        "pair" => pairs.sample,
        "status" => statuses.sample,
        "T" => types.sample,
        "market" => markets.sample,
        "p" => (rand * 100000).round(2),
        "s" => s,
        "remained" => [(rand * 500).round(8), s].min.round(8),
        "t" => 1609459200 + i * 60
      }
      # 20% of the orders are filled.
      if rand < 0.2
        order['remained'] = 0
      end
      order['status'] = "filled" if order['remained'] == 0
      order["executed"] = (order['s'] - order['remained']).round(8)
      order["maker_size"] = (rand * order["executed"]).round(8)

      # Add optional fields to 20% of the orders
      if rand < 0.2
        order["_alive"] = (order['status'] == 'new')
        order["_cancelled"] = (order['status'] == 'canceled')
      end

      # Add 'v' field to 25% of the orders
      if rand < 0.25
        order["v"] = (order["p"] * order["s"]).round
        order["executed_v"] = (order["p"] * order["executed"]).round
      end

      # Add optional client_oid, avg_price, fee, maker_size fields to some orders
      order["client_oid"] = "oid_#{sprintf('%05d', rand(10000))}" if rand < 0.3
      order["avg_price"] = (rand * 100000).round(2) if rand < 0.3
      order["fee"] = (rand * 100).round(2) if rand < 0.3
      order["maker_size"] = (rand * 1000).round(2) if rand < 0.3

      order['_data'] = {
        'fee' => {
          'maker/buy' => 0,
          'taker/buy' => 0.0001,
          'maker/sell' => 0,
          'taker/sell' => 0.0001
        }
      }

      samples << order
    end

    samples
  end

  def initialize(samples)
    @samples = samples
  end
  def samples
    @samples
  end

  def o_hashmap_get(key)
    @samples.map { |o| o[key] }
  end
  def o_hashmap_set(key, value)
    @samples.map { |o| o[key] = value }
  end
  def o_to_json
    @samples.map { |o| o.to_json }
  end
  def o_clone
    @samples.map { |o| o.clone }
  end
  def o_from_hash
    @samples.map { |o| o.clone }
  end
  def o_to_h
    @samples.map { |o| o.to_h }
  end
  def o_format
    @samples.map { |o| [format_order(o), format_trade(o)].join("\n").uncolorize }
  end
  def o_alive?
    @samples.map { |o| [order_alive?(o), order_cancelled?(o)] }
  end
  def o_set_dead
    @samples.map { |o| order_set_dead(o); o; }
  end
  def o_age
    @samples.map { |o| order_age(o) > 1609459200 } # Allow some time diff
  end
  def o_full_filled
    @samples.map { |o| [order_full_filled?(o), order_full_filled?(o, omit_size: 0.001)] }
  end
  def o_status_evaluate
    @samples.map { |o| order_status_evaluate(o); o }
  end
  def o_same
    @samples.map { |o| order_same?(o, o.clone) }
  end
  def o_changed
    @samples.map { |o| order_changed?(o, o.clone) }
  end
  def o_should_update
    @samples.map { |o| o1 = o.clone; order_should_update?(o, o1) }
  end
  def o_stat
    order_stat(@samples)
  end
  def o_real_vol
    @samples.map { |o| order_real_vol(o) }
  end
  def o_same_mkt_pair
    @samples.map { |o| order_same_mkt_pair?([o, o.clone, o.clone, o.clone]) }
  end
end

class RubyNative < Base
  include URN::MathUtil_RB
  include URN::OrderUtil_RB
end
class RubyCExt < Base
  include URN::MathUtil
  include URN::OrderUtil
end

class Tester
  def assert(x, y, opt={})
    samples = opt[:samples] || []
    if x.is_a?(Array) && y.is_a?(Array)
      x.size.times { |i|
        a = x[i]
        b = y[i]
        next if a == b
        if a.is_a?(Hash) && b.is_a?(Hash)
          # Ignore key=nil cases
          a.keys.each { |k| a.delete(k) if b[k].nil? && a[k].nil? }
          b.keys.each { |k| b.delete(k) if b[k].nil? && a[k].nil? }
        end
        next if a == b
        puts "\nSample [#{i}] #{samples[i].inspect}"
        puts "\nExpect     #{a.inspect}\nGot        #{b.inspect}\n"
        raise "asset error"
      }
    else
      return if x == y
      a = x
      b = y
      puts "\nExpect #{a.inspect}\nGot    #{b.inspect}\n"
      raise "Expect #{a.inspect}\nGot    #{b.inspect}\n"
    end
  end

  def test_and_benchmark(name, args, times)
    test(name, args, benchmark: times)
  end

  def test(name, args, opt={})
    name = name.to_sym
    print "prep #{name.inspect} #{args.inspect}..."

    times = opt[:benchmark] || 1

    use_samples = name.to_s.start_with?('o_')
    samples = Base.make_samples(use_samples ? times : 1)
    @r_ver = RubyNative.new(JSON.parse(samples.to_json))
    @c_ver = RubyCExt.new(JSON.parse(samples.to_json))

    print "\rTest #{name.inspect} #{args.inspect} C ver"
    t = Time.now.to_f
    if use_samples
      c_result = @c_ver.send(name, *args)
    else
      times.times { c_result = @c_ver.send(name, *args) }
    end
    ct = t = Time.now.to_f - t
    # puts "#{name} C version #{times} times #{t.round(5)} sec, #{faster}X"

    print "\rTest #{name.inspect} #{args.inspect} R ver"
    t = Time.now.to_f
    if use_samples
      r_result = @r_ver.send(name, *args)
    else
      times.times { r_result = @r_ver.send(name, *args) }
    end
    rt = t = Time.now.to_f - t
    # puts "#{name} R version #{times} times #{t.round(5)} sec"

    assert(r_result, c_result, samples: samples)
    print "\rTest #{name.inspect} #{args.inspect} √ "

    faster = (rt/ct).round(1)
    if faster > 1.1
      print " \033[32m#{faster} x\033[0m\n"
    elsif faster < 1
      print " \033[31m#{faster} x\033[0m\n"
    else
      print " \033[33m#{faster} x\033[0m\n"
    end
  end
end

t = Tester.new

t.test_and_benchmark(:o_same_mkt_pair, [], 200_000)
t.test_and_benchmark(:o_real_vol, [], 200_000)
t.test_and_benchmark(:o_stat, [], 20_000)
t.test_and_benchmark(:o_should_update, [], 200_000)
t.test_and_benchmark(:o_changed, [], 200_000)
t.test_and_benchmark(:o_same, [], 200_000)
t.test_and_benchmark(:o_status_evaluate, [], 200_000)
t.test_and_benchmark(:o_age, [], 200_000)
t.test_and_benchmark(:o_full_filled, [], 200_000)
t.test_and_benchmark(:o_format, [], 200_000)
t.test_and_benchmark(:o_set_dead, [], 200_000)
t.test_and_benchmark(:o_alive?, [], 200_000)

t.test_and_benchmark(:parse_contract, ["USDT-BTC@20240812"], 1_000_000)
t.test_and_benchmark(:parse_contract, ["USDT-BTC"], 1_000_000)

t.test_and_benchmark(:pair_assets, ["USDT-BTC"], 1_000_000)
t.test_and_benchmark(:pair_assets, ["USDT-BTC@20240812"], 1_000_000)

t.test_and_benchmark(:is_future?, ["USDT-BTC"], 1_000_000)
t.test_and_benchmark(:is_future?, ["USDT-BTC@20240812"], 1_000_000)

t.test_and_benchmark(:o_from_hash, [], 100_000)
t.test_and_benchmark(:o_to_h, [], 100_000)
t.test_and_benchmark(:o_clone, [], 100_000)

t.test_and_benchmark(:o_hashmap_get, ['i'], 100_000)
t.test_and_benchmark(:o_hashmap_set, ['i', '998877'], 100_000)
t.test_and_benchmark(:o_to_json, [], 100_000)

t.test(:rough_num, [1234.789])
t.test(:rough_num, [12.7896789])
t.test(:rough_num, [0.78967986])
t.test(:rough_num, [0.007896789])
t.test(:rough_num, [0.000007896789])
t.test_and_benchmark(:rough_num, [0.000007896789], 1_000_000)

t.test(:stat_array, [[1]])
t.test(:stat_array, [[]])
t.test_and_benchmark(:stat_array, [[1,2,3,4,5,6,7,9.0123]], 100_000)

t.test(:diff, [8, 4])
t.test(:diff, [4, 8.88])
t.test_and_benchmark(:diff, [3.88, 8.33], 100_000)

t.test(:format_num, [0.0, 8, 4])
t.test(:format_num, [0.88, 8, 4])
t.test(:format_num, [0.88, 4, 8])
t.test(:format_num, [8888, 8, 4])
t.test(:format_num, [9.00000034, 5, 5])
t.test(:format_num, [81176.0, 10, 5])
t.test_and_benchmark(:format_num, [1.88, 8, 4], 100_000)
