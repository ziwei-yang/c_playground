require "urn_core"

class RubyNative
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
    print "test #{name.inspect} #{args.inspect}"
    @c_ver = RubyCExt.new
    @r_ver = RubyNative.new
    assert(
      @r_ver.send(name, *args),
      @c_ver.send(name, *args)
    )
    print " √\n"
  end

  def benchmark(name, args, times)
    t = Time.now.to_f
    times.times { @r_ver.send(name, *args) }
    t = Time.now.to_f - t
    puts "#{name} R version #{times} times #{t} sec"

    t = Time.now.to_f
    times.times { @c_ver.send(name, *args) }
    t = Time.now.to_f - t
    puts "#{name} C version #{times} times #{t} sec"
  end
end

t = Tester.new
t.test(:format_num, [0.88, 8, 4])
t.test(:format_num, [0.88, 4, 8])
t.test(:format_num, [8888, 8, 4])
t.test_and_benchmark(:format_num, [1.88, 8, 4], 100_000)
