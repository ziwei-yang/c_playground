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
puts "Make sure did: cb_ws.c usd-btc before below test"
puts "mktdata_pairs(5):"
puts mktdata_pairs(5).inspect

puts "--"
puts "mktdata_odbk(5, 1, 9):"
puts mktdata_odbk(5, 1, 9).inspect
puts "mktdata_odbk(5, 1, 9) for 100K times"
start_t = Time.now.to_f
batch = 100_000
batch.times {
  mktdata_odbk(5, 1, 9).inspect
}
end_t = Time.now.to_f
puts "Cost seconds #{end_t - start_t}, single op #{(end_t-start_t)*1000_000/batch} us"

puts "--"
puts "mktdata_new_odbk(5, 1, 9):"
puts mktdata_new_odbk(5, 1, 9).inspect
puts "mktdata_new_odbk(5, 1, 9) for 100K times"
start_t = Time.now.to_f
batch = 100_000
batch.times {
  mktdata_new_odbk(5, 1, 9).inspect
}
end_t = Time.now.to_f
puts "Cost seconds #{end_t - start_t}, single op #{(end_t-start_t)*1000_000/batch} us"

puts "--"
