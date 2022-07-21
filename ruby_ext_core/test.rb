require "urn_core"
require_relative "../../uranus/common/old"

class RubyNative
  include URN::MathUtil_RB
  def initialize
    @hashmap = {'i'=>'123456'}
    @i = @hashmap['i']
  end
  def hashmap_get(key)
    return @hashmap[key]
  end
  def hashmap_set(key, value)
    @hashmap[key] = value
  end
  def i
    @i
  end
end

class RubyCExt
  include URN_CORE
  def initialize
    @order_c = URN_CORE::Order.new
  end
  def hashmap_get(key)
    return @order_c[key]
  end
  def i
    return @order_c['i']
  end
end

class Tester
  def assert(a, b)
    return if a == b
    raise "#{a.inspect} should equals #{b.inspect}"
  end

  def test_and_benchmark(name, args, times)
    test(name, args)
    benchmark(name, args, times)
  end

  def test(name, args)
    name = name.to_sym
    print "\ttest #{name.inspect} #{args.inspect}"
    @r_ver = RubyNative.new
    @c_ver = RubyCExt.new
    assert(
      @r_ver.send(name, *args),
      @c_ver.send(name, *args)
    )
    print " √\n"
  end

  def benchmark(name, args, times)
    t = Time.now.to_f
    times.times { @r_ver.send(name, *args) }
    rt = t = Time.now.to_f - t
    puts "#{name} R version #{times} times #{t.round(5)} sec"

    t = Time.now.to_f
    times.times { @c_ver.send(name, *args) }
    ct = t = Time.now.to_f - t
    faster = (rt/ct).round(1)
    puts "#{name} C version #{times} times #{t.round(5)} sec, #{faster}X"
  end
end

t = Tester.new
t.test(:format_num, [0.88, 8, 4])
t.test(:format_num, [0.88, 4, 8])
t.test(:format_num, [8888, 8, 4])
t.test_and_benchmark(:format_num, [1.88, 8, 4], 100_000)

t.test(:diff, [8, 4])
t.test(:diff, [4, 8.88])
t.test_and_benchmark(:diff, [3.88, 8.33], 100_000)

t.test(:stat_array, [[1,2,3,4,5,6,7,9.0123]])
t.test(:stat_array, [[1]])
t.test(:stat_array, [[]])
t.test_and_benchmark(:stat_array, [[1,2,3,4,5,6,7,9.0123]], 100_000)

t.test(:rough_num, [1234.789])
t.test(:rough_num, [12.7896789])
t.test(:rough_num, [0.78967986])
t.test(:rough_num, [0.007896789])
t.test(:rough_num, [0.000007896789])
t.test_and_benchmark(:rough_num, [0.000007896789], 1_000_000)

t.test_and_benchmark(:hashmap_get, ['i'], 1_000_000)
t.test_and_benchmark(:i, [], 1_000_000)
