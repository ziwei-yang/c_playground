require "urn_mktdata"
include URN_MKTDATA

puts "--"
print "Test mktdata_shm_index() and mktdata_exch_by_shm_index()"
exch_arr = ["Binance", "BNCM", "BNUM", "Bybit", "BybitU", "Coinbase", "FTX"]
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

puts "--"
exch_id = 0
puts "Make sure did: bnn_ws.c usd-btc before below test"
puts "mktdata_pairs(#{exch_id}):"
puts mktdata_pairs(exch_id).inspect

puts "--"
puts "mktdata_odbk(#{exch_id}, 1, 9):"
puts mktdata_odbk(exch_id, 1, 9).inspect
puts "mktdata_odbk(#{exch_id}, 1, 9) for 100K times"
start_t = Time.now.to_f
batch = 100_000
batch.times {
  mktdata_odbk(exch_id, 1, 9).inspect
}
end_t = Time.now.to_f
puts "Cost seconds #{end_t - start_t}, single op #{(end_t-start_t)*1000_000/batch} us"

puts "--"
puts "mktdata_new_odbk(#{exch_id}, 1, 9):"
puts mktdata_new_odbk(exch_id, 1, 9).inspect
puts "mktdata_new_odbk(#{exch_id}, 1, 9) for 100K times"
start_t = Time.now.to_f
batch = 100_000
batch.times {
  mktdata_new_odbk(exch_id, 1, 9).inspect
}
end_t = Time.now.to_f
puts "Cost seconds #{end_t - start_t}, single op #{(end_t-start_t)*1000_000/batch} us"
puts "--"

trap("SIGUSR1") {
  puts mktdata_new_odbk(exch_id, 1, 9).inspect
}
rv = mktdata_reg_sigusr1(exch_id, 1)
puts "mktdata_reg_sigusr1() = #{rv}"
sleep 1

puts "--"

puts "Test mktdata_sigusr1_timedwait()"
10.times {
	t = Time.now.to_f
	sig = mktdata_sigusr1_timedwait(0.1)
	t = Time.now.to_f - t
	puts "Sig timed wait got #{sig}, #{t}s"
}
10.times {
	t = Time.now.to_f
	sig = mktdata_sigusr1_timedwait(nil)
	t = Time.now.to_f - t
	puts "Sig wait got #{sig}, #{t}s"
}

puts "--"
puts "--"
