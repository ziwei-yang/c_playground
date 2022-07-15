require "urn_core"

class RubyNative
  def diff(f1, f2)
    f1 = f1.to_f
    f2 = f2.to_f
    return 9999999 if [f1, f2].min <= 0
    (f1 - f2) / [f1, f2].min
  end

  def stat_array(array)
    return [0,0,0,0] if array.nil? || array.empty?
    n = array.size
    sum = array.reduce(:+)
    mean = sum/n
    deviation = array.map { |p| (p-mean)*(p-mean) }.reduce(:+)/n
    deviation = Math.sqrt(deviation)
    [n, sum, mean, deviation]
  end

  def rough_num(f)
    f ||= 0
    if f.abs > 100
      return f.round
    elsif f.abs > 1
      return f.round(2)
    elsif f.abs > 0.01
      return f.round(4)
    elsif f.abs > 0.0001
      return f.round(6)
    elsif f.abs > 0.000001
      return f.round(8)
    else
      f
    end
  end

  def format_num(f, float=8, decimal=8)
    return ''.ljust(decimal+float+1) if f.nil?
    return ' '.ljust(decimal+float+1, ' ') if f == 0
    return f.rjust(decimal+float+1) if f.is_a? String
    if float == 0
      f = f.round
      return ' '.ljust(decimal+float+1, ' ') if f == 0
      return f.to_s.rjust(decimal+float+1, ' ')
    end
    num = f.to_f
    f = "%.#{float}f" % f
    loop do
      break unless f.include?('.')
      break unless f.end_with?('0')
      break if f.end_with?('.0')
      f = f[0..-2]
    end
    segs = f.split('.')
    if num.to_i == num
      return "#{segs[0].rjust(decimal)} #{''.ljust(float, ' ')}"
    else
      return "#{segs[0].rjust(decimal)}.#{segs[1][0..float].ljust(float, ' ')}"
    end
  end
end

class RubyCExt
  include URN_CORE
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
