require "urn_core"
require "json"
require "date"
require "colorize"
require_relative "../../uranus/common/old"

class RubyNative
  include URN::MathUtil_RB
  include URN::OrderUtil_RB
  def initialize
    @o = {"i"=>"123456", "client_oid"=>"", "pair"=>"", "asset"=>"", "currency"=>"", "status"=>"", "T"=>"", "market"=>"", "p"=>0.0, "s"=>0.0, "remained"=>0.0, "executed"=>0.0, "avg_price"=>0.0, "fee"=>0.0, "maker_size"=>0.0, "t"=>0, "_status_cached" => false}
    @o['p'] = 66666.66
    @o['s'] = 0.1
    @o['maker_size'] = 0.1
    @o['executed'] = 0.0123
    @o['T'] = 'buy'
    @o['t'] = 10000000000
    @o['market'] = 'market'
    @o['status'] = 'new'
  end
  def o_hashmap_get(key)
    return @o[key]
  end
  def o_hashmap_set(key, value)
    @o[key] = value
  end
  def o_to_json
    JSON.pretty_generate(@o)
  end
  def o_clone
    @o.clone()['i']
  end
  def o_from_hash
    @o.clone
    nil
  end
  def o_to_h
    @o.to_h
  end
  def format_o
    [format_order(@o), format_trade(@o)].join("\n").uncolorize
  end
  def o_alive?
    order_alive?(@o)
    order_cancelled?(@o)
  end
  def o_set_dead
    order_set_dead(@o)
    @o['_alive'] = nil
    true
  end
end

class RubyCExt
  include URN_CORE::MathUtil
  include URN_CORE::OrderUtil
  def initialize
    @o = URN_CORE::Order.new
    @o['i'] = '123456'
    @o['p'] = 66666.66
    @o['executed'] = 0.0123
    @o['s'] = 0.1
    @o['maker_size'] = 0.1
    @o['T'] = 'buy'
    @o['t'] = 10000000000
    @o['market'] = 'market'
    @o['status'] = 'new'
    @o_hash = @o.to_h
  end
  def o_hashmap_get(key)
    @o[key]
  end
  def o_hashmap_set(key, value)
    @o[key] = value
  end
  def o_to_json
    JSON.pretty_generate(@o)
  end
  def o_clone
    @o.clone['i']
  end
  def o_from_hash
    URN_CORE::Order.from_hash(@o_hash)
    nil
  end
  def o_to_h
    @o.to_h
  end
  def format_o
    [format_order(@o), format_trade(@o)].join("\n").uncolorize
  end
  def o_alive?
    order_alive?(@o)
    order_cancelled?(@o)
  end
  def o_set_dead
    order_set_dead(@o)
    @o['_status_cached'] = false
    true
  end
end

class Tester
  def assert(a, b)
    return if a == b
    puts "\nExpect #{a.inspect}\nGot    #{b.inspect}\n"
    raise "Expect #{a.inspect}\nGot    #{b.inspect}\n"
  end

  def test_and_benchmark(name, args, times)
    test(name, args, benchmark: times)
  end

  def test(name, args, opt={})
    name = name.to_sym
    print "Test #{name.inspect} #{args.inspect}"
    @r_ver = RubyNative.new
    @c_ver = RubyCExt.new
    assert(
      @r_ver.send(name, *args),
      @c_ver.send(name, *args)
    )
    print " √ "

    benchmark_times = opt[:benchmark] || 0
    if benchmark_times == 0
      print "\n"
      return
    end

    times = benchmark_times
    t = Time.now.to_f
    times.times { @r_ver.send(name, *args) }
    rt = t = Time.now.to_f - t
    # puts "#{name} R version #{times} times #{t.round(5)} sec"

    t = Time.now.to_f
    times.times { @c_ver.send(name, *args) }
    ct = t = Time.now.to_f - t
    faster = (rt/ct).round(1)
    # puts "#{name} C version #{times} times #{t.round(5)} sec, #{faster}X"
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

t.test_and_benchmark(:o_set_dead, [], 100_000)
t.test_and_benchmark(:o_alive?, [], 100_000)
t.test_and_benchmark(:format_o, [], 10_000)

t.test_and_benchmark(:parse_contract, ["USDT-BTC@20240812"], 1_000_000)
t.test_and_benchmark(:parse_contract, ["USDT-BTC"], 1_000_000)

t.test_and_benchmark(:pair_assets, ["USDT-BTC"], 1_000_000)
t.test_and_benchmark(:pair_assets, ["USDT-BTC@20240812"], 1_000_000)

t.test_and_benchmark(:is_future?, ["USDT-BTC"], 1_000_000)
t.test_and_benchmark(:is_future?, ["USDT-BTC@20240812"], 1_000_000)

t.test_and_benchmark(:o_from_hash, [], 100_000)
t.test_and_benchmark(:o_to_h, [], 100_000)
t.test_and_benchmark(:o_clone, [], 100_000)

t.test_and_benchmark(:o_hashmap_get, ['i'], 3_000_000)
t.test_and_benchmark(:o_hashmap_set, ['i', '998877'], 3_000_000)
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

t.test(:format_num, [0.88, 8, 4])
t.test(:format_num, [0.88, 4, 8])
t.test(:format_num, [8888, 8, 4])
t.test_and_benchmark(:format_num, [1.88, 8, 4], 100_000)
