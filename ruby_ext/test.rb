require "urn_mktdata"
include URN_MKTDATA

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

puts "Make sure did: cb_ws.c usd-btc before below test"
5.times {
  puts mktdata_pairs(5).inspect
}

puts mktdata_odbk(5, 1, 9).inspect
