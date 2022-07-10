require "urn_mktdata"
include URN_MKTDATA

exch_arr = [
	"Binance","BNCM","BNUM","Bybit",
	"BybitU","Coinbase","FTX","Kraken",
	"Bittrex", "Gemini", "Bitstamp", "BybitS"
]

########################################################

puts "--"
print "Test mktdata_shm_index() mktdata_exch_by_shm_index()"
exch_arr.each_with_index { |ex, i|
  idx = mktdata_shm_index(ex)
  raise "#{ex} mktdata_shm_index #{idx} != #{i}" if idx != i
  ex_name = mktdata_exch_by_shm_index(idx)
  raise "mktdata_exch_by_shm_index #{idx} = #{ex_name} != #{ex}" if ex_name != ex
}
["XXXX", -1, {}].each { |ex|
  idx = mktdata_shm_index(ex)
  raise "#{ex} mktdata_shm_index #{idx} != -1" if idx != -1
  ex_name = mktdata_exch_by_shm_index(idx)
  raise "mktdata_exch_by_shm_index #{idx} = #{ex_name} != nil" if ex_name != nil
}
ex_name = mktdata_exch_by_shm_index(exch_arr.size)
raise "mktdata_exch_by_shm_index #{exch_arr.size} = #{ex_name} != nil" if ex_name != nil
print " √\n"

########################################################

puts "--"
exch_id = 0
puts "Make sure run: bnn_ws.c busd-btc before test"
puts "mktdata_pairs(#{exch_id}) #{mktdata_pairs(exch_id).join(', ')[0..79]}..."
pair = "USD-BTC"
depth = 2

puts "--"
puts "Test mktdata_odbk(#{exch_id}, #{pair}, depth)"
odbk = mktdata_odbk(exch_id, pair, depth)
raise "ODBK should not be null" if odbk.nil?
puts "#{mktdata_odbk(exch_id, pair, depth).inspect[0..79]}..."
print "Test mktdata_odbk(#{exch_id}, #{pair}, depth)"
print " √\n"


print "Test mktdata_odbk(#{exch_id}, #{pair}, depth) for 1M times"
start_t = Time.now.to_f
batch = 1000_000
batch.times {
  odbk = mktdata_odbk(exch_id, pair, depth)
  raise "ODBK should not be null" if odbk.nil?
}
end_t = Time.now.to_f
print " √\n"
puts "Cost seconds #{end_t - start_t}, single op #{(end_t-start_t)*1000_000/batch} us"

puts "--"
puts "Test mktdata_new_odbk(#{exch_id}, #{pair}, depth):"
puts mktdata_new_odbk(exch_id, pair, depth).inspect
print "Test mktdata_new_odbk(#{exch_id}, #{pair}, depth) for 1M times"
start_t = Time.now.to_f
batch = 1000_000
batch.times { mktdata_new_odbk(exch_id, 1, depth).inspect }
end_t = Time.now.to_f
print " √\n"
puts "Cost seconds #{end_t - start_t}, single op #{(end_t-start_t)*1000_000/batch} us"

puts "--"
print "Test mktdata_reg_sigusr1(#{exch_id}, #{pair})"
trap("SIGUSR1") {
  odbk = mktdata_new_odbk(exch_id, pair, depth)
  raise "ODBK from mktdata_new_odbk() should not be null" if odbk.nil?
  puts "SIGUSR1 #{pair} #{odbk.inspect[0..79]}..."
}
rv = mktdata_reg_sigusr1(exch_id, pair)
raise "mktdata_reg_sigusr1() = #{rv}" if rv != 0
print " √\n"

puts "--"

puts "Test mktdata_sigusr1_timedwait(), could change tasks of bnn_ws.c now"
puts "Signal timed wait:"
100.times { |i|
	t = Time.now.to_f
	sig = mktdata_sigusr1_timedwait(0.1)
	t = Time.now.to_f - t
  print "\r#{i} Sig #{sig} in #{t.round(9)}s"
  odbk = mktdata_new_odbk(exch_id, pair, depth)
  print "\t-> #{pair} #{odbk.inspect[0..39]}..."
	next if odbk.nil?
	bids, asks, t1, t2 = odbk
	raise "Too much data #{odbk}" if bids.size > depth || asks.size > depth
}
puts "\nSignal endless wait:"
100.times { |i|
	t = Time.now.to_f
	sig = mktdata_sigusr1_timedwait(nil)
	t = Time.now.to_f - t
  print "\r#{i} Sig #{sig} in #{t.round(9)}s"
  odbk = mktdata_new_odbk(exch_id, pair, depth)
  print "\t-> #{pair} #{odbk.inspect[0..39]}..."
  # Sometimes this happens, data changed but orderbook kept same at last.
	# raise "odbk is nil in no timed wait" if odbk.nil?
  next if odbk.nil?
	bids, asks, t1, t2 = odbk
	raise "Too much data #{odbk}" if bids.size > depth || asks.size > depth
}
puts "\nSignal endless wait then get new trades:"
last_trade_ts = 0
length = 3
100.times { |i|
	t = Time.now.to_f
	sig = mktdata_sigusr1_timedwait(nil)
	t = Time.now.to_f - t
  print "\r#{i} Sig #{sig} in #{t.round(9)}s"
  ticks = mktdata_new_ticks(exch_id, pair, length)
  next if ticks.nil?
  # from old to new
  print "\n"
  ticks.reverse.each { |trade|
    print "\t-> #{pair} #{trade.inspect}\n"
    p, s, type, ts_e3 = trade
    if ts_e3 < last_trade_ts
      raise "ts #{ts_e3} older than last #{last_trade_ts}"
    end
    last_trade_ts = ts_e3
    raise "type should only be 1/0" unless [0,1].include?(type)
  }
}
print " √\n"

puts "--"
puts "All passed"
puts "--"
